/**
 * @file test_threads.c
 *
 * This file is part of the UCB project
 * - SPDX-FileCopyrightText: © 2026 Åke Svedin <ake@svedin.org>
 * - SPDX-License-Identifier: MIT
 *
 * @brief C threads API testing implementation
 *
 * Verifies that the public mutex/cond/once API is usable from plain C,
 * including stack initialization.
 */

#include "test_threads.h"

#include "ucb/cond.h"
#include "ucb/mutex.h"
#include "ucb/once.h"

static int s_once_calls = 0;

static void c_once_func(void)
{
    s_once_calls++;
}

int test_c_threads(void)
{
    // Standard mutex on the stack
    ucb_mutex mutex;
    if (!ucb_mutex_init(&mutex))
        return 0;
    if (ucb_mutex_is_recursive(&mutex))
        return 0;

    ucb_mutex_lock(&mutex);
    // A standard mutex locked by this thread cannot be try-locked again
    if (ucb_mutex_trylock(&mutex))
    {
        ucb_mutex_unlock(&mutex);
        return 0;
    }
    ucb_mutex_unlock(&mutex);
    ucb_mutex_release(&mutex);

    // Recursive mutex on the stack
    ucb_mutex rmutex;
    if (!ucb_mutex_init_recursive(&rmutex))
        return 0;
    if (!ucb_mutex_is_recursive(&rmutex))
        return 0;
    ucb_mutex_lock(&rmutex);
    if (!ucb_mutex_trylock(&rmutex))
        return 0;
    ucb_mutex_unlock(&rmutex);
    ucb_mutex_unlock(&rmutex);
    ucb_mutex_release(&rmutex);

    // Condition on the stack
    ucb_cond cond;
    if (!ucb_cond_init(&cond))
        return 0;
    ucb_cond_release(&cond);

    // Once, statically initialized
    ucb_once once = UCB_ONCE_INIT;
    ucb_once_run(&once, c_once_func);
    ucb_once_run(&once, c_once_func);
    if (s_once_calls != 1)
        return 0;

    // Once, initiated at runtime
    ucb_once once2;
    ucb_once_init(&once2);
    ucb_once_run(&once2, c_once_func);
    if (s_once_calls != 2)
        return 0;

    return 1;
}
