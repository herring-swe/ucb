/**
 * This file is part of the UCB project
 * SPDX-FileCopyrightText: © 2025 Åke Svedin <ake@svedin.org>
 * SPDX-License-Identifier: MIT
 *
 * @brief
 */

#ifndef UCB_TYPES_H
#define UCB_TYPES_H

#include <stdint.h>

#define UCB_PID_INVALID 0

/**
 * Default error type.
 * Always 0 for no error (UCB_OK).
 */
typedef int ucb_ecode;

/**
 * Process or thread ID
 */
typedef uint32_t ucb_pid_t;

#endif // UCB_TYPES_H
