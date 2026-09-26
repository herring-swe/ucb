/**
 * @file buffer_private.c
 *
 * This file is part of the UCB project
 * - SPDX-FileCopyrightText: © 2026 Åke Svedin <ake@svedin.org>
 * - SPDX-License-Identifier: MIT
 *
 * @brief Buffer type internal implementation
 */

#include "buffer_private.h"

#include <assert.h>
#include <stdlib.h>
#include <string.h>

/* -------------------------------------------------------------------------- */
/*                            Untracked malloc buffer                         */
/* -------------------------------------------------------------------------- */

/*
 * Raw allocators on purpose: see buffer_private.h. This path is used from
 * within memdbg and must not call ucb_malloc/ucb_calloc/ucb_realloc/ucb_free.
 */

ucb_buffer* ucb_buffer_new_untracked(size_t initial_capacity)
{
    ucb_buffer* buf = calloc(1, sizeof(ucb_buffer));
    if (buf)
    {
        if (!ucb_buffer_init_untracked(buf, initial_capacity))
        {
            free(buf);
            buf = UCB_NULL;
        }
    }
    return buf;
}

static bool ucb_buffer_resize_malloc(ucb_buffer* buf, size_t new_capacity)
{
    assert(buf);
    if (new_capacity == 0)
    {
        free(buf->data);
        buf->data = UCB_NULL;
        buf->alloc = 0;
        return true;
    }

    char* tmp = (char*)realloc(buf->data, new_capacity);
    if (!tmp)
        return false;

    buf->data = tmp;
    buf->alloc = new_capacity;
    return true;
}

static void ucb_buffer_free_malloc(ucb_buffer* buf)
{
    if (buf)
    {
        if (buf->data)
            free(buf->data);
        buf->data = UCB_NULL;
    }
}

static bool ucb_buffer_transfer_malloc(ucb_buffer* buf,
                                       void** out_data,
                                       size_t* out_used,
                                       size_t* out_capacity,
                                       const ucb_error** perr)
{
    UCB_UNUSED(perr);
    *out_data = buf->data;
    if (out_used)
        *out_used = buf->size;
    if (out_capacity)
        *out_capacity = buf->alloc;
    return true;
}

bool ucb_buffer_init_untracked(ucb_buffer* buf, size_t initial_capacity)
{
    UCB_VERIFY_ARGS(buf && initial_capacity > 0);

    memset(buf, 0, sizeof(ucb_buffer));

    buf->data = (char*)calloc(1, initial_capacity);
    if (!buf->data)
        return false;

    buf->alloc = initial_capacity;
    buf->size = 0;
    buf->_impl_resize = ucb_buffer_resize_malloc;
    buf->_impl_free = ucb_buffer_free_malloc;
    buf->_impl_transfer = ucb_buffer_transfer_malloc;
    return true;
}
