/**
 * This file is part of the UCB project
 * SPDX-FileCopyrightText: © 2026 Åke Svedin <ake@svedin.org>
 * SPDX-License-Identifier: MIT
 *
 * @brief Debug macros and helpers
 */

#ifndef UCB_DEBUG_H
#define UCB_DEBUG_H

#ifndef NDEBUG // Debug mode

#include "error.h"

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>

#define UCB_ASSERT(expr, code, msg)                                              \
    do                                                                           \
    {                                                                            \
        if (!(expr))                                                             \
        {                                                                        \
            ucb_fatal_format(code, "%s: Assertion failure - %s", __func__, msg); \
            abort();                                                             \
        }                                                                        \
    } while (0)

#define UCB_ASSERT_INTERNAL(expr, msg) UCB_ASSERT(expr, UCB_ERROR_INTERNAL, msg)
#define UCB_ASSERT_STATUS(status, msg) UCB_ASSERT(status == 0, ucb_err_wrap_errno(status), msg)
#define UCB_ASSERT_ERRNO(status, msg)  UCB_ASSERT(status == 0, ucb_err_wrap_errno(errno), msg)
#ifdef _WIN32
#define UCB_ASSERT_WIN32(status, msg) UCB_ASSERT(status == 0, ucb_err_wrap_win32(status), msg)
#endif

#define UCB_WARN(code, fmt, ...) ucb_warn_format(code, "%s: " fmt, __func__, ##__VA_ARGS__)

#define UCB_PRINT(fmt, ...) fprintf(stderr, fmt, ##__VA_ARGS__)

#ifdef UCB_DEVEL
// Internal debugging enabled
#define UCB_DPRINT(fmt, ...) fprintf(stderr, fmt, ##__VA_ARGS__)
#endif

#else // Release mode

#define UCB_ASSERT(expr, code, msg)    ((void)0)
#define UCB_ASSERT_INTERNAL(expr, msg) ((void)0)
#define UCB_ASSERT_STATUS(status, msg) ((void)0)
#define UCB_ASSERT_ERRNO(status, msg)  ((void)0)
#ifdef _WIN32
#define UCB_ASSERT_WIN32(status, msg) ((void)0)
#endif

#define UCB_WARN(code, fmt, ...) ((void)0)

#define UCB_PRINT(fmt, ...) ((void)0)

#endif // NDEBUG

#ifndef UCB_DPRINT
#define UCB_DPRINT(fmt, ...) ((void)0)
#endif

#endif // UCB_DEBUG_H
