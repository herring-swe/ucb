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

#include <errno.h>

_Static_assert(sizeof(struct ucb_mutex_impl) <= UCB_MUTEX_STORAGE_SIZE,
               "ucb_mutex storage is too small for the platform mutex");

#define MTX_STANDARD false
#define MTX_RECURSIVE true

static bool ucb_mutex_init_common(ucb_mutex* mutex, bool recursive)
{
    UCB_VERIFY_ARGS(mutex);

    struct ucb_mutex_impl* impl = UCB_MUTEX_IMPL(mutex);
    impl->recursive = recursive;
#if defined(_WIN32)
    impl->owner = UCB_PID_INVALID;
    impl->count = 0;
    InitializeCriticalSection(&impl->handle);
    return true;
#else
    pthread_mutexattr_t attr;
    int ret = pthread_mutexattr_init(&attr);
    if (UCB_REPORT_ERRNO(ret, "Failed to initialize mutex attributes"))
        return false;

    ret = pthread_mutexattr_settype(&attr,
                                    recursive ? PTHREAD_MUTEX_RECURSIVE : PTHREAD_MUTEX_ERRORCHECK);
    if (ret == 0)
        ret = pthread_mutex_init(&impl->handle, &attr);

    pthread_mutexattr_destroy(&attr);

    if (UCB_REPORT_ERRNO(ret, "Failed to initialize mutex"))
        return false;
    return true;
#endif
}

static ucb_mutex* ucb_mutex_new_common(bool recursive)
{
    ucb_mutex* mutex = ucb_malloc_type(1, ucb_mutex);
    if (!mutex)
        return UCB_NULL;
    if (!ucb_mutex_init_common(mutex, recursive))
    {
        ucb_free(mutex);
        return UCB_NULL;
    }
    return mutex;
}

ucb_mutex* ucb_mutex_new(void)
{
    return ucb_mutex_new_common(MTX_STANDARD);
}

ucb_mutex* ucb_mutex_new_recursive(void)
{
    return ucb_mutex_new_common(MTX_RECURSIVE);
}

bool ucb_mutex_init(ucb_mutex* mutex)
{
    return ucb_mutex_init_common(mutex, MTX_STANDARD);
}

bool ucb_mutex_init_recursive(ucb_mutex* mutex)
{
    return ucb_mutex_init_common(mutex, MTX_RECURSIVE);
}

void ucb_mutex_release(ucb_mutex* mutex)
{
    UCB_VERIFY_ARGS(mutex);

    struct ucb_mutex_impl* impl = UCB_MUTEX_IMPL(mutex);
#ifdef _WIN32
    UCB_VERIFY(impl->owner == UCB_PID_INVALID,
               UCB_ERROR_MUTEX_LOCKED,
               "Mutex locked during release");
    DeleteCriticalSection(&impl->handle);
#else
    int ret = pthread_mutex_destroy(&impl->handle);
    if (ret == EBUSY)
        UCB_REPORT(UCB_ERROR_MUTEX_LOCKED, "Mutex locked during release");
    UCB_REPORT_ERRNO(ret, "Failed to destroy mutex");
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
    return mutex && UCB_MUTEX_IMPL_CONST(mutex)->recursive;
}

void ucb_mutex_lock(ucb_mutex* mutex)
{
    UCB_VERIFY_ARGS(mutex);

    struct ucb_mutex_impl* impl = UCB_MUTEX_IMPL(mutex);
#if defined(_WIN32)
    ucb_pid self = ucb_thread_id();
    if (impl->owner == self)
    {
        if (!impl->recursive)
            UCB_REPORT(UCB_ERROR_MUTEX_LOCKED, "Standard mutex already locked by this thread");
        impl->count++;
        return;
    }
    EnterCriticalSection(&impl->handle);
    impl->owner = self;
    impl->count = 1;
#else
    int ret = pthread_mutex_lock(&impl->handle);
    if (ret == EDEADLK)
        UCB_REPORT(UCB_ERROR_MUTEX_LOCKED, "Standard mutex already locked by this thread");
    UCB_REPORT_ERRNO(ret, "Failed to lock mutex");
#endif
}

bool ucb_mutex_trylock(ucb_mutex* mutex)
{
    UCB_VERIFY_ARGS(mutex);

    struct ucb_mutex_impl* impl = UCB_MUTEX_IMPL(mutex);
#if defined(_WIN32)
    ucb_pid self = ucb_thread_id();
    if (impl->owner == self)
    {
        if (!impl->recursive)
            return false;
        impl->count++;
        return true;
    }
    if (TryEnterCriticalSection(&impl->handle))
    {
        impl->owner = self;
        impl->count = 1;
        return true;
    }
    return false;
#else
    int ret = pthread_mutex_trylock(&impl->handle);
    if (ret == 0)
        return true;
    if (ret == EBUSY)
        return false;
    if (ret == EDEADLK)
        UCB_REPORT(UCB_ERROR_MUTEX_LOCKED, "Standard mutex already locked by this thread");
    UCB_REPORT_ERRNO(ret, "Failed to lock mutex");
    return false;
#endif
}

void ucb_mutex_unlock(ucb_mutex* mutex)
{
    UCB_VERIFY_ARGS(mutex);

    struct ucb_mutex_impl* impl = UCB_MUTEX_IMPL(mutex);
#if defined(_WIN32)
    if (impl->owner != ucb_thread_id())
        UCB_REPORT(UCB_ERROR_MUTEX_LOCKED, "Mutex not locked by this thread");
    if (impl->recursive)
    {
        impl->count--;
        if (impl->count > 0)
            return;
    }
    // Clear the bookkeeping before releasing the critical section, otherwise a
    // thread that acquires it in between could have its ownership overwritten.
    impl->count = 0;
    impl->owner = UCB_PID_INVALID;
    LeaveCriticalSection(&impl->handle);
#else
    int ret = pthread_mutex_unlock(&impl->handle);
    if (ret == EPERM)
        UCB_REPORT(UCB_ERROR_MUTEX_LOCKED, "Mutex not locked by this thread");
    UCB_REPORT_ERRNO(ret, "Failed to unlock mutex");
#endif
}
