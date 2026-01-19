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
#define WIN32_LEAN_AND_MEAN
#include <Windows.h>
#else
#include <pthread.h>
#endif

#include <stdbool.h>

struct ucb_cond
{
#if defined(_WIN32)
    CONDITION_VARIABLE handle;
#else
    pthread_cond_t handle;
#endif
};

bool ucb_cond_init(ucb_cond* cond);
bool ucb_cond_release(ucb_cond* cond);

#endif // UCB_COND_PRIVATE_H
