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
/*                            Untracked malloc buffer                         */
/* -------------------------------------------------------------------------- */

/*
 * These buffers intentionally allocate with raw malloc/calloc/realloc/free
 * instead of the tracked ucb_* wrappers. They are used by btrace.c, which is
 * captured from within memdbg.c while the allocation tracker is active; going
 * through ucb_malloc there would recurse into the tracker and loop.
 *
 * Do not convert these to ucb_malloc/ucb_free.
 */

ucb_buffer* ucb_buffer_new_untracked(size_t initial_capacity);
bool ucb_buffer_init_untracked(ucb_buffer* buf, size_t initial_capacity);

#endif // UCB_BUFFER_PRIVATE_H
