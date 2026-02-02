/**
 * @file buffer.h
 * This file is part of the UCB project
 * - SPDX-FileCopyrightText: © 2026 Åke Svedin <ake@svedin.org>
 * - SPDX-License-Identifier: MIT
 *
 * @brief General buffer interface
 */

#ifndef UCB_BUFFER_H
#define UCB_BUFFER_H

#include "defines.h"
#include "error.h"
#include "export.h"

#include <stdalign.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

// Forward declare, declared below
struct ucb_buffer;
typedef struct ucb_buffer ucb_buffer;

/**
 * @brief Buffer implementation specific resize function
 *
 * Resize function must resize buffer to new_capacity and set
 * buffer->data and buffer->size to the new values.
 *
 * @returns true on success
 * @see ucb_buffer_resize
 */
typedef bool (*ucb_buffer_resize_func)(ucb_buffer* buf, size_t new_capacity);

/**
 * @brief Buffer implementation specific free function
 *
 * Free function must free buffer and implementation specific data.
 * Must set data and impl to UCB_NULL
 *
 * @see uch_buffer_free
 */
typedef void (*ucb_buffer_free_func)(ucb_buffer* buf);

/**
 * @brief Buffer implementation specific transfer function
 *
 * Transfer function must transfer the buffer data to out_data and set out_used and out_capacity
 * to the new values.
 *
 * @see ucb_buffer_transfer
 */
typedef bool (*ucb_buffer_transfer_func)(ucb_buffer* buf, void** out_data, size_t* out_used,
                                         size_t* out_capacity, const ucb_error** error);

/**
 * @brief User-defined grow function
 * Grow function will determine how many bytes to expand the buffer with to accomodate
 * size_needed bytes. This is not tied to a specific buffer type and can be swapped
 * by the user as-needed.
 *
 * @param buf The buffer to grow
 * @param size_needed The number of bytes needed to be added to the buffer
 * @return The new (total) capacity of the buffer to use with ucb_buffer_resize
 */
typedef size_t (*ucb_buffer_grow_func)(ucb_buffer* buf, size_t size_needed);

/**
 * @struct ucb_buffer
 * @brief Byte buffer type
 *
 * A byte buffer for storing sequential data.
 * It can have multiple implementations.
 *
 * Allocate on heap with ucb_buffer_new_<type>
 * or initiate on stack with ucb_buffer_init_<type>
 *
 * Only resize or free the buffer using the functions provided in
 * the ucb_buffer namespace.
 *
 * The buffer is not thread safe.
 *
 * @warning
 * All of the possible resize functions may invalidate pointers
 * to the current memory. For this reason, it is recommended to
 * only use offsets.
 *
 * Unsafe usage:
 * @code
 * ucb_buffer* buf = ucb_buffer_new_heap(1024);
 * char* data = buf->data;
 * ucb_buffer_resize(buf, 2048);
 * data[0] = 'a'; // This is now invalid.
 * @endcode
 *
 * Safe usage:
 * @code
 * ucb_buffer* buf = ucb_buffer_new_heap(1024);
 * size_t offset = 0;
 * ucb_buffer_resize(buf, 2048);
 * buf->data[offset] = 'a';
 * @endcode
 */
struct ucb_buffer
{
    char* data;                     ///< Memory
    size_t size;                    ///< Number used bytes in memory
    size_t alloc;                   ///< Number of bytes allocated
    ucb_buffer_grow_func grow_func; ///< If UCB_NULL, always allocate requested size as-is.

    // Implementation specific data must never be called by user
    void* _impl;                             ///< Reserved for implementation specific data.
    ucb_buffer_resize_func _impl_resize;     ///< If UCB_NULL, resize not allowed.
    ucb_buffer_free_func _impl_free;         ///< If UCB_NULL, free not needed (static buffer).
    ucb_buffer_transfer_func _impl_transfer; ///< If UCB_NULL, release not allowed.
};

/**
 * @struct ucb_buffer_view
 * @brief A view into a buffer.
 */
typedef struct ucb_buffer_view
{
    ucb_buffer* buf;
    size_t offset;       ///< Start offset in bytes to parent buf
    size_t element_size; ///< Element size in bytes
    size_t count;        ///< Number of elements
} ucb_buffer_view;

/* -------------------------------------------------------------------------- */
/*                               Regular buffer                               */
/* -------------------------------------------------------------------------- */

/**
 * Allocates and returns a new initiated static buffer
 * Free buffer and it's data with ucb_buffer_free.
 * @see ucb_buffer_init_static
 * @return a pointer to the new buffer or UCB_NULL on any error.
 */
UCB_API ucb_buffer* ucb_buffer_new_static(void* data, size_t size);

/**
 * Initates a static buffer.
 * The buffers capacity will be set to size, used to 0.
 * Free data with ucb_buffer_release.
 * @param buf pointer to a zeroed buffer struct
 * @param data pointer to the data to use as buffer, must be non-null
 * @param size the size of the data in bytes, must be non-zero
 */
UCB_API bool ucb_buffer_init_static(ucb_buffer* buf, void* data, size_t size);

/**
 * Allocates and returns a new initiated heap buffer
 * Free buffer and it's data with ucb_buffer_free.
 * @see ucb_buffer_init_heap
 * @return a pointer to the new buffer or UCB_NULL on any error.
 */
UCB_API ucb_buffer* ucb_buffer_new_heap(size_t initial_capacity);

/**
 * Initiates a heap buffer.
 * buffer->data must be UCB_NULL or it will fail.
 * @param buf pointer to a zeroed buffer struct
 * @param initial_capacity initial capacity in bytes, must be non-zero
 */
UCB_API bool ucb_buffer_init_heap(ucb_buffer* buf, size_t initial_capacity);

/**
 * @brief Free all resources within buffer
 * The buffer struct itself is not free'd
 */
UCB_API void ucb_buffer_release(ucb_buffer* buf);

/**
 * @brief Free the buffer and all of its resources.
 * For buffers allocated with uch_buffer_new_*
 * @param buf the buffer to free
 */
UCB_API void ucb_buffer_free(ucb_buffer* buf);

/**
 * @brief Check if the buffer can be transferred
 */
UCB_API bool ucb_buffer_can_transfer(ucb_buffer* buf);

/**
 * Release the buffer from being managed, if allowed.
 * The buffer will then become invalid. Either call ucb_free_buffer or
 * reinitialize it.
 */
UCB_API bool ucb_buffer_transfer(ucb_buffer* buf, void** out_data, size_t* out_size,
                                 size_t* out_capacity, const ucb_error** perr);

/**
 * @brief Check if the buffer can be resized
 */
UCB_API bool ucb_buffer_can_resize(ucb_buffer* buf);

/**
 * Resize the buffer capacity to given bytes. This may be an increase or reduction.
 * Used will be set to capacity, if capacity is reduced to be smaller than used.
 * Note, that any pointers to the buffers data are invalidated by this call.
 *
 * The operation either fails if the buffer is not resizable or the underlying
 * resize failed. Both errors are reported to the error handler.
 * @param buf the buffer
 * @param new_capacity new capacity in bytes
 * @return true on success or no change needed.
 */
UCB_API bool ucb_buffer_resize(ucb_buffer* buf, size_t new_capacity);

/**
 * Grow the buffer capacity by given bytes. This will always try to allocate memory.
 * It may use a user-defined grow function if one is set, otherwise
 * size will be added to the current size.
 * Calls ucb_buffer_resize to do the actual resize.
 * @param buf the buffer
 * @param inc_capacity number of bytes to grow from current capacity
 * @return true on success
 * @see ucb_buffer_grow_func
 * @see ucb_buffer_resize
 */
UCB_API bool ucb_buffer_grow(ucb_buffer* buf, size_t inc_capacity);

/**
 * Ensure that a certain amount of bytes are available as free space in the buffer.
 * If the buffer capacity is too small, it will be grown to accomodate the request.
 * Calls ucb_buffer_grow if needed
 * @param buf the buffer
 * @param size size to ensure
 * @return true on success
 * @see ucb_buffer_grow
 */
UCB_API bool ucb_buffer_ensure(ucb_buffer* buf, size_t size);

/**
 * Read from the buffer at a given offset.
 * @param buf the buffer
 * @param out_data pointer to data that will be set
 * @param size number of bytes to read
 * @param offset offset to read from
 */
UCB_API void ucb_buffer_read(ucb_buffer* buf, void* out_data, size_t size, size_t offset);

/**
 * Push data to the end of the buffer, growing the buffer if needed.
 * Calls ucb_buffer_grow if needed
 * @param buf the buffer
 * @param data data to push, must be at least size bytes
 * @param size number of bytes to push
 * @return true on success, false if buffer is full and cannot grow
 */
UCB_API bool ucb_buffer_push(ucb_buffer* buf, const void* data, size_t size);

/**
 * Formats and appends string to buffer. The buffer may grow as needed.
 *
 * @param buf the buffer
 * @param fmt a format string
 * @param ... arguments to format
 * @return length of string written, excluding null terminator, or -1 on failure
 */
UCB_API int ucb_buffer_push_format(ucb_buffer* buf, const char* fmt, ...);

/**
 * Formats and appends string to buffer. The buffer may grow as needed.
 * @param buf the buffer
 * @param fmt a format string
 * @param args arguments to format
 * @return length of string written, excluding null terminator, or -1 on failure
 */
UCB_API int ucb_buffer_push_formatv(ucb_buffer* buf, const char* fmt, va_list args);

/**
 * Copies the last size data from the buffer and reduce the buffers used size.
 * Does not shrink the buffers capacity or modify it's data
 * @param buf the buffer
 * @param out_data pointer to data that will be set
 * @param size number of bytes to read
 */
UCB_API void ucb_buffer_pop(ucb_buffer* buf, void* out_data, size_t size);

/**
 * Marks the buffer as unused but does not shrink the buffer capacity.
 */
UCB_API void ucb_buffer_clear(ucb_buffer* buf);

/**
 * Resize the capacity of the buffer to the size of the data
 */
UCB_API bool ucb_buffer_fit(ucb_buffer* buf);

/* -------------------------------------------------------------------------- */
/*                               Grow functions                               */
/* -------------------------------------------------------------------------- */

static inline size_t ucb_buffer_grow_double(ucb_buffer* buf, size_t size_needed)
{
    size_t cap = buf->alloc * 2;
    while (size_needed > cap - buf->size)
        cap *= 2;
    return cap;
}

/* -------------------------------------------------------------------------- */
/*                                Buffer views                                */
/* -------------------------------------------------------------------------- */

/**
 * Wraps a buffer into a mutable view, which can have an optional offset and element size.
 *
 * The view does not own the buffer. As such is cannot free, transfer or directly
 * resize it. However it can ensure that the buffer has enough capacity and
 * call the parent grow function as required.
 *
 * It's safe to modify the original buffer as long as:
 * - The buffer remains valid (no transfer or free)
 * - The buffer capacity is not reduced below the view's offset + count
 * - The view and the buffer does not operate on overlapping memory
 *
 * It's up to the user to ensure data safety.
 * The view is not thread safe.
 */

ucb_ecode ucb_buffer_view_init(ucb_buffer_view* view, ucb_buffer* buf, size_t offset,
                               size_t element_size);
void ucb_buffer_view_release(ucb_buffer_view* view);

void* ucb_buffer_view_get_data(ucb_buffer_view* view);

bool ucb_buffer_view_is_full(ucb_buffer_view* view);
bool ucb_buffer_view_can_resize(ucb_buffer_view* view);

void ucb_buffer_view_clear(ucb_buffer_view* view);
void ucb_buffer_view_set_count(ucb_buffer_view* view, size_t count);
size_t ucb_buffer_view_get_count(ucb_buffer_view* view);
size_t ucb_buffer_view_get_capacity(ucb_buffer_view* view);

ucb_ecode ucb_buffer_view_grow(ucb_buffer_view* view, size_t inc_count);
ucb_ecode ucb_buffer_view_ensure(ucb_buffer_view* view, size_t count);

ucb_ecode ucb_buffer_view_push(ucb_buffer_view* view, const void* data, size_t count);
void ucb_buffer_view_pop(ucb_buffer_view* view, void* out_data, size_t count);

#define ucb_buffer_view_init_type(view, buf, type) ucb_buffer_view_init(view, buf, sizeof(type))

/* -------------------------------------------------------------------------- */
/*                              Generic interface                             */
/* -------------------------------------------------------------------------- */

#if 0
typedef struct ucb_bufobj
{
    union
    {
        ucb_buffer* buf;
        ucb_buffer_view* view;
    };
    bool is_view;
} ucb_bufobj_t;

static inline ucb_bufobj_t ucb_buffer_as_obj(ucb_buffer* buf)
{
    return (ucb_bufobj_t){.buf = buf, .is_view = false};
}

static inline ucb_bufobj_t ucb_buffer_view_as_obj(ucb_buffer_view* view)
{
    return (ucb_bufobj_t){.view = view, .is_view = true};
}

#define ucb_as_bufobj(obj) \
    _Generic((obj), ucb_buffer*: ucb_buffer_as_obj, ucb_buffer_view*:
    ucb_buffer_view_as_obj)( \
        obj)

static inline void ucb_bufobj_release(ucb_bufobj_t* obj)
{
    if (obj->is_view)
    {
        ucb_buffer_view_release(obj->view);
        obj->view = UCB_NULL;
    }
    else
    {
        ucb_buffer_release(obj->buf);
        obj->buf = UCB_NULL;
    }
}

#define ucb_buf_release(obj)                       \
    _Generic(obj,                                  \
        ucb_buffer*: ucb_buffer_release,           \
        ucb_buffer_view*: ucb_buffer_view_release, \
        ucb_bufobj_t*: ucb_bufobj_release)(obj)

static inline bool ucb_buf_can_transfer(ucb_bufobj_t* obj)
{
    if (obj->is_view)
        return false;
    return ucb_buffer_can_transfer(obj->buf);
}

#define ucb_buf_can_transfer(obj)                  \
    _Generic(obj,                                  \
        ucb_buffer*: ucb_buffer_can_transfer,      \
        ucb_buffer_view*: ucb_buffer_can_transfer, \
        ucb_bufobj_t*: ucb_buf_can_transfer)(obj)
#endif

#endif // UCB_BUFFER_H
