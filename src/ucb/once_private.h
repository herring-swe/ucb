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
#define WIN32_LEAN_AND_MEAN
#include <Windows.h>
#else
#include <pthread.h>
#endif

struct ucb_once
{
#if defined(_WIN32)
    INIT_ONCE handle;
#else
    pthread_once_t handle;
#endif
};

#if defined(_WIN32)
#define UCB_ONCE_INIT {INIT_ONCE_STATIC_INIT}
#else
#define UCB_ONCE_INIT {PTHREAD_ONCE_INIT}
#endif

void ucb_once_init_common(ucb_once* once);

#endif // UCB_ONCE_PRIVATE_H
