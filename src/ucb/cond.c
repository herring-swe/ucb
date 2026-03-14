/**
 * @file cond.c
 * This file is part of the UCB project
 * - SPDX-FileCopyrightText: © 2026 Åke Svedin <ake@svedin.org>
 * - SPDX-License-Identifier: MIT
 *
 * @brief Cross platform thread condition implementation
 */

#include "cond_private.h"

#include "mutex_private.h"

#include "ucb/error.h"
#include "ucb/memory.h"

ucb_cond* ucb_cond_new()
{
    ucb_cond* cond = ucb_malloc_type(1, ucb_cond);
    if (cond && !ucb_cond_init(cond))
    {
        ucb_free(cond);
        cond = UCB_NULL;
    }
    return cond;
}

void ucb_cond_free(ucb_cond* cond)
{
    ucb_cond_release(cond);
    ucb_free(cond);
}

bool ucb_cond_init(ucb_cond* cond)
{
    UCB_VERIFY_ARGS(cond);
#ifdef _WIN32
    InitializeConditionVariable(&cond->handle);
    return true;
#else
    int ret = pthread_cond_init(&cond->handle, NULL);
    UCB_REPORT_ERRNO(ret, "pthread_cond_init failed");
    return ret == 0;
#endif
}

bool ucb_cond_release(ucb_cond* cond)
{
    UCB_VERIFY_ARGS(cond);
#ifdef _WIN32
    // No-op on Windows
    return true;
#else
    int ret = pthread_cond_destroy(&cond->handle);
    UCB_REPORT_ERRNO(ret, "pthread_cond_destroy failed");
    return ret == 0;
#endif
}

void ucb_cond_signal(ucb_cond* cond)
{
    UCB_VERIFY_ARGS(cond);
#ifdef _WIN32
    WakeConditionVariable(&cond->handle);
#else
    UCB_REPORT_ERRNO(pthread_cond_signal(&cond->handle), "pthread_cond_signal failed");
#endif
}

void ucb_cond_broadcast(ucb_cond* cond)
{
    UCB_VERIFY_ARGS(cond);
#ifdef _WIN32
    WakeAllConditionVariable(&cond->handle);
#else
    UCB_REPORT_ERRNO(pthread_cond_broadcast(&cond->handle), "pthread_cond_broadcast failed");
#endif
}

bool ucb_cond_wait(ucb_cond* cond, ucb_mutex* mutex)
{
    UCB_VERIFY_ARGS(cond && mutex);
#ifdef _WIN32
    return SleepConditionVariableCS(&cond->handle, &mutex->handle, INFINITE) != 0;
#else
    int ret = pthread_cond_wait(&cond->handle, &mutex->handle);
    UCB_REPORT_ERRNO(ret, "pthread_cond_wait failed");
    return ret == 0;
#endif
}
