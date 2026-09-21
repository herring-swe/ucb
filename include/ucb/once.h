/**
 * @file once.h
 * This file is part of the UCB project
 * - SPDX-FileCopyrightText: © 2026 Åke Svedin <ake@svedin.org>
 * - SPDX-License-Identifier: MIT
 *
 * @brief Cross platform one-time initialization
 */

#ifndef UCB_ONCE_H
#define UCB_ONCE_H

#include <ucb/export.h>

typedef struct ucb_once ucb_once;

/**
 * @brief Allocates and initiates a new one-time object
 * @return the new object or UCB_NULL on failure
 */
UCB_API ucb_once* ucb_once_new(void);

/**
 * @brief Free a one-time object
 * @param once the object, may be UCB_NULL
 */
UCB_API void ucb_once_free(ucb_once* once);

/**
 * @brief Run a function exactly once
 *
 * The first call runs @p func. Any later call, from any thread, is a no-op.
 * The call is thread-safe and blocks concurrent callers until @p func has
 * completed.
 *
 * @warning @p func must not call @ref ucb_once_run on the same object, and must
 * not use longjmp or otherwise leave the call abnormally.
 *
 * @param once the one-time object
 * @param func the function to run once
 */
UCB_API void ucb_once_run(ucb_once* once, void (*func)(void));

#endif // UCB_ONCE_H
