/**
 * @file cond.h
 * This file is part of the UCB project
 * - SPDX-FileCopyrightText: © 2026 Åke Svedin <ake@svedin.org>
 * - SPDX-License-Identifier: MIT
 *
 * @brief Cross platform thread condition
 *
 * A condition variable can be allocated on the heap with @ref ucb_cond_new or
 * initiated on the stack with @ref ucb_cond_init.
 *
 * The condition is a complete type so that it can be declared as a stack
 * variable. Its contents are private; never access the fields directly.
 *
 * @code{.c}
 * ucb_cond cond;
 * ucb_cond_init(&cond);
 * // ...
 * ucb_cond_release(&cond);
 * @endcode
 */

#ifndef UCB_COND_H
#define UCB_COND_H

#include <ucb/export.h>
#include <ucb/mutex.h>
#include <ucb/time.h>

#include <stdbool.h>

/**
 * @brief Size in bytes of the private condition storage.
 *
 * Implementation detail. The public struct is sized so that the platform
 * primitive fits inside it.
 */
#define UCB_COND_STORAGE_SIZE 64

/**
 * @struct ucb_cond
 * @brief Condition variable
 *
 * The contents are private. Use the @c ucb_cond_* functions.
 */
typedef struct ucb_cond
{
    union
    {
        void* _ptr;
        long long _i64;
        double _f64;
        long double _f80;
        unsigned char _bytes[UCB_COND_STORAGE_SIZE];
    } _opaque;
} ucb_cond;

/**
 * @brief Result of a condition wait
 */
typedef enum ucb_cond_result
{
    UCB_COND_SIGNALLED = 0, ///< The condition was signalled
    UCB_COND_TIMEDOUT,      ///< The timeout expired before the condition was signalled
    UCB_COND_ERROR,         ///< A system error occurred and was reported
} ucb_cond_result;

/**
 * @brief Allocates and initiates a new condition
 * @return the new condition, or UCB_NULL on failure
 */
UCB_API ucb_cond* ucb_cond_new(void);

/**
 * @brief Initiate a condition on the stack
 *
 * Release it with @ref ucb_cond_release.
 *
 * @param cond pointer to the condition
 * @return true on success
 */
UCB_API bool ucb_cond_init(ucb_cond* cond);

/**
 * @brief Release a condition initiated with @ref ucb_cond_init
 * @param cond the condition
 * @return true on success
 */
UCB_API bool ucb_cond_release(ucb_cond* cond);

/**
 * @brief Free a condition allocated with @ref ucb_cond_new
 *
 * If UCB_NULL, this is a safe no-op.
 *
 * @param cond the condition
 */
UCB_API void ucb_cond_free(ucb_cond* cond);

/**
 * @brief Wake one thread waiting on the condition
 * @param cond the condition
 */
UCB_API void ucb_cond_signal(ucb_cond* cond);

/**
 * @brief Wake all threads waiting on the condition
 * @param cond the condition
 */
UCB_API void ucb_cond_broadcast(ucb_cond* cond);

/**
 * @brief Wait for a condition to be signalled
 *
 * The mutex must be locked by the current thread before this call.
 * The mutex will be unlocked while waiting and locked again before returning.
 *
 * Spurious wakeups are possible and must be handled by the caller.
 *
 * @param cond the condition
 * @param mutex the mutex
 * @return true if the condition was signalled, false on error
 */
UCB_API bool ucb_cond_wait(ucb_cond* cond, ucb_mutex* mutex);

/**
 * @brief Wait for a condition to be signalled, with a timeout
 *
 * Behaves like @ref ucb_cond_wait, but returns @ref UCB_COND_TIMEDOUT if the
 * condition was not signalled within @p timeout_ms milliseconds.
 *
 * The timeout is relative to the time of the call.
 *
 * @param cond the condition
 * @param mutex the mutex
 * @param timeout_ms relative timeout in milliseconds, must be non-negative
 * @return the wait result
 */
UCB_API ucb_cond_result ucb_cond_timedwait(ucb_cond* cond,
                                           ucb_mutex* mutex,
                                           ucb_stime_ms timeout_ms);

#endif // UCB_COND_H
