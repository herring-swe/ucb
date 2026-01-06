/**
 * This file is part of the UCB project
 * SPDX-FileCopyrightText: © 2025 Åke Svedin <ake@svedin.org>
 * SPDX-License-Identifier: MIT
 * 
 * @brief 
 */

#ifndef UCB_MUTEX_PRIVATE_H
#define UCB_MUTEX_PRIVATE_H

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
    int flags;
};

#endif // UCB_MUTEX_PRIVATE_H
