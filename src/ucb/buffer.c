/**
 * This file is part of the UCB project
 * SPDX-FileCopyrightText: © 2025 Åke Svedin <ake@svedin.org>
 * SPDX-License-Identifier: MIT
 *
 * @brief Buffer type implementation
 */

#include "ucb/buffer.h"

#include "ucb/errcodes.h"
#include "ucb/memory.h"

#include <assert.h>
#include <math.h>
#include <string.h>

/* -------------------------------------------------------------------------- */
/*                                Static buffer                               */
/* -------------------------------------------------------------------------- */

ucb_buffer_t* ucb_buffer_new_static(void* data, size_t size)
{
    ucb_buffer_t* buf = ucb_calloc_type(1, ucb_buffer_t);
    if (ucb_buffer_init_static(buf, data, size) != UCB_OK)
        ucb_free_null(buf);
    return buf;
}

ucb_error_t ucb_buffer_init_static(ucb_buffer_t* buf, void* data, size_t size)
{
    if (!buf || !data || size == 0)
        return UCB_ERROR_INVALID_ARG;
    if (buf->data)
        return UCB_ERROR_BUFFER;

    memset(buf, 0, sizeof(ucb_buffer_t));
    buf->data     = (char*)data;
    buf->capacity = size;
    return UCB_OK;
}

/* -------------------------------------------------------------------------- */
/*                               Dynamic buffer                               */
/* -------------------------------------------------------------------------- */

ucb_buffer_t* ucb_buffer_new_heap(size_t initial_capacity)
{
    ucb_buffer_t* buf = ucb_calloc_type(1, ucb_buffer_t);
    if (ucb_buffer_init_heap(buf, initial_capacity) != UCB_OK)
        ucb_free_null(buf);
    return buf;
}

static ucb_error_t ucb_buffer_resize_heap(ucb_buffer_t* buf, size_t new_capacity)
{
    char* tmp = (char*)ucb_realloc2(buf->data, new_capacity, false);
    if (!tmp)
        return UCB_ERROR_OUT_OF_MEMORY;

    buf->data     = tmp;
    buf->capacity = new_capacity;
    return UCB_OK;
}

static void ucb_buffer_free_heap(ucb_buffer_t* buf)
{
    ucb_free(buf->data);
    buf->data = UCB_NULL;
}

static ucb_error_t ucb_buffer_transfer_heap(ucb_buffer_t* buf, void** out_data, size_t* out_used,
                                            size_t* out_capacity)
{
    *out_data = buf->data;
    if (out_used)
        *out_used = buf->used;
    if (out_capacity)
        *out_capacity = buf->capacity;
    return UCB_OK;
}

ucb_error_t ucb_buffer_init_heap(ucb_buffer_t* buf, size_t initial_capacity)
{
    if (!buf || initial_capacity == 0)
        return UCB_ERROR_INVALID_ARG;
    if (buf->data)
        return UCB_ERROR_BUFFER;

    memset(buf, 0, sizeof(ucb_buffer_t));

    buf->data = (char*)ucb_calloc(initial_capacity, 1);
    if (!buf->data)
        return UCB_ERROR_OUT_OF_MEMORY;

    buf->capacity       = initial_capacity;
    buf->used           = 0;
    buf->_impl_resize   = ucb_buffer_resize_heap;
    buf->_impl_free     = ucb_buffer_free_heap;
    buf->_impl_transfer = ucb_buffer_transfer_heap;
    return UCB_OK;
}

/* -------------------------------------------------------------------------- */
/*                               Generic buffer                               */
/* -------------------------------------------------------------------------- */

void ucb_buffer_release(ucb_buffer_t* buf)
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

void ucb_buffer_free(ucb_buffer_t* buf)
{
    if (!buf)
        return;

    ucb_buffer_release(buf);
    ucb_free(buf);
}

bool ucb_buffer_can_transfer(ucb_buffer_t* buf)
{
    return buf && buf->_impl_transfer;
}

ucb_error_t ucb_buffer_transfer(ucb_buffer_t* buf, void** out_data, size_t* out_size,
                                size_t* out_capacity)
{
    if (!buf || !out_data)
        return UCB_ERROR_INVALID_ARG;
    if (!buf->_impl_transfer)
        return UCB_ERROR_BUFFER;

    ucb_error_t ret = buf->_impl_transfer(buf, out_data, out_size, out_capacity);
    if (ret == UCB_OK)
    {
        memset(buf, 0, sizeof(ucb_buffer_t));
    }
    return ret;
}

bool ucb_buffer_can_resize(ucb_buffer_t* buf)
{
    return buf && buf->_impl_resize;
}

ucb_error_t ucb_buffer_resize(ucb_buffer_t* buf, size_t new_capacity)
{
    if (!buf)
        return UCB_ERROR_INVALID_ARG;
    if (buf->capacity == new_capacity)
        return UCB_OK;
    if (!buf->_impl_resize)
        return UCB_ERROR_BUFFER;

    ucb_error_t ret = buf->_impl_resize(buf, new_capacity);
    if (ret != UCB_OK)
        return ret;
    assert(buf->capacity == new_capacity);
    if (buf->used > buf->capacity)
        buf->used = buf->capacity;
    return ret;
}

ucb_error_t ucb_buffer_grow(ucb_buffer_t* buf, size_t inc_capacity)
{
    if (buf == UCB_NULL)
        return UCB_ERROR_INVALID_ARG;
    if (buf->grow_func)
        inc_capacity = buf->grow_func(buf, inc_capacity);
    return ucb_buffer_resize(buf, buf->capacity + inc_capacity);
}

ucb_error_t ucb_buffer_ensure(ucb_buffer_t* buf, size_t size)
{
    if (buf == UCB_NULL)
        return UCB_ERROR_INVALID_ARG;
    if (buf->used + size <= buf->capacity)
        return UCB_OK;
    return ucb_buffer_grow(buf, buf->used + size - buf->capacity);
}

ucb_error_t ucb_buffer_read(ucb_buffer_t buf, void* out_data, size_t size, size_t offset);

ucb_error_t ucb_buffer_push(ucb_buffer_t* buf, const void* data, size_t size)
{
    if (buf == UCB_NULL || data == UCB_NULL)
        return UCB_ERROR_INVALID_ARG;
    if (buf->used + size > buf->capacity)
    {
        ucb_error_t err = ucb_buffer_grow(buf, buf->used + size - buf->capacity);
        if (err != UCB_OK)
            return err;
    }

    memcpy(buf->data + buf->used, data, size);
    buf->used += size;
    return UCB_OK;
}

ucb_error_t ucb_buffer_pop(ucb_buffer_t* buf, void* out_data, size_t size)
{
    if (buf == UCB_NULL || out_data == UCB_NULL)
        return UCB_ERROR_INVALID_ARG;

    if (buf->used < size)
        return UCB_ERROR_NOT_FOUND;

    if (out_data)
        memcpy(out_data, buf->data + buf->used - size, size);
    buf->used -= size;

    return UCB_OK;
}

ucb_error_t ucb_buffer_clear(ucb_buffer_t* buf)
{
    if (buf == UCB_NULL)
        return UCB_ERROR_INVALID_ARG;

    buf->used = 0;
    return UCB_OK;
}

ucb_error_t ucb_buffer_fit(ucb_buffer_t* buf)
{
    if (buf == UCB_NULL)
        return UCB_ERROR_INVALID_ARG;
    if (buf->used == buf->capacity)
        return UCB_OK;
    return ucb_buffer_resize(buf, buf->used);
}

/* -------------------------------------------------------------------------- */
/*                                 Buffer view                                */
/* -------------------------------------------------------------------------- */

ucb_error_t ucb_buffer_view_init(ucb_buffer_view_t* view, ucb_buffer_t* buf, size_t offset,
                                 size_t element_size)
{
    if (!view || !buf)
        return UCB_ERROR_INVALID_ARG;

    if (offset < buf->used)
        return UCB_ERROR_INVALID_ARG;

    // TODO Should register view with buffer

    view->buf          = buf;
    view->offset       = offset;
    view->element_size = element_size;
    view->count        = 0;
    return UCB_OK;
}

void ucb_buffer_view_release(ucb_buffer_view_t* view)
{
    if (!view)
        return;
    memset(view, 0, sizeof(ucb_buffer_view_t));
}

void* ucb_buffer_view_get_data(ucb_buffer_view_t* view)
{
    if (!view || !view->buf)
        return NULL;

    return view->buf->data + view->offset;
}

bool ucb_buffer_view_is_full(ucb_buffer_view_t* view)
{
    if (!view || !view->buf)
        return true;

    size_t capacity = (view->buf->capacity - view->offset) / view->element_size;
    return view->count >= capacity;
}

bool ucb_buffer_view_can_resize(ucb_buffer_view_t* view)
{
    if (!view || !view->buf)
        return false;

    return ucb_buffer_can_resize(view->buf);
}

void ucb_buffer_view_clear(ucb_buffer_view_t* view)
{
    if (!view || !view->buf)
        return;

    view->count = 0;
}

void ucb_buffer_view_set_count(ucb_buffer_view_t* view, size_t count)
{
    if (!view || !view->buf)
        return;

    view->count = count;
}

size_t ucb_buffer_view_get_count(ucb_buffer_view_t* view)
{
    if (!view || !view->buf)
        return 0;

    return view->count;
}

size_t ucb_buffer_view_get_capacity(ucb_buffer_view_t* view)
{
    if (!view || !view->buf)
        return 0;

    return (view->buf->capacity - view->offset) / view->element_size;
}

ucb_error_t ucb_buffer_view_grow(ucb_buffer_view_t* view, size_t inc_count)
{
    if (!view || !view->buf)
        return UCB_ERROR_INVALID_ARG;

    return ucb_buffer_grow(view->buf, inc_count * view->element_size);
}

ucb_error_t ucb_buffer_view_ensure(ucb_buffer_view_t* view, size_t count)
{
    if (!view || !view->buf)
        return UCB_ERROR_INVALID_ARG;

    // Take offset into consideration
    size_t bytes_total = (count + view->count) * view->element_size;
    size_t bytes_free  = view->buf->capacity - view->offset;
    if (bytes_free < bytes_total)
        return ucb_buffer_grow(view->buf, bytes_total - bytes_free);

    return UCB_OK;
}

ucb_error_t ucb_buffer_view_push(ucb_buffer_view_t* view, const void* data, size_t count)
{
    if (!view || !view->buf)
        return UCB_ERROR_INVALID_ARG;

    size_t bytes_used = view->count * view->element_size;
    size_t bytes_need = count * view->element_size;
    size_t bytes_free = view->buf->capacity - view->offset;
    if (bytes_free < bytes_used + bytes_need)
    {
        ucb_error_t err = ucb_buffer_grow(view->buf, bytes_used + bytes_need - bytes_free);
        if (err != UCB_OK)
            return err;
    }

    memcpy(view->buf->data + view->offset + bytes_used, data, bytes_need);
    view->count += count;
    return UCB_OK;
}

ucb_error_t ucb_buffer_view_pop(ucb_buffer_view_t* view, void* out_data, size_t count)
{
    if (!view || !view->buf)
        return UCB_ERROR_INVALID_ARG;

    if (view->count < count)
        return UCB_ERROR_NOT_FOUND;

    if (out_data)
    {
        size_t size   = count * view->element_size;
        size_t offset = view->offset + view->count * view->element_size - size;
        memcpy(out_data, view->buf->data + offset, size);
    }
    view->count -= count;
    return UCB_OK;
}

/* -------------------------------------------------------------------------- */
/*                               Mixed interface                              */
/* -------------------------------------------------------------------------- */

// void test(ucb_buffer_t* buf)
// {
//     ucb_bufobj_t obj = ucb_as_bufobj(buf);
//     ucb_buf_release(&obj);
// }
