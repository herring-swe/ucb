/**
 * @file mutex_private.h
 *
 * This file is part of the UCB project
 * - SPDX-FileCopyrightText: © 2026 Åke Svedin <ake@svedin.org>
 * - SPDX-License-Identifier: MIT
 *
 * @brief Private mutex implementation layout
 */

#ifndef UCB_MUTEX_PRIVATE_H
#define UCB_MUTEX_PRIVATE_H

#include "ucb/mutex.h"
#include "ucb/types.h"

#if defined(_WIN32)
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <Windows.h>
#else
#include <pthread.h>
#include <unistd.h>
#endif

/**
 * Platform-specific layout placed inside the opaque public ucb_mutex storage.
 */
struct ucb_mutex_impl
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

#define UCB_MUTEX_IMPL(mutex) ((struct ucb_mutex_impl*)(void*)(mutex))
#define UCB_MUTEX_IMPL_CONST(mutex) ((const struct ucb_mutex_impl*)(const void*)(mutex))

#endif // UCB_MUTEX_PRIVATE_H
