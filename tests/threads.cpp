/**
 * @file threads.cpp
 * 
 * This file is part of the UCB project
 * - SPDX-FileCopyrightText: © 2026 Åke Svedin <ake@svedin.org>
 * - SPDX-License-Identifier: MIT
 * 
 * @brief threads tests
 */

#include "doctest.h"

#include "common.h"
#include "ucb/memory.h"
#include "ucb/mutex.h"
#include "ucb/threads.h"
#include "ucb/time.h"

#include <atomic>
#include <chrono>
#include <cinttypes>
#include <string>
#include <thread>
#include <vector>

// --- Fixtures ---
struct ThreadFixture
{
    ucb_thread* th;
    std::atomic<int> counter{0};
    std::vector<int> results;
    ucb_mutex* mutex;

    ThreadFixture()
    {
        mutex = ucb_mutex_new(UCB_MUTEX_DEFAULT);
        th    = nullptr;
    }

    ~ThreadFixture()
    {
        if (th)
            ucb_thread_free(th);
        ucb_mutex_free(mutex);
    }
};

struct FuncArg
{
    ThreadFixture* state;
    int value;
};

struct StressArgs
{
    std::atomic<int>* counter;
    int iters;
};

static void worker_func(void* arg)
{
    FuncArg* fa = static_cast<FuncArg*>(arg);
    REQUIRE(fa->value == 42);
    ThreadFixture* st = fa->state;

    st->counter.fetch_add(1);
    ucb_mutex_lock(st->mutex);
    st->results.push_back(fa->value);
    ucb_mutex_unlock(st->mutex);
}

static void exit_func(void* exit_arg, void* func_arg)
{
    FuncArg* fa = static_cast<FuncArg*>(func_arg);
    REQUIRE(fa->value == 42);
    FuncArg* fa_exit = static_cast<FuncArg*>(exit_arg);
    REQUIRE(fa_exit->value == 99);
    ThreadFixture* st = fa_exit->state;

    ucb_mutex_lock(st->mutex);
    st->results.push_back(fa_exit->value * 100); // Mark exit values
    ucb_mutex_unlock(st->mutex);
}

static void stress_worker(void* arg)
{
    StressArgs* sa = static_cast<StressArgs*>(arg);
    for (int i = 0; i < sa->iters; i++)
    {
        sa->counter->fetch_add(1);
    }
}

static void dummy_worker(void* arg)
{
    UCB_UNUSED(arg);
    ucb_sleep_ms(100);
}

// --- Tests ---
TEST_CASE_FIXTURE(ThreadFixture, "thread basics")
{
    FuncArg fa      = {this, 42};
    FuncArg fa_exit = {this, 99};

    SUBCASE("Create/Free")
    {
        REQUIRE(th == nullptr);
        th = ucb_thread_new(UCB_THREAD_FLAG_DEFAULT);
        REQUIRE_FALSE(ucb_thread_is_running(th));
        ucb_thread_set_func(th, worker_func, reinterpret_cast<void*>(&fa));
        ucb_thread_set_exit_func(th, exit_func, reinterpret_cast<void*>(&fa_exit));
        REQUIRE(ucb_thread_start(th) == true);
        REQUIRE(ucb_thread_is_running(th));
        REQUIRE(ucb_thread_is_joinable(th));
        ucb_thread_join(th);
        REQUIRE_FALSE(ucb_thread_is_running(th));
        REQUIRE(counter.load() == 1);
        REQUIRE(results.size() == 2); // Worker + exit
        REQUIRE(results[0] == 42);
        REQUIRE(results[1] == 9900); // Exit func marks values
    }

    SUBCASE("Detached Thread")
    {
        th = ucb_thread_new(UCB_THREAD_FLAG_DETACHED);
        ucb_thread_set_func(th, worker_func, reinterpret_cast<void*>(&fa));
        REQUIRE(ucb_thread_start(th) == true);
        REQUIRE(ucb_thread_is_running(th));
        // Cannot join detached threads
        REQUIRE_FALSE(ucb_thread_is_joinable(th));
        std::this_thread::sleep_for(std::chrono::milliseconds(10)); // Yield
        REQUIRE_FALSE(ucb_thread_is_running(th));                   // Detached threads auto-cleanup
    }

    SUBCASE("Stack Size")
    {
        size_t original_stack = ucb_thread_get_current_stack_size();

        th = ucb_thread_new(UCB_THREAD_FLAG_DEFAULT);
        ucb_thread_set_stack_size(th, original_stack * 2);
        ucb_thread_set_func(th, worker_func, reinterpret_cast<void*>(&fa));
        REQUIRE(ucb_thread_get_stack_size(th) >= original_stack * 2);
        REQUIRE(ucb_thread_start(th) == true);
        ucb_thread_join(th);
    }
}

TEST_CASE_FIXTURE(ThreadFixture, "thread priorities")
{
    FuncArg fa = {this, 42};

    SUBCASE("Default Priority")
    {
        th = ucb_thread_new(UCB_THREAD_FLAG_DEFAULT);
        ucb_thread_set_priority(th, UCB_THREAD_PRIO_DEFAULT);
        ucb_thread_set_func(th, worker_func, reinterpret_cast<void*>(&fa));
        REQUIRE(ucb_thread_start(th) == true);
        ucb_thread_join(th);
    }

    SUBCASE("Custom Priority")
    {
        th = ucb_thread_new(UCB_THREAD_FLAG_DEFAULT);
        ucb_thread_set_priority(th, UCB_THREAD_PRIO_HIGH);
        ucb_thread_set_func(th, worker_func, reinterpret_cast<void*>(&fa));
        REQUIRE(ucb_thread_start(th) == true);
        ucb_thread_join(th);
    }
}

TEST_CASE_FIXTURE(ThreadFixture, "thread names")
{
    std::string in_name = "TestThread";
    std::string out_name;

    FuncArg fa = {this, 42};

    th = ucb_thread_new(UCB_THREAD_FLAG_DEFAULT);
    ucb_thread_set_name(th, in_name.c_str());
    out_name = ucb_thread_get_name(th);
    REQUIRE(out_name == in_name);
    ucb_thread_set_func(th, worker_func, reinterpret_cast<void*>(&fa));
    REQUIRE(ucb_thread_start(th) == true);
    ucb_thread_join(th);
    // Name verification requires platform-specific APIs (omitted for brevity)
}

TEST_CASE_FIXTURE(ThreadFixture, "thread stress test")
{
    constexpr int N_THREADS = 100;
    constexpr int N_ITERS   = 1000;
    std::vector<ucb_thread*> threads;
    std::atomic<int> shared_counter{0};

    struct StressArgs
    {
        std::atomic<int>* counter;
        int iters;
    };
    StressArgs args{&shared_counter, N_ITERS};

    for (int i = 0; i < N_THREADS; i++)
    {
        ucb_thread* t = ucb_thread_new(UCB_THREAD_FLAG_JOINABLE);
        ucb_thread_set_func(t, stress_worker, reinterpret_cast<void*>(&args));
        REQUIRE(ucb_thread_start(t) == true);
        threads.push_back(t);
    }

    for (auto t : threads)
    {
        ucb_thread_join(t);
        ucb_thread_free(t);
    }

    REQUIRE(shared_counter.load() == N_THREADS * N_ITERS);
}

TEST_CASE_FIXTURE(TestFailureFixture, "thread error handling")
{
    ucb_thread* th = nullptr;

    SUBCASE("Null Arguments")
    {
        REQUIRE(ucb_thread_new(UCB_THREAD_FLAG_JOINABLE | UCB_THREAD_FLAG_DETACHED) == nullptr);
        ucb_thread_set_name(nullptr, "fail");
        ucb_thread_set_priority(nullptr, UCB_THREAD_PRIO_HIGH);

        REQUIRE(num_error == 3);
        for (int i = 0; i < num_error; i++)
        {
            REQUIRE(errors[i].lvl == UCB_ERRLVL_USER);
            REQUIRE(errors[i].code == UCB_ERROR_INVALID_ARG);
        }
    }

    SUBCASE("Start")
    {
        th = ucb_thread_new(UCB_THREAD_FLAG_JOINABLE);
        // No function set
        REQUIRE(ucb_thread_start(th) == false);
        ucb_thread_set_func(th, dummy_worker, nullptr);

        REQUIRE(ucb_thread_start(th) == true);
        REQUIRE(ucb_thread_start(th) == false); // Already running
        ucb_thread_join(th);

        REQUIRE(num_error == 2);
        REQUIRE(errors[0].code == UCB_ERROR_INVALID_STATE);
        REQUIRE(errors[1].code == UCB_ERROR_THREAD_BUSY);
        ucb_thread_free(th);
    }

    SUBCASE("Invalid Priority")
    {
        th = ucb_thread_new(UCB_THREAD_FLAG_DEFAULT);
        ucb_thread_set_priority(th, UCB_THREAD_PRIO_MIN - 1);
        REQUIRE(ucb_thread_get_priority(th) == UCB_THREAD_PRIO_DEFAULT);
        ucb_thread_set_priority(th, UCB_THREAD_PRIO_MAX + 1);
        REQUIRE(ucb_thread_get_priority(th) == UCB_THREAD_PRIO_DEFAULT);

        REQUIRE(num_error == 2);
        REQUIRE(errors[0].code == UCB_ERROR_INVALID_ARG);
        REQUIRE(errors[1].code == UCB_ERROR_INVALID_ARG);
        ucb_thread_free(th);
    }
}

// --- Performance Benchmark ---
TEST_CASE("benchmark threads" * doctest::test_suite("benchmark") * doctest::skip())
{
    const int N = 1000;
    auto start  = std::chrono::high_resolution_clock::now();

    for (int i = 0; i < N; i++)
    {
        ucb_thread* th = ucb_thread_new(UCB_THREAD_FLAG_JOINABLE);
        ucb_thread_set_func(th, [](void*) {}, nullptr);
        ucb_thread_start(th);
        ucb_thread_join(th);
        ucb_thread_free(th);
    }

    auto end    = std::chrono::high_resolution_clock::now();
    uint64_t ms = std::chrono::duration_cast<std::chrono::milliseconds>(end - start).count();
    printf("Created/joined %d threads in %" PRIu64 " ms (%.1f threads/sec)\n", N, ms,
           (N * 1000.0) / ms);
}
