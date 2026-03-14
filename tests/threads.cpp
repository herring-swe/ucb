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
        mutex = ucb_mutex_new();
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

static int worker_func(void* arg)
{
    FuncArg* fa = static_cast<FuncArg*>(arg);
    REQUIRE(fa->value == 42);
    ThreadFixture* st = fa->state;

    st->counter.fetch_add(1);
    ucb_mutex_lock(st->mutex);
    st->results.push_back(fa->value);
    ucb_mutex_unlock(st->mutex);
    return 0;
}

static void callback_func(void* arg, int status)
{
    FuncArg* fa = static_cast<FuncArg*>(arg);
    REQUIRE(fa->value == 42);
    REQUIRE(status == 0);
    ThreadFixture* st = fa->state;

    ucb_mutex_lock(st->mutex);
    st->results.push_back(99 * 100); // Mark exit values
    ucb_mutex_unlock(st->mutex);
}

static int stress_worker(void* arg)
{
    StressArgs* sa = static_cast<StressArgs*>(arg);
    for (int i = 0; i < sa->iters; i++)
    {
        sa->counter->fetch_add(1);
    }
    return 0;
}

static int dummy_worker(void* arg)
{
    UCB_UNUSED(arg);
    ucb_sleep_ms(100);
    return 0;
}

// --- Tests ---
TEST_CASE_FIXTURE(ThreadFixture, "thread basics")
{
    FuncArg fa      = {this, 42};

    ucb_task task = {0};
    task.func     = worker_func;
    task.arg      = reinterpret_cast<void*>(&fa);

    SUBCASE("Create/Free")
    {
        REQUIRE(th == nullptr);
        th = ucb_thread_new();
        REQUIRE_FALSE(ucb_thread_is_running(th));
        task.callback = callback_func;
        REQUIRE(ucb_thread_start(th, task) == true);
        // Thread may already have finished here...
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
        ucb_thread* dth = ucb_thread_new_detached();
        REQUIRE_FALSE(ucb_thread_is_joinable(dth));
        REQUIRE_FALSE(ucb_thread_is_running(dth));
        REQUIRE(ucb_thread_start(dth, task) == true);
        // Cannot touch dth after start
        // Wait for thread to finish
        int max_wait = 50; // timeout in ms
        while (counter.load() != 1 && --max_wait >= 0)
        {
            ucb_sleep_ms(1);
        }
        // We require the thread to finish in short time
        REQUIRE(max_wait >= 0);
        REQUIRE(counter.load() == 1);
        REQUIRE(results.size() == 1); // Worker only (no exit)
        REQUIRE(results[0] == 42);
    }

    SUBCASE("Stack Size")
    {
        size_t original_stack = ucb_thread_get_current_stack_size();

        th = ucb_thread_new();
        ucb_thread_set_stack_size(th, original_stack * 2);
        REQUIRE(ucb_thread_get_stack_size(th) >= original_stack * 2);
        REQUIRE(ucb_thread_start(th, task) == true);
        ucb_thread_join(th);
    }
}

TEST_CASE_FIXTURE(ThreadFixture, "thread priorities")
{
    FuncArg fa = {this, 42};

    ucb_task task = {0};
    task.func     = worker_func;
    task.arg      = reinterpret_cast<void*>(&fa);

    SUBCASE("Default Priority")
    {
        th = ucb_thread_new();
        ucb_thread_set_priority(th, UCB_THREAD_PRIO_DEFAULT);
        REQUIRE(ucb_thread_start(th, task) == true);
        ucb_thread_join(th);
    }

    SUBCASE("Custom Priority")
    {
        th = ucb_thread_new();
        ucb_thread_set_priority(th, UCB_THREAD_PRIO_HIGH);
        REQUIRE(ucb_thread_start(th, task) == true);
        ucb_thread_join(th);
    }
}

TEST_CASE_FIXTURE(ThreadFixture, "thread names")
{
    std::string in_name = "TestThread";
    std::string out_name;

    FuncArg fa = {this, 42};

    ucb_task task = {0};
    task.func     = worker_func;
    task.arg      = reinterpret_cast<void*>(&fa);

    th = ucb_thread_new();
    ucb_thread_set_name(th, in_name.c_str());
    out_name = ucb_thread_get_name(th);
    REQUIRE(out_name == in_name);
    REQUIRE(ucb_thread_start(th, task) == true);
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

    ucb_task task = {0};
    task.func     = stress_worker;
    task.arg      = reinterpret_cast<void*>(&args);

    for (int i = 0; i < N_THREADS; i++)
    {
        ucb_thread* t = ucb_thread_new();
        REQUIRE(ucb_thread_start(t, task) == true);
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
        CHECK_ABORTS(ucb_thread_set_name(nullptr, "fail"));
        CHECK_ABORTS(ucb_thread_set_priority(nullptr, UCB_THREAD_PRIO_HIGH));

        REQUIRE(num_aborts == 2);
        REQUIRE(num_error == 2);
        for (int i = 0; i < num_error; i++)
        {
            REQUIRE(errors[i].lvl == UCB_ERRLVL_USER);
            REQUIRE(errors[i].code == UCB_ERROR_INVALID_ARG);
        }
    }

    SUBCASE("Start")
    {
        th = ucb_thread_new();

        ucb_task task = {0};
        task.func     = dummy_worker;

        REQUIRE(ucb_thread_start(th, task) == true);
        CHECK_ABORTS(ucb_thread_start(th, task)); // Already running
        ucb_thread_join(th);

        REQUIRE(num_aborts == 1);
        REQUIRE(num_error == 1);
        REQUIRE(errors[0].code == UCB_ERROR_THREAD_BUSY);
        ucb_thread_free(th);
    }

    SUBCASE("Invalid Priority")
    {
        th = ucb_thread_new();
        CHECK_ABORTS(ucb_thread_set_priority(th, UCB_THREAD_PRIO_MIN - 1));
        REQUIRE(ucb_thread_get_priority(th) == UCB_THREAD_PRIO_DEFAULT);
        CHECK_ABORTS(ucb_thread_set_priority(th, UCB_THREAD_PRIO_MAX + 1));
        REQUIRE(ucb_thread_get_priority(th) == UCB_THREAD_PRIO_DEFAULT);

        REQUIRE(num_aborts == 2);
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

    ucb_task task = {0};
    task.func     = [](void*) { return 0; };
    task.arg      = nullptr;

    for (int i = 0; i < N; i++)
    {
        ucb_thread* th = ucb_thread_new();
        ucb_thread_start(th, task);
        ucb_thread_join(th);
        ucb_thread_free(th);
    }

    auto end    = std::chrono::high_resolution_clock::now();
    uint64_t ms = std::chrono::duration_cast<std::chrono::milliseconds>(end - start).count();
    printf("Created/joined %d threads in %" PRIu64 " ms (%.1f threads/sec)\n", N, ms,
           (N * 1000.0) / ms);
}
