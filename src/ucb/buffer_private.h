/**
 * @file buffer_private.h
 *
 * This file is part of the UCB project
 * - SPDX-FileCopyrightText: © 2026 Åke Svedin <ake@svedin.org>
 * - SPDX-License-Identifier: MIT
 *
 * @brief Buffer type internal
 */

#ifndef UCB_BUFFER_PRIVATE_H
#define UCB_BUFFER_PRIVATE_H

#include "ucb/buffer.h"

/* -------------------------------------------------------------------------- */
/*                             Plain malloc buffer                            */
/* -------------------------------------------------------------------------- */

ucb_buffer* ucb_buffer_new_malloc(size_t initial_capacity);
bool ucb_buffer_init_malloc(ucb_buffer* buf, size_t initial_capacity);

#endif // UCB_BUFFER_PRIVATE_H
