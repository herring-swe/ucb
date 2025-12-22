/**
 * This file is part of the UCB project
 * SPDX-FileCopyrightText: © 2025 Åke Svedin <ake@svedin.org>
 * SPDX-License-Identifier: MIT
 *
 * @brief
 */

#ifndef UCB_ERROR_PRIVATE_H
#define UCB_ERROR_PRIVATE_H

#include "ucb/error.h"

#include <stdint.h>

ucb_error_t ucb_wrap_errno(int err);
void ucb_set_last_errno(void);

#ifdef _WIN32
ucb_error_t ucb_wrap_win32_error(uint32_t err);
void ucb_set_last_win32_error(void);
#endif

#endif // UCB_ERROR_PRIVATE_H
