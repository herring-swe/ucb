/**
 * @file mutex_private.h
 *
 * This file is part of the UCB project
 * - SPDX-FileCopyrightText: © 2026 Åke Svedin <ake@svedin.org>
 * - SPDX-License-Identifier: MIT
 *
 * @brief Private mutex type
 */

#ifndef UCB_MUTEX_PRIVATE_H
#define UCB_MUTEX_PRIVATE_H

#include "ucb/mutex.h"

#include "ucb/types.h"

#if defined(_WIN32)
#define WIN32_LEAN_AND_MEAN
#include <Windows.h>
#else
#include <pthread.h>
#include <unistd.h>
#endif

struct ucb_mutex
{
#if defined(_WIN32)
    CRITICAL_SECTION handle;
    ucb_pid owner;
    int count;
#else
    pthread_mutex_t handle;
#endif
    bool recursive;
};

void ucb_mutex_init(ucb_mutex* mutex);
void ucb_mutex_init_recursive(ucb_mutex* mutex);
void ucb_mutex_release(ucb_mutex* mutex);

#endif // UCB_MUTEX_PRIVATE_H
