/**
 * @file cond.h
 * This file is part of the UCB project
 * - SPDX-FileCopyrightText: © 2026 Åke Svedin <ake@svedin.org>
 * - SPDX-License-Identifier: MIT
 *
 * @brief Cross platform thread condition
 */

#ifndef UCB_COND_H
#define UCB_COND_H

#include "export.h"

#include <stdbool.h>

// Forward declare
struct ucb_mutex;

typedef struct ucb_cond ucb_cond;

UCB_API ucb_cond* ucb_cond_new();
UCB_API void ucb_cond_free(ucb_cond* cond);

UCB_API void ucb_cond_signal(ucb_cond* cond);
UCB_API void ucb_cond_broadcast(ucb_cond* cond);

/**
 * @brief Wait for a condition to be signalled
 * The mutex must be locked by the current thread before this call.
 * The mutex will be unlocked while waiting and locked again before returning.
 * @param cond the condition
 * @param mutex the mutex
 * @return true if the condition was signalled, false on error
 */
UCB_API bool ucb_cond_wait(ucb_cond* cond, struct ucb_mutex* mutex);

#endif // UCB_COND_H
