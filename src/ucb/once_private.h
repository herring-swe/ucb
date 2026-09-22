/**
 * @file once_private.h
 * This file is part of the UCB project
 * - SPDX-FileCopyrightText: © 2026 Åke Svedin <ake@svedin.org>
 * - SPDX-License-Identifier: MIT
 *
 * @brief One-time initialization private header
 */

#ifndef UCB_ONCE_PRIVATE_H
#define UCB_ONCE_PRIVATE_H

#include "ucb/once.h"

#if defined(_WIN32)
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <Windows.h>
#else
#include <pthread.h>
#endif

/**
 * Platform-specific layout placed inside the opaque public ucb_once storage.
 */
struct ucb_once_impl
{
#if defined(_WIN32)
    INIT_ONCE handle;
#else
    pthread_once_t handle;
#endif
};

#define UCB_ONCE_IMPL(once) ((struct ucb_once_impl*)(void*)(once))

#endif // UCB_ONCE_PRIVATE_H
