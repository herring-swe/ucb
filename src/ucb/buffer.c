/**
 * @file buffer.c
 *
 * This file is part of the UCB project
 * - SPDX-FileCopyrightText: © 2026 Åke Svedin <ake@svedin.org>
 * - SPDX-License-Identifier: MIT
 *
 * @brief General buffer interface implementation
 */

#include "ucb/buffer.h"

#include "ucb/error.h"
#include "ucb/memory.h"

#include <assert.h>
#include <errno.h>
#include <math.h>
#include <stdio.h>
#include <string.h>

/* -------------------------------------------------------------------------- */
/*                                Static buffer                               */
/* -------------------------------------------------------------------------- */

ucb_buffer* ucb_buffer_new_static(void* data, size_t size)
{
    ucb_buffer* buf = ucb_calloc_type(1, ucb_buffer);
    if (!ucb_buffer_init_static(buf, data, size))
        ucb_free_null(buf);
    return buf;
}

bool ucb_buffer_init_static(ucb_buffer* buf, void* data, size_t size)
{
    UCB_VERIFY_ARGS(buf && data && size > 0);

    memset(buf, 0, sizeof(ucb_buffer));
    buf->data = (char*)data;
    buf->alloc = size;
    return true;
}

/* -------------------------------------------------------------------------- */
/*                               Dynamic buffer                               */
/* -------------------------------------------------------------------------- */

ucb_buffer* ucb_buffer_new_heap(size_t initial_capacity)
{
    ucb_buffer* buf = ucb_calloc_type(1, ucb_buffer);
    if (!ucb_buffer_init_heap(buf, initial_capacity))
        ucb_free_null(buf);
    return buf;
}

static bool ucb_buffer_resize_heap(ucb_buffer* buf, size_t new_capacity)
{
    assert(buf);
    char* tmp = (char*)ucb_realloc2(buf->data, new_capacity, false);
    if (!tmp)
        return false;

    buf->data = tmp;
    buf->alloc = new_capacity;
    return true;
}

static void ucb_buffer_free_heap(ucb_buffer* buf)
{
    ucb_free(buf->data);
    buf->data = UCB_NULL;
}

static bool ucb_buffer_transfer_heap(ucb_buffer* buf,
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

bool ucb_buffer_init_heap(ucb_buffer* buf, size_t initial_capacity)
{
    UCB_VERIFY_ARGS(buf && initial_capacity > 0);

    memset(buf, 0, sizeof(ucb_buffer));

    buf->data = (char*)ucb_calloc(initial_capacity, 1);
    if (!buf->data)
        return false;

    buf->alloc = initial_capacity;
    buf->size = 0;
    buf->_impl_resize = ucb_buffer_resize_heap;
    buf->_impl_free = ucb_buffer_free_heap;
    buf->_impl_transfer = ucb_buffer_transfer_heap;
    return true;
}

/* -------------------------------------------------------------------------- */
/*                               Generic buffer                               */
/* -------------------------------------------------------------------------- */

void ucb_buffer_release(ucb_buffer* buf)
{
    if (!buf)
        return;

    if (buf->_impl_free)
    {
        buf->_impl_free(buf);
        assert(buf->data == UCB_NULL);
    }
    assert(buf->_impl == UCB_NULL);
}

void ucb_buffer_free(ucb_buffer* buf)
{
    if (!buf)
        return;

    ucb_buffer_release(buf);
    ucb_free(buf);
}

bool ucb_buffer_can_transfer(ucb_buffer* buf)
{
    return buf && buf->_impl_transfer;
}

bool ucb_buffer_transfer(ucb_buffer* buf,
                         void** out_data,
                         size_t* out_size,
                         size_t* out_capacity,
                         const ucb_error** perr)
{
    UCB_VERIFY_ARGS(buf && out_data);
    UCB_VERIFY(buf->_impl_transfer, UCB_ERROR_BUFFER, "Buffer does not support transfer");

    bool success = buf->_impl_transfer(buf, out_data, out_size, out_capacity, perr);
    if (success)
    {
        memset(buf, 0, sizeof(ucb_buffer));
    }
    return success;
}

bool ucb_buffer_can_resize(ucb_buffer* buf)
{
    return buf && buf->_impl_resize;
}

bool ucb_buffer_resize(ucb_buffer* buf, size_t new_capacity)
{
    UCB_VERIFY_ARGS(buf);
    UCB_VERIFY(buf->_impl_resize, UCB_ERROR_BUFFER, "Buffer does not support resize");

    bool success = buf->alloc == new_capacity || buf->_impl_resize(buf, new_capacity);
    if (success)
    {
        assert(buf->alloc == new_capacity);
        if (buf->size > buf->alloc)
            buf->size = buf->alloc;
    }
    return success;
}

bool ucb_buffer_grow(ucb_buffer* buf, size_t inc_capacity)
{
    UCB_VERIFY_ARGS(buf);
    if (buf->grow_func)
        inc_capacity = buf->grow_func(buf, inc_capacity) - buf->alloc;
    return ucb_buffer_resize(buf, buf->alloc + inc_capacity);
}

bool ucb_buffer_ensure(ucb_buffer* buf, size_t size)
{
    UCB_VERIFY_ARGS(buf);
    if (buf->size + size <= buf->alloc)
        return true;
    return ucb_buffer_grow(buf, buf->size + size - buf->alloc);
}

void ucb_buffer_read(ucb_buffer* buf, void* out_data, size_t size, size_t offset)
{
    UCB_VERIFY_ARGS(buf && out_data);
    UCB_VERIFY(offset + size <= buf->size, UCB_ERROR_OUT_OF_BOUNDS, "Read out of bounds");

    if (size > 0)
    {
        memcpy(out_data, buf->data + offset, size);
    }
}

bool ucb_buffer_push(ucb_buffer* buf, const void* data, size_t size)
{
    UCB_VERIFY_ARGS(buf && data);
    if (size > 0)
    {
        if (buf->size + size > buf->alloc)
        {
            if (!ucb_buffer_grow(buf, buf->size + size - buf->alloc))
                return false;
        }

        memcpy(buf->data + buf->size, data, size);
        buf->size += size;
    }
    return true;
}

int ucb_buffer_push_format(ucb_buffer* buf, const char* fmt, ...)
{
    va_list args;
    va_start(args, fmt);
    int ret = ucb_buffer_push_formatv(buf, fmt, args);
    va_end(args);
    return ret;
}

int ucb_buffer_push_formatv(ucb_buffer* buf, const char* fmt, va_list args)
{
    UCB_VERIFY_ARGS(buf && fmt);

    // Measure using a copy; vsnprintf consumes the va_list on some ABIs.
    va_list args_copy;
    va_copy(args_copy, args);
    int size = vsnprintf(UCB_NULL, 0, fmt, args_copy);
    va_end(args_copy);

    if (size < 0)
    {
        UCB_REPORT_ERRNO(errno, "Failed to format string for buffer");
    }
    else if (size == 0)
    {
        ucb_buffer_ensure(buf, 1);
        buf->data[buf->size++] = '\0';
    }
    if (size > 0)
    {
        if (!ucb_buffer_ensure(buf, size + 1))
            return -1;
        vsnprintf((char*)buf->data + buf->size, size + 1, fmt, args);
        buf->size += size + 1;
    }
    return size;
}

void ucb_buffer_pop(ucb_buffer* buf, void* out_data, size_t size)
{
    UCB_VERIFY_ARGS(buf && out_data);
    UCB_VERIFY(size <= buf->size, UCB_ERROR_OUT_OF_BOUNDS, "Not enough data in buffer");

    if (size > 0)
    {
        if (out_data)
            memcpy(out_data, buf->data + buf->size - size, size);
        buf->size -= size;
    }
}

void ucb_buffer_clear(ucb_buffer* buf)
{
    UCB_VERIFY_ARGS(buf);
    buf->size = 0;
}

bool ucb_buffer_fit(ucb_buffer* buf)
{
    UCB_VERIFY_ARGS(buf);
    if (buf->size == buf->alloc)
        return true;
    return ucb_buffer_resize(buf, buf->size);
}
