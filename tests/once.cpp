/**
 * @file once.cpp
 *
 * This file is part of the UCB project
 * - SPDX-FileCopyrightText: © 2026 Åke Svedin <ake@svedin.org>
 * - SPDX-License-Identifier: MIT
 *
 * @brief once tests
 */

#include "ucb/once.h"

#include "common.h"

#include "ucb/memory.h"
#include "ucb/threads.h"

#include <doctest.h>

#include <atomic>
#include <vector>

static std::atomic<int> g_once_calls{0};

static void once_inc(void)
{
    g_once_calls.fetch_add(1);
}

struct OnceArg
{
    ucb_once* once;
};

static int once_worker(void* arg)
{
    OnceArg* a = static_cast<OnceArg*>(arg);
    ucb_once_run(a->once, once_inc);
    return 0;
}

TEST_CASE("once")
{
    SUBCASE("stack static init")
    {
        g_once_calls = 0;
        ucb_once once = UCB_ONCE_INIT;
        ucb_once_run(&once, once_inc);
        ucb_once_run(&once, once_inc);
        REQUIRE(g_once_calls.load() == 1);
    }

    SUBCASE("runtime init")
    {
        g_once_calls = 0;
        ucb_once once;
        ucb_once_init(&once);
        ucb_once_run(&once, once_inc);
        ucb_once_run(&once, once_inc);
        REQUIRE(g_once_calls.load() == 1);
    }

    SUBCASE("heap")
    {
        g_once_calls = 0;
        ucb_once* once = ucb_once_new();
        REQUIRE(once != nullptr);
        ucb_once_run(once, once_inc);
        ucb_once_run(once, once_inc);
        REQUIRE(g_once_calls.load() == 1);
        ucb_once_free(once);
    }

    SUBCASE("free null is safe")
    {
        ucb_once_free(nullptr);
    }

    SUBCASE("concurrent")
    {
        constexpr int N = 8;
        g_once_calls = 0;

        ucb_once once = UCB_ONCE_INIT;
        OnceArg arg = {&once};

        std::vector<ucb_thread*> threads;
        for (int i = 0; i < N; i++)
        {
            ucb_thread* th = ucb_thread_new();
            REQUIRE(th != nullptr);
            ucb_task task = ucb_task_make(once_worker);
            task.arg = &arg;
            REQUIRE(ucb_thread_start(th, task));
            threads.push_back(th);
        }

        for (ucb_thread* th : threads)
        {
            ucb_thread_join(th);
            ucb_thread_free(th);
        }

        REQUIRE(g_once_calls.load() == 1);
    }
}
