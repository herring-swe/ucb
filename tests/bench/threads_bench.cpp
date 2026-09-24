/**
 * @file threads_bench.cpp
 * This file is part of the UCB project
 * - SPDX-FileCopyrightText: © 2026 Åke Svedin <ake@svedin.org>
 * - SPDX-License-Identifier: MIT
 *
 * @brief Benchmarks for thread creation and join with std::thread as a reference.
 */

#include "threads_bench.h"

#include "microbench.h"

#include "ucb/threads.h"

#include <cstdint>
#include <thread>

namespace {

volatile std::int64_t g_sink = 0;

void sink(std::int64_t value)
{
    g_sink = g_sink + value;
}

int thread_noop(void*)
{
    return 0;
}

void thread_create_join()
{
    ucb_thread* th = ucb_thread_new();
    ucb_task task = {0};
    task.func = thread_noop;
    ucb_thread_start(th, task);
    ucb_thread_join(th);
    ucb_thread_free(th);
    sink(1);
}

void std_thread_create_join()
{
    std::thread th([] {});
    th.join();
    sink(1);
}

} // namespace

void register_thread_benchmarks(ucb::microbench::benchmark& bench)
{
    ucb::microbench::suite& suite = bench.add_suite("threads", "Create and join a thread");

    ucb::microbench::test& create = suite.add_test("create-join",
                                                   "Run a no-op task on a new thread and join it",
                                                   thread_create_join);
    create.add_ref(std_thread_create_join, "std-thread");
}
