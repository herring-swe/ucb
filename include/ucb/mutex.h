/**
 * @file mutex.h
 *
 * This file is part of the UCB project
 * - SPDX-FileCopyrightText: © 2026 Åke Svedin <ake@svedin.org>
 * - SPDX-License-Identifier: MIT
 *
 * @brief Cross-platform mutex
 *
 * A mutex can be allocated on the heap with @ref ucb_mutex_new or
 * @ref ucb_mutex_new_recursive, or initiated on the stack with
 * @ref ucb_mutex_init or @ref ucb_mutex_init_recursive.
 *
 * The mutex is a complete type so that it can be declared as a stack variable.
 * Its contents are private; never access the fields directly.
 *
 * @code{.c}
 * ucb_mutex mutex;
 * ucb_mutex_init(&mutex);
 * ucb_mutex_lock(&mutex);
 * // ...
 * ucb_mutex_unlock(&mutex);
 * ucb_mutex_release(&mutex);
 * @endcode
 *
 * A standard (non-recursive) mutex must not be locked again by the thread that
 * already holds it, and must only be unlocked by the owning thread. Such misuse
 * is reported as @ref UCB_ERROR_MUTEX_LOCKED and aborts.
 */

#ifndef UCB_MUTEX_H
#define UCB_MUTEX_H

#include <ucb/export.h>

#include <stdbool.h>

/**
 * @brief Size in bytes of the private mutex storage.
 *
 * Implementation detail. The public struct is sized so that the platform
 * primitive fits inside it.
 */
#define UCB_MUTEX_STORAGE_SIZE 64

/**
 * @struct ucb_mutex
 * @brief Mutex
 *
 * The contents are private. Use the @c ucb_mutex_* functions.
 */
typedef struct ucb_mutex
{
    union
    {
        void* _ptr;
        long long _i64;
        double _f64;
        long double _f80;
        unsigned char _bytes[UCB_MUTEX_STORAGE_SIZE];
    } _opaque;
} ucb_mutex;

/**
 * @brief Allocates and initiates a new standard (non-recursive) mutex
 * @return the new mutex, or UCB_NULL on failure
 */
UCB_API ucb_mutex* ucb_mutex_new(void);

/**
 * @brief Allocates and initiates a new recursive mutex
 * @return the new mutex, or UCB_NULL on failure
 */
UCB_API ucb_mutex* ucb_mutex_new_recursive(void);

/**
 * @brief Initiate a standard (non-recursive) mutex on the stack
 *
 * Release it with @ref ucb_mutex_release.
 *
 * @param mutex pointer to the mutex
 * @return true on success
 */
UCB_API bool ucb_mutex_init(ucb_mutex* mutex);

/**
 * @brief Initiate a recursive mutex on the stack
 *
 * A recursive mutex may be locked multiple times by the same thread, and must
 * be unlocked the same number of times.
 *
 * Release it with @ref ucb_mutex_release.
 *
 * @param mutex pointer to the mutex
 * @return true on success
 */
UCB_API bool ucb_mutex_init_recursive(ucb_mutex* mutex);

/**
 * @brief Release a mutex initiated with @ref ucb_mutex_init or
 * @ref ucb_mutex_init_recursive
 *
 * The mutex must not be locked. Releasing a locked mutex is reported as
 * @ref UCB_ERROR_MUTEX_LOCKED and aborts.
 *
 * @param mutex the mutex
 */
UCB_API void ucb_mutex_release(ucb_mutex* mutex);

/**
 * @brief Free a mutex allocated with @ref ucb_mutex_new or
 * @ref ucb_mutex_new_recursive
 *
 * If UCB_NULL, this is a safe no-op.
 *
 * @param mutex the mutex
 */
UCB_API void ucb_mutex_free(ucb_mutex* mutex);

/**
 * @brief Check if the mutex is recursive
 * @param mutex the mutex
 * @return true if the mutex is recursive
 */
UCB_API bool ucb_mutex_is_recursive(const ucb_mutex* mutex);

/**
 * @brief Lock the mutex, blocking until it is available
 *
 * Locking a standard mutex that is already held by the current thread is
 * reported as @ref UCB_ERROR_MUTEX_LOCKED and aborts.
 *
 * @param mutex the mutex
 */
UCB_API void ucb_mutex_lock(ucb_mutex* mutex);

/**
 * @brief Try to lock the mutex without blocking
 * @param mutex the mutex
 * @return true if the mutex was locked, false if it is already locked
 */
UCB_API bool ucb_mutex_trylock(ucb_mutex* mutex);

/**
 * @brief Unlock the mutex
 *
 * Unlocking a mutex not owned by the current thread is reported as
 * @ref UCB_ERROR_MUTEX_LOCKED and aborts.
 *
 * @param mutex the mutex
 */
UCB_API void ucb_mutex_unlock(ucb_mutex* mutex);

#endif // UCB_MUTEX_H
