/**
 * @file cond_private.h
 * This file is part of the UCB project
 * - SPDX-FileCopyrightText: © 2026 Åke Svedin <ake@svedin.org>
 * - SPDX-License-Identifier: MIT
 *
 * @brief Condition private header
 */

#ifndef UCB_COND_PRIVATE_H
#define UCB_COND_PRIVATE_H

#include "ucb/cond.h"

#if defined(_WIN32)
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <Windows.h>
#else
#include <pthread.h>
#endif

/**
 * Platform-specific layout placed inside the opaque public ucb_cond storage.
 */
struct ucb_cond_impl
{
#if defined(_WIN32)
    CONDITION_VARIABLE handle;
#else
    pthread_cond_t handle;
#endif
};

#define UCB_COND_IMPL(cond) ((struct ucb_cond_impl*)(void*)(cond))

#endif // UCB_COND_PRIVATE_H
