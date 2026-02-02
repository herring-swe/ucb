/**
 * @file btrace.h
 * This file is part of the UCB project
 * - SPDX-FileCopyrightText: © 2026 Åke Svedin <ake@svedin.org>
 * - SPDX-License-Identifier: MIT
 *
 * @brief Cross-platform backtrace
 */

#ifndef UCB_BTRACE_H
#define UCB_BTRACE_H

#include "export.h"

#include <stddef.h>
#include <stdio.h>

/**
 * @struct ucb_btrace
 * @brief Backtrace
 */
typedef struct ucb_btrace
{
    char** strs;
    size_t count;
} ucb_btrace;

UCB_API ucb_btrace* ucb_btrace_new();
UCB_API void ucb_btrace_init(ucb_btrace* bt);
UCB_API void ucb_btrace_release(ucb_btrace* bt);
UCB_API void ucb_btrace_free(ucb_btrace* bt);

UCB_API ucb_btrace* ucb_btrace_clone(const ucb_btrace* bt);
UCB_API void ucb_btrace_copy(ucb_btrace* dst, const ucb_btrace* src);

UCB_API void ucb_btrace_capture(ucb_btrace* bt);
UCB_API void ucb_btrace_print(const ucb_btrace* bt, FILE* stream, int indent);

#endif // UCB_BTRACE_H
