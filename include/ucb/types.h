/**
 * @file types.h
 *
 * This file is part of the UCB project
 * - SPDX-FileCopyrightText: © 2026 Åke Svedin <ake@svedin.org>
 * - SPDX-License-Identifier: MIT
 *
 * @brief Common type definitions
 */

#ifndef UCB_TYPES_H
#define UCB_TYPES_H

#include <stddef.h>
#include <stdint.h>

#define UCB_PID_INVALID 0

/**
 * @brief Used to indicate no position in a string.
 *
 * Each function may specify if this value holds a special meaning.
 */
#define UCB_NPOS (SIZE_MAX - 1)

/**
 * Default error type.
 * Always 0 for no error (UCB_OK).
 */
typedef int ucb_ecode;

/**
 * Process or thread ID
 */
typedef uint32_t ucb_pid;

/**
 * Unicode codepoint
 */
typedef uint32_t ucb_cp;

/**
 * Signed size_t
 */
typedef ptrdiff_t ucb_ssize;

#endif // UCB_TYPES_H
