/**
 * This file is part of the UCB project
 * SPDX-FileCopyrightText: © 2025 Åke Svedin <ake@svedin.org>
 * SPDX-License-Identifier: MIT
 *
 * @brief Cross-platform mutex implementation
 */

#if defined(__INTELLISENSE__) && defined(__GNUC__)
#define PTHREAD_MUTEX_RECURSIVE PTHREAD_MUTEX_RECURSIVE_NP
#endif

#include "ucb/mutex.h"

#include "ucb/errcodes.h"
#include "ucb/memory.h"
#include "ucb/thread.h"

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
    int count;
#else
    pthread_mutex_t handle;
#endif
    ucb_pid_t owner;
    int flags;
};

// TODO Check return codes from system calls

ucb_mutex_t* ucb_mutex_new(int flags)
{
    ucb_mutex_t* mutex = ucb_malloc_type(1, ucb_mutex_t);
    if (!mutex)
        return UCB_NULL;
    ucb_mutex_init(mutex, flags);
    return mutex;
}

ucb_ecode ucb_mutex_init(ucb_mutex_t* mutex, int flags)
{
    if (!mutex)
        return UCB_ERROR_INVALID_ARG;
    mutex->flags = flags;
#if defined(_WIN32)
    mutex->owner = UCB_PID_INVALID;
    mutex->count = 0;
    InitializeCriticalSection(&mutex->handle);
#else
    pthread_mutexattr_t attr;
    pthread_mutexattr_init(&attr);
    if (mutex->flags & UCB_MUTEX_RECURSIVE)
    {
        pthread_mutexattr_settype(&attr, PTHREAD_MUTEX_RECURSIVE);
    }
    pthread_mutex_init(&mutex->handle, &attr);
#endif
    return UCB_OK;
}

ucb_ecode ucb_mutex_release(ucb_mutex_t* mutex)
{
    if (!mutex)
        return UCB_ERROR_INVALID_ARG;
    if (mutex->owner != UCB_PID_INVALID)
    {
        ucb_user(ucb_error_literal(UCB_ERROR_MUTEX_LOCKED, "Mutex locked during release"));
        return UCB_ERROR_MUTEX_LOCKED;
    }
#ifdef _WIN32
    DeleteCriticalSection(&mutex->handle);
#else
    pthread_mutex_destroy(&mutex->handle);
#endif
    return UCB_OK;
}

void ucb_mutex_free(ucb_mutex_t* mutex)
{
    if (!mutex)
        return;
    ucb_mutex_release(mutex);
    ucb_free(mutex);
}

ucb_ecode ucb_mutex_lock(ucb_mutex_t* mutex)
{
#if defined(_WIN32)
    if (mutex->flags & UCB_MUTEX_RECURSIVE && mutex->owner == ucb_thread_id())
    {
        mutex->count++;
        return UCB_OK;
    }
    EnterCriticalSection(&mutex->handle);
    mutex->owner = ucb_thread_id();
    mutex->count = 1;
#else
    return pthread_mutex_lock(&mutex->handle);
#endif
    return UCB_OK;
}

ucb_ecode ucb_mutex_trylock(ucb_mutex_t* mutex)
{
#if defined(_WIN32)
    if (mutex->flags & UCB_MUTEX_RECURSIVE && mutex->owner == ucb_thread_id())
    {
        mutex->count++;
        return UCB_OK;
    }
    if (TryEnterCriticalSection(&mutex->handle))
    {
        mutex->owner = ucb_thread_id();
        mutex->count = 1;
        return UCB_OK;
    }
    return UCB_ERROR_FALSE;
#else
    return pthread_mutex_trylock(&mutex->handle) == 0;
#endif
}

ucb_ecode ucb_mutex_unlock(ucb_mutex_t* mutex)
{
#if defined(_WIN32)
    if (mutex->flags & UCB_MUTEX_RECURSIVE && mutex->owner == ucb_thread_id())
    {
        mutex->count--;
        if (mutex->count > 0)
            return UCB_OK;
    }
    LeaveCriticalSection(&mutex->handle);
    mutex->count = 0;
    mutex->owner = UCB_PID_INVALID;
#else
    return pthread_mutex_unlock(&mutex->handle);
#endif
    return UCB_OK;
}
