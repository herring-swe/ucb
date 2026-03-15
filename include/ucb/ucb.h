/**
 * @file ucb.h
 *
 * This file is part of the UCB project
 * - SPDX-FileCopyrightText: © 2026 Åke Svedin <ake@svedin.org>
 * - SPDX-License-Identifier: MIT
 *
 * @brief Main header
 */

#ifndef UCB_UCB_H
#define UCB_UCB_H

#include "export.h"

UCB_API const char* ucb_get_version(void);

/**
 * Initiate console to use UTF-8
 */
UCB_API void ucb_init_console(void);

#endif // UCB_UCB_H
