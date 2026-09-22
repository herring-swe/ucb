/**
 * @file once.h
 * This file is part of the UCB project
 * - SPDX-FileCopyrightText: © 2026 Åke Svedin <ake@svedin.org>
 * - SPDX-License-Identifier: MIT
 *
 * @brief Cross platform one-time initialization
 *
 * A one-time object can be allocated on the heap with @ref ucb_once_new,
 * initiated on the stack with @ref ucb_once_init, or statically initialized
 * with @ref UCB_ONCE_INIT.
 *
 * The object is a complete type so that it can be declared as a stack or static
 * variable. Its contents are private; never access the fields directly.
 *
 * @code{.c}
 * static ucb_once once = UCB_ONCE_INIT;
 *
 * void init(void) { ... }
 *
 * void use(void)
 * {
 *     ucb_once_run(&once, init);
 * }
 * @endcode
 */

#ifndef UCB_ONCE_H
#define UCB_ONCE_H

#include <ucb/export.h>

/**
 * @brief Size in bytes of the private one-time storage.
 *
 * Implementation detail. The public struct is sized so that the platform
 * primitive fits inside it.
 */
#define UCB_ONCE_STORAGE_SIZE 16

/**
 * @struct ucb_once
 * @brief One-time initialization object
 *
 * The contents are private. Use the @c ucb_once_* functions.
 */
typedef struct ucb_once
{
    union
    {
        void* _ptr;
        long long _i64;
        double _f64;
        long double _f80;
        unsigned char _bytes[UCB_ONCE_STORAGE_SIZE];
    } _opaque;
} ucb_once;

/**
 * @brief Static initializer for a one-time object
 *
 * Use for static or stack objects instead of @ref ucb_once_init.
 */
#define UCB_ONCE_INIT {{0}}

/**
 * @brief Allocates and initiates a new one-time object
 * @return the new object or UCB_NULL on failure
 */
UCB_API ucb_once* ucb_once_new(void);

/**
 * @brief Initiate a one-time object
 *
 * Equivalent to initializing with @ref UCB_ONCE_INIT.
 * A one-time object has no resources, so no release call is needed.
 *
 * @param once pointer to the object
 */
UCB_API void ucb_once_init(ucb_once* once);

/**
 * @brief Free a one-time object
 * @param once the object, may be UCB_NULL
 */
UCB_API void ucb_once_free(ucb_once* once);

/**
 * @brief Run a function exactly once
 *
 * The first call runs @p func. Any later call, from any thread, is a no-op.
 * The call is thread-safe and blocks concurrent callers until @p func has
 * completed.
 *
 * @warning @p func must not call @ref ucb_once_run on the same object, and must
 * not use longjmp or otherwise leave the call abnormally.
 *
 * @param once the one-time object
 * @param func the function to run once
 */
UCB_API void ucb_once_run(ucb_once* once, void (*func)(void));

#endif // UCB_ONCE_H
