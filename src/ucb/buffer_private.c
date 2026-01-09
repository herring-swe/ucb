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
/*                             Plain malloc buffer                            */
/* -------------------------------------------------------------------------- */

ucb_buffer* ucb_buffer_new_malloc(size_t initial_capacity)
{
    ucb_buffer* buf = calloc(1, sizeof(ucb_buffer));
    if (buf)
    {
        if (!ucb_buffer_init_malloc(buf, initial_capacity))
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
    char* tmp = (char*)realloc(buf->data, new_capacity);
    if (!tmp)
        return false;

    buf->data     = tmp;
    buf->capacity = new_capacity;
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

static bool ucb_buffer_transfer_malloc(ucb_buffer* buf, void** out_data, size_t* out_used,
                                       size_t* out_capacity, const ucb_error** perr)
{
    UCB_UNUSED(perr);
    *out_data = buf->data;
    if (out_used)
        *out_used = buf->used;
    if (out_capacity)
        *out_capacity = buf->capacity;
    return true;
}

bool ucb_buffer_init_malloc(ucb_buffer* buf, size_t initial_capacity)
{
    UCB_VERIFY_ARGS_RET(buf && initial_capacity > 0, false);

    memset(buf, 0, sizeof(ucb_buffer));

    buf->data = (char*)calloc(1, initial_capacity);
    if (!buf->data)
        return false;

    buf->capacity       = initial_capacity;
    buf->used           = 0;
    buf->_impl_resize   = ucb_buffer_resize_malloc;
    buf->_impl_free     = ucb_buffer_free_malloc;
    buf->_impl_transfer = ucb_buffer_transfer_malloc;
    return true;
}
