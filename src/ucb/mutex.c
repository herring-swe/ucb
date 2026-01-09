/**
 * @file mutex.c
 * 
 * This file is part of the UCB project
 * - SPDX-FileCopyrightText: © 2026 Åke Svedin <ake@svedin.org>
 * - SPDX-License-Identifier: MIT
 * 
 * @brief Cross-platform mutex implementation
 */

#if defined(__INTELLISENSE__) && defined(__GNUC__)
#define PTHREAD_MUTEX_RECURSIVE PTHREAD_MUTEX_RECURSIVE_NP
#endif

#include "ucb/mutex.h"

#include "ucb/error.h"
#include "ucb/memory.h"
#include "ucb/threads.h"

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

// TODO Check return codes from system calls

ucb_mutex* ucb_mutex_new(int flags)
{
    ucb_mutex* mutex = ucb_malloc_type(1, ucb_mutex);
    if (!mutex)
        return UCB_NULL;
    ucb_mutex_init(mutex, flags);
    return mutex;
}

void ucb_mutex_init(ucb_mutex* mutex, int flags)
{
    UCB_VERIFY_ARGS(mutex);

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
    if (UCB_VERIFY_ERRNO(pthread_mutex_init(&mutex->handle, &attr), "Failed to initialize mutex"))
    {
        pthread_mutexattr_destroy(&attr);
    }
#endif
}

void ucb_mutex_release(ucb_mutex* mutex)
{
    UCB_VERIFY_ARGS(mutex);

#ifdef _WIN32
    UCB_VERIFY(mutex->owner == UCB_PID_INVALID, UCB_ERROR_MUTEX_LOCKED,
               "Mutex locked during release");
    DeleteCriticalSection(&mutex->handle);
#else
    UCB_VERIFY_ERRNO(pthread_mutex_destroy(&mutex->handle), "Failed to destroy mutex");
#endif
}

void ucb_mutex_free(ucb_mutex* mutex)
{
    if (!mutex)
        return;
    ucb_mutex_release(mutex);
    ucb_free(mutex);
}

void ucb_mutex_lock(ucb_mutex* mutex)
{
    UCB_VERIFY_ARGS(mutex);

#if defined(_WIN32)
    if (mutex->flags & UCB_MUTEX_RECURSIVE && mutex->owner == ucb_thread_id())
    {
        mutex->count++;
        return;
    }
    EnterCriticalSection(&mutex->handle);
    mutex->owner = ucb_thread_id();
    mutex->count = 1;
#else
    UCB_VERIFY_ERRNO(pthread_mutex_lock(&mutex->handle), "Failed to lock mutex");
#endif
}

bool ucb_mutex_trylock(ucb_mutex* mutex)
{
    UCB_VERIFY_ARGS_RET(mutex, false);

#if defined(_WIN32)
    if (mutex->flags & UCB_MUTEX_RECURSIVE && mutex->owner == ucb_thread_id())
    {
        mutex->count++;
        return true;
    }
    if (TryEnterCriticalSection(&mutex->handle))
    {
        mutex->owner = ucb_thread_id();
        mutex->count = 1;
        return true;
    }
    return false;
#else
    UCB_VERIFY_ERRNO(pthread_mutex_trylock(&mutex->handle) == 0, "Failed to lock mutex");
    return true;
#endif
}

void ucb_mutex_unlock(ucb_mutex* mutex)
{
    UCB_VERIFY_ARGS(mutex);

#if defined(_WIN32)
    if (mutex->flags & UCB_MUTEX_RECURSIVE && mutex->owner == ucb_thread_id())
    {
        mutex->count--;
        if (mutex->count > 0)
            return;
    }
    LeaveCriticalSection(&mutex->handle);
    mutex->count = 0;
    mutex->owner = UCB_PID_INVALID;
#else
    UCB_VERIFY_ERRNO(pthread_mutex_unlock(&mutex->handle), "Failed to unlock mutex");
#endif
}
