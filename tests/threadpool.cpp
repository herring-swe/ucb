/**
 * @file threadpool.cpp
 *
 * This file is part of the UCB project
 * - SPDX-FileCopyrightText: © 2026 Åke Svedin <ake@svedin.org>
 * - SPDX-License-Identifier: MIT
 *
 * @brief threadpool tests
 */

#include "doctest.h"

#include "common.h"
#include "ucb/cond.h"
#include "ucb/memory.h"
#include "ucb/mutex.h"
#include "ucb/threadpool.h"
#include "ucb/time.h"

#include <atomic>
#include <thread>
#include <vector>

// --- Fixtures ---
struct ThreadPoolFixture
{
    ucb_threadpool* pool;
    std::atomic<int> counter{0};
    std::vector<int> results;
    ucb_mutex* mutex;

    ThreadPoolFixture(size_t num_threads = 4)
    {
        mutex = ucb_mutex_new();
        pool  = ucb_threadpool_new(num_threads);
        REQUIRE(pool != nullptr);
    }

    ~ThreadPoolFixture()
    {
        ucb_threadpool_free(pool);
        ucb_mutex_free(mutex);
    }
};

struct func_arg
{
    ThreadPoolFixture* fixture;
    int value;
};

static int worker_func(void* arg)
{
    func_arg* fa          = reinterpret_cast<func_arg*>(arg);
    ThreadPoolFixture* ft = fa->fixture;

    ft->counter.fetch_add(1);
    ucb_mutex_lock(ft->mutex);
    ft->results.push_back(fa->value);
    ucb_mutex_unlock(ft->mutex);

    return 0;
}

static void callback_func(void* arg, int status)
{
    UCB_UNUSED(status);
    func_arg* fa          = reinterpret_cast<func_arg*>(arg);
    ThreadPoolFixture* ft = fa->fixture;

    ucb_mutex_lock(ft->mutex);
    ft->results.push_back(fa->value * 100); // Mark callback results
    ucb_mutex_unlock(ft->mutex);
}

// --- Tests ---
TEST_CASE_FIXTURE(ThreadPoolFixture, "threadpool basics")
{
    SUBCASE("Create/Free")
    {
        REQUIRE(pool != nullptr);
        REQUIRE(ucb_threadpool_num_running(pool) == 0);
    }

    SUBCASE("Add Task")
    {
        func_arg farg = {this, 42};

        ucb_task task = ucb_task_make(worker_func);
        task.arg      = &farg;
        task.callback = callback_func;

        REQUIRE(ucb_threadpool_add_task(pool, &task) == true);
        ucb_threadpool_start(pool);
        ucb_threadpool_wait_all(pool);
        REQUIRE(counter.load() == 1);
        REQUIRE(results.size() == 2); // Worker + callback
        REQUIRE(results[0] == 42);
        REQUIRE(results[1] == 4200); // Callback marks values
    }

    SUBCASE("Add Multiple Tasks")
    {
        const int N_TASKS = 100;

        // Keep alive for the duration of the test
        std::vector<func_arg> fargs;
        fargs.reserve(N_TASKS);

        ucb_threadpool_start(pool);

        for (int i = 0; i < N_TASKS; i++)
        {
            fargs.push_back({this, i});

            ucb_task task = {0};
            task.func     = worker_func;
            task.arg      = &fargs[i];
            task.callback = callback_func;

            REQUIRE(ucb_threadpool_add_task(pool, &task) == true);
        }
        ucb_threadpool_wait_all(pool);
        REQUIRE(counter.load() == N_TASKS);
        REQUIRE(results.size() == N_TASKS * 2); // Worker + callback for each task
    }

    SUBCASE("Default Priority")
    {
        func_arg farg = {this, 42};

        ucb_task task = {0};
        task.func     = worker_func;
        task.arg      = &farg;
        task.priority = UCB_TASK_PRIO_NORMAL;

        REQUIRE(ucb_threadpool_add_task(pool, &task) == true);
        ucb_threadpool_start(pool);
        ucb_threadpool_wait_all(pool);
        REQUIRE(counter.load() == 1);
    }

    SUBCASE("High Priority")
    {
        constexpr int low   = UCB_THREAD_PRIO_MIN;
        constexpr int high  = UCB_THREAD_PRIO_MAX;
        constexpr int range = high - low + 1;

        std::vector<func_arg> fargs;
        fargs.reserve(range);

        ucb_threadpool_free(pool);
        // Force one thread to ensure priority order.
        pool = ucb_threadpool_new(1);

        for (int prio = low, i = 0; prio <= high; prio++, i++)
        {
            fargs.push_back({this, prio});

            ucb_task task = {0};
            task.func     = worker_func;
            task.arg      = &fargs[i];
            task.priority = prio;

            REQUIRE(ucb_threadpool_add_task(pool, &task) == true);

            // It will ignore multiple start attempts
            ucb_threadpool_start(pool);
        }
        ucb_threadpool_wait_all(pool);

        REQUIRE(counter.load() == range);

        for (int prio = high, i = 0; prio >= low; prio--, i++)
        {
            REQUIRE(results[i] == prio);
        }
    }

    SUBCASE("Querying")
    {
        ucb_cond* cond = ucb_cond_new();
        ucb_mutex* mtx = ucb_mutex_new();

        struct farg
        {
            ucb_cond* cond;
            ucb_mutex* mtx;
        };

        farg arg = {cond, mtx};

        ucb_task task = {0};
        task.func     = [](void* arg) -> int {
            farg* f = reinterpret_cast<farg*>(arg);
            // Wait for the main thread to signal us to continue
            ucb_mutex_lock(f->mtx);
            ucb_cond_wait(f->cond, f->mtx);
            ucb_mutex_unlock(f->mtx);
            return 0;
        };
        task.arg = &arg;

        REQUIRE(ucb_threadpool_add_task(pool, &task) == true);
        REQUIRE(ucb_threadpool_num_running(pool) == 0);
        REQUIRE(ucb_threadpool_num_queued(pool) == 1);

        // Start and give it time to wait
        ucb_threadpool_start(pool);
        ucb_sleep_ms(10);

        // Thread is running, verify it's state
        REQUIRE(ucb_threadpool_num_running(pool) == 1);
        REQUIRE(ucb_threadpool_num_queued(pool) == 0);

        // Signal for it to finish and wait
        ucb_cond_signal(cond);
        // Safe to use join, since task is known to be started and no new tasks
        ucb_threadpool_join(pool);

        REQUIRE(ucb_threadpool_num_running(pool) == 0);
        REQUIRE(ucb_threadpool_num_queued(pool) == 0);

        ucb_cond_free(cond);
        ucb_mutex_free(mtx);
    }
}

// TEST_CASE_FIXTURE(ThreadPoolFixture, "threadpool Stress Test")
// {
//     constexpr int num_tasks = 1000;
//     constexpr int num_add   = 10;
//     std::atomic<int> shared_counter{0};

//     auto stress_worker = [](void* arg) {
//         std::atomic<int>* counter = static_cast<std::atomic<int>*>(arg);
//         for (int i = 0; i < num_add; i++)
//         {
//             counter->fetch_add(1);
//         }
//         return 0;
//     };

//     std::vector<func_arg> fargs;

//     for (int i = 0; i < num_tasks; i++)
//     {
//         ucb_task task;
//         task.func = stress_worker;
//         task.arg  = &shared_counter;

//         REQUIRE(ucb_threadpool_add_task(pool, &task) == true);
//     }

//     ucb_threadpool_wait_all(pool);
//     REQUIRE(shared_counter.load() == num_tasks * num_add);
// }

TEST_CASE_FIXTURE(TestFailureFixture, "threadpool error handling")
{
    SUBCASE("Null Arguments")
    {
        CHECK_ABORTS(ucb_threadpool_new(0));
        CHECK_ABORTS(ucb_threadpool_add_task(nullptr, nullptr));

        REQUIRE(num_aborts == 2);
        REQUIRE(num_error == 2);
        for (int i = 0; i < num_error; i++)
        {
            REQUIRE(errors[i].lvl == UCB_ERRLVL_USER);
            REQUIRE(errors[i].code == UCB_ERROR_INVALID_ARG);
        }
    }

    SUBCASE("Invalid Task")
    {
        ucb_task invalid_task = {0};
        ucb_threadpool* pool  = ucb_threadpool_new(1);
        CHECK_ABORTS(ucb_threadpool_add_task(pool, &invalid_task));
        ucb_threadpool_free(pool);

        REQUIRE(num_error == 1);
        REQUIRE(num_aborts == 1);
        REQUIRE(errors[0].lvl == UCB_ERRLVL_USER);
        REQUIRE(errors[0].code == UCB_ERROR_INVALID_ARG);
    }
}

// // --- Performance Benchmark ---
// TEST_CASE("ThreadPool Benchmark")
// {
//     const int N_TASKS    = 10000;
//     ucb_threadpool* pool = ucb_threadpool_create(8);
//     REQUIRE(pool != nullptr);

//     std::atomic<int> shared_counter{0};
//     auto start = std::chrono::high_resolution_clock::now();

//     for (int i = 0; i < N_TASKS; i++)
//     {
//         ucb_task task = {
//             .func =
//                 [](void* arg) {
//                     std::atomic<int>* counter = static_cast<std::atomic<int>*>(arg);
//                     counter->fetch_add(1);
//                     return 0;
//                 },
//             .arg = &shared_counter,
//         };
//         ucb_threadpool_add_task(pool, &task);
//     }

//     ucb_threadpool_wait_all(pool);
//     auto end = std::chrono::high_resolution_clock::now();
//     auto ms  = std::chrono::duration_cast<std::chrono::milliseconds>(end - start).count();
//     printf("Executed %d tasks in %d ms (%.1f tasks/sec)\n", N_TASKS, ms, (N_TASKS * 1000.0) /
//     ms);

//     REQUIRE(shared_counter.load() == N_TASKS);
//     ucb_threadpool_free(pool);
// }
