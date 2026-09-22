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
#include "ucb/threads.h"

#include <errno.h>
#include <time.h>

_Static_assert(sizeof(struct ucb_cond_impl) <= UCB_COND_STORAGE_SIZE,
               "ucb_cond storage is too small for the platform condition");

// Use a monotonic clock for timeouts where the platform supports it, so that
// wall-clock changes cannot break a timed wait.
#if !defined(_WIN32) && defined(__linux__) && defined(CLOCK_MONOTONIC)
#define UCB_COND_USE_MONOTONIC 1
#else
#define UCB_COND_USE_MONOTONIC 0
#endif

ucb_cond* ucb_cond_new(void)
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
    if (!cond)
        return;
    ucb_cond_release(cond);
    ucb_free(cond);
}

bool ucb_cond_init(ucb_cond* cond)
{
    UCB_VERIFY_ARGS(cond);

    struct ucb_cond_impl* impl = UCB_COND_IMPL(cond);
#ifdef _WIN32
    InitializeConditionVariable(&impl->handle);
    return true;
#elif UCB_COND_USE_MONOTONIC
    pthread_condattr_t attr;
    int ret = pthread_condattr_init(&attr);
    if (ret == 0)
    {
        ret = pthread_condattr_setclock(&attr, CLOCK_MONOTONIC);
        if (ret == 0)
            ret = pthread_cond_init(&impl->handle, &attr);
        pthread_condattr_destroy(&attr);
    }
    if (UCB_REPORT_ERRNO(ret, "pthread_cond_init failed"))
        return false;
    return true;
#else
    int ret = pthread_cond_init(&impl->handle, NULL);
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
    int ret = pthread_cond_destroy(&UCB_COND_IMPL(cond)->handle);
    UCB_REPORT_ERRNO(ret, "pthread_cond_destroy failed");
    return ret == 0;
#endif
}

void ucb_cond_signal(ucb_cond* cond)
{
    UCB_VERIFY_ARGS(cond);
#ifdef _WIN32
    WakeConditionVariable(&UCB_COND_IMPL(cond)->handle);
#else
    UCB_REPORT_ERRNO(pthread_cond_signal(&UCB_COND_IMPL(cond)->handle),
                     "pthread_cond_signal failed");
#endif
}

void ucb_cond_broadcast(ucb_cond* cond)
{
    UCB_VERIFY_ARGS(cond);
#ifdef _WIN32
    WakeAllConditionVariable(&UCB_COND_IMPL(cond)->handle);
#else
    UCB_REPORT_ERRNO(pthread_cond_broadcast(&UCB_COND_IMPL(cond)->handle),
                     "pthread_cond_broadcast failed");
#endif
}

bool ucb_cond_wait(ucb_cond* cond, ucb_mutex* mutex)
{
    UCB_VERIFY_ARGS(cond && mutex);
#ifdef _WIN32
    struct ucb_mutex_impl* mimpl = UCB_MUTEX_IMPL(mutex);
    int saved_count = mimpl->count;
    BOOL ok = SleepConditionVariableCS(&UCB_COND_IMPL(cond)->handle, &mimpl->handle, INFINITE);
    // The condition variable releases and reacquires the critical section
    // internally, so the owner bookkeeping must be restored.
    mimpl->owner = ucb_thread_id();
    mimpl->count = saved_count;
    return ok != 0;
#else
    int ret = pthread_cond_wait(&UCB_COND_IMPL(cond)->handle, &UCB_MUTEX_IMPL(mutex)->handle);
    UCB_REPORT_ERRNO(ret, "pthread_cond_wait failed");
    return ret == 0;
#endif
}

ucb_cond_result ucb_cond_timedwait(ucb_cond* cond, ucb_mutex* mutex, ucb_stime_ms timeout_ms)
{
    UCB_VERIFY_ARGS(cond && mutex && timeout_ms >= 0);

#ifdef _WIN32
    struct ucb_mutex_impl* mimpl = UCB_MUTEX_IMPL(mutex);
    int saved_count = mimpl->count;
    BOOL ok =
        SleepConditionVariableCS(&UCB_COND_IMPL(cond)->handle, &mimpl->handle, (DWORD)timeout_ms);
    // See ucb_cond_wait: the owner bookkeeping must be restored after the
    // critical section has been reacquired.
    mimpl->owner = ucb_thread_id();
    mimpl->count = saved_count;
    if (ok)
        return UCB_COND_SIGNALLED;
    if (GetLastError() == ERROR_TIMEOUT)
        return UCB_COND_TIMEDOUT;
    UCB_REPORT_WIN32(GetLastError(), "SleepConditionVariableCS failed");
    return UCB_COND_ERROR;
#else
    struct timespec ts;
#if UCB_COND_USE_MONOTONIC
    if (clock_gettime(CLOCK_MONOTONIC, &ts) != 0)
#else
    if (clock_gettime(CLOCK_REALTIME, &ts) != 0)
#endif
    {
        UCB_REPORT_ERRNO(errno, "clock_gettime failed");
        return UCB_COND_ERROR;
    }
    ts.tv_sec += timeout_ms / 1000;
    ts.tv_nsec += (long)(timeout_ms % 1000) * 1000000L;
    if (ts.tv_nsec >= 1000000000L)
    {
        ts.tv_sec += 1;
        ts.tv_nsec -= 1000000000L;
    }

    int ret =
        pthread_cond_timedwait(&UCB_COND_IMPL(cond)->handle, &UCB_MUTEX_IMPL(mutex)->handle, &ts);
    if (ret == 0)
        return UCB_COND_SIGNALLED;
    if (ret == ETIMEDOUT)
        return UCB_COND_TIMEDOUT;
    UCB_REPORT_ERRNO(ret, "pthread_cond_timedwait failed");
    return UCB_COND_ERROR;
#endif
}
