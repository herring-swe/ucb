/**
 * @file mutex.h
 *
 * This file is part of the UCB project
 * - SPDX-FileCopyrightText: © 2026 Åke Svedin <ake@svedin.org>
 * - SPDX-License-Identifier: MIT
 *
 * @brief Cross-platform mutex
 */

#ifndef UCB_MUTEX_H
#define UCB_MUTEX_H

#include "error.h"
#include "export.h"

typedef struct ucb_mutex ucb_mutex;

UCB_API ucb_mutex* ucb_mutex_new();
UCB_API ucb_mutex* ucb_mutex_new_recursive();
UCB_API void ucb_mutex_free(ucb_mutex* mutex);

UCB_API bool ucb_mutex_is_recursive(const ucb_mutex* mutex);

UCB_API void ucb_mutex_lock(ucb_mutex* mutex);
UCB_API bool ucb_mutex_trylock(ucb_mutex* mutex);
UCB_API void ucb_mutex_unlock(ucb_mutex* mutex);

#endif // UCB_MUTEX_H
