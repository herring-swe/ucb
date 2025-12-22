/**
 * This file is part of the UCB project
 * SPDX-FileCopyrightText: © 2025 Åke Svedin <ake@svedin.org>
 * SPDX-License-Identifier: MIT
 *
 * @brief
 */

#ifndef UCB_DEFINES_H
#define UCB_DEFINES_H

#define UCB_UNUSED(x) (void)(x)

// clang-format off
#ifdef __cplusplus
    #define UCB_NULL nullptr
#else
    #if defined(__STDC_VERSION__) && __STDC_VERSION__ >= 202311L
        // C23 or later
        #define UCB_NULL nullptr    
        #define UCB_NORETURN [[ noreturn ]]
    #else
        #define UCB_NULL ((void*)0)
        #if defined(__GNUC__) || defined(__clang__)
            #define UCB_NORETURN __attribute__((noreturn))
            #define UCB_LIKELY(x) __builtin_expect(!!(x), 1)
            #define UCB_UNLIKELY(x) __builtin_expect(!!(x), 0)
        #elif defined(_WIN32)
            #define UCB_NORETURN _Noreturn
            #define UCB_LIKELY(x) (x)
            #define UCB_UNLIKELY(x) (x)
        #endif

        #ifdef UCB_FORWARD_DECLARE
            #define nullptr ((void*)0)
            #define void* nullptr_t;
        #endif
    #endif
#endif

#if defined(__GNUC__) || defined(__clang__)
    #define UCB_LIKELY(x) __builtin_expect(!!(x), 1)
    #define UCB_UNLIKELY(x) __builtin_expect(!!(x), 0)
#elif defined(_WIN32)
    #define UCB_LIKELY(x) (x)
    #define UCB_UNLIKELY(x) (x)
#endif
// clang-format on

// Boilerplate macro for functions that return ucb_error_t
#define UCB_TRY(call)              \
    do                             \
    {                              \
        ucb_error_t _err = (call); \
        if (_err != UCB_ERROR_OK)  \
            return _err;           \
    } while (0)

/**
 * Boilerplate macro for functions that return something else than ucb_error_t
 */
#define UCB_TRY_SET(call, errval)     \
    do                                \
    {                                 \
        ucb_error_t _err = (call);    \
        if (_err != UCB_ERROR_OK)     \
        {                             \
            ucb_set_last_error(_err); \
            return errval;            \
        }                             \
    } while (0)

#endif // UCB_DEFINES_H
