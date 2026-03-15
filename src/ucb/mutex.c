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

#include "mutex_private.h"

#include "ucb/error.h"
#include "ucb/memory.h"
#include "ucb/threads.h"

// TODO Check return codes from system calls

#define MTX_STANDARD false
#define MTX_RECURSIVE true

static void ucb_mutex_init_common(ucb_mutex* mutex, bool recursive)
{
    UCB_VERIFY_ARGS(mutex);

    mutex->recursive = recursive;
#if defined(_WIN32)
    mutex->owner = UCB_PID_INVALID;
    mutex->count = 0;
    InitializeCriticalSection(&mutex->handle);
#else
    pthread_mutexattr_t attr;
    pthread_mutexattr_init(&attr);
    if (mutex->recursive)
    {
        pthread_mutexattr_settype(&attr, PTHREAD_MUTEX_RECURSIVE);
    }
    if (UCB_REPORT_ERRNO(pthread_mutex_init(&mutex->handle, &attr), "Failed to initialize mutex"))
    {
        pthread_mutexattr_destroy(&attr);
    }
#endif
}

ucb_mutex* ucb_mutex_new()
{
    ucb_mutex* mutex = ucb_malloc_type(1, ucb_mutex);
    if (!mutex)
        return UCB_NULL;
    ucb_mutex_init_common(mutex, MTX_STANDARD);
    return mutex;
}

ucb_mutex* ucb_mutex_new_recursive()
{
    ucb_mutex* mutex = ucb_malloc_type(1, ucb_mutex);
    if (!mutex)
        return UCB_NULL;
    ucb_mutex_init_common(mutex, MTX_RECURSIVE);
    return mutex;
}

void ucb_mutex_init(ucb_mutex* mutex)
{
    ucb_mutex_init_common(mutex, MTX_STANDARD);
}

void ucb_mutex_init_recursive(ucb_mutex* mutex)
{
    ucb_mutex_init_common(mutex, MTX_RECURSIVE);
}

void ucb_mutex_release(ucb_mutex* mutex)
{
    UCB_VERIFY_ARGS(mutex);

#ifdef _WIN32
    UCB_VERIFY(mutex->owner == UCB_PID_INVALID, UCB_ERROR_MUTEX_LOCKED,
               "Mutex locked during release");
    DeleteCriticalSection(&mutex->handle);
#else
    UCB_REPORT_ERRNO(pthread_mutex_destroy(&mutex->handle), "Failed to destroy mutex");
#endif
}

void ucb_mutex_free(ucb_mutex* mutex)
{
    if (!mutex)
        return;
    ucb_mutex_release(mutex);
    ucb_free(mutex);
}

bool ucb_mutex_is_recursive(const ucb_mutex* mutex)
{
    return mutex && mutex->recursive;
}

void ucb_mutex_lock(ucb_mutex* mutex)
{
    UCB_VERIFY_ARGS(mutex);

#if defined(_WIN32)
    if (mutex->recursive && mutex->owner == ucb_thread_id())
    {
        mutex->count++;
        return;
    }
    EnterCriticalSection(&mutex->handle);
    mutex->owner = ucb_thread_id();
    mutex->count = 1;
#else
    UCB_REPORT_ERRNO(pthread_mutex_lock(&mutex->handle), "Failed to lock mutex");
#endif
}

bool ucb_mutex_trylock(ucb_mutex* mutex)
{
    UCB_VERIFY_ARGS(mutex);

#if defined(_WIN32)
    if (mutex->recursive && mutex->owner == ucb_thread_id())
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
    UCB_REPORT_ERRNO(pthread_mutex_trylock(&mutex->handle) == 0, "Failed to lock mutex");
    return true;
#endif
}

void ucb_mutex_unlock(ucb_mutex* mutex)
{
    UCB_VERIFY_ARGS(mutex);

#if defined(_WIN32)
    if (mutex->recursive && mutex->owner == ucb_thread_id())
    {
        mutex->count--;
        if (mutex->count > 0)
            return;
    }
    LeaveCriticalSection(&mutex->handle);
    mutex->count = 0;
    mutex->owner = UCB_PID_INVALID;
#else
    UCB_REPORT_ERRNO(pthread_mutex_unlock(&mutex->handle), "Failed to unlock mutex");
#endif
}
