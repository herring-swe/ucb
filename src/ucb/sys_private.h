/**
 * @file sys_private.h
 * 
 * This file is part of the UCB project
 * - SPDX-FileCopyrightText: © 2026 Åke Svedin <ake@svedin.org>
 * - SPDX-License-Identifier: MIT
 * 
 * @brief Private system calls
 */

#ifndef UCB_SYS_PRIVATE_H
#define UCB_SYS_PRIVATE_H

#include <stddef.h>

size_t ucb_sys_get_thread_alignment(void);

#endif // UCB_SYS_PRIVATE_H
