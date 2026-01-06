/**
 * This file is part of the UCB project
 * SPDX-FileCopyrightText: © 2025 Åke Svedin <ake@svedin.org>
 * SPDX-License-Identifier: MIT
 * 
 * @brief Cross-platform mutex
 */

#ifndef UCB_MUTEX_H
#define UCB_MUTEX_H

#include "export.h"
#include "error.h"

#define UCB_MUTEX_DEFAULT   0x00
#define UCB_MUTEX_RECURSIVE 0x01

typedef struct ucb_mutex ucb_mutex;

UCB_API ucb_mutex* ucb_mutex_new(int flags);
UCB_API void ucb_mutex_init(ucb_mutex* mutex, int flags);
UCB_API void ucb_mutex_release(ucb_mutex* mutex);
UCB_API void ucb_mutex_free(ucb_mutex* mutex);

UCB_API void ucb_mutex_lock(ucb_mutex* mutex);
UCB_API bool ucb_mutex_trylock(ucb_mutex* mutex);
UCB_API void ucb_mutex_unlock(ucb_mutex* mutex);

#endif // UCB_MUTEX_H
