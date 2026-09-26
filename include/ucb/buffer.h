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

#include <ucb/defines.h>
#include <ucb/error.h>
#include <ucb/export.h>

#include <stdalign.h>
#include <stdarg.h>
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
 * @see ucb_buffer_free
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
typedef bool (*ucb_buffer_transfer_func)(ucb_buffer* buf,
                                         void** out_data,
                                         size_t* out_used,
                                         size_t* out_capacity,
                                         const ucb_error** error);

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

/* -------------------------------------------------------------------------- */
/*                               Regular buffer                               */
/* -------------------------------------------------------------------------- */

/**
 * Allocates and returns a new initiated static buffer.
 *
 * The buffer borrows @p data; it never owns or frees it. Free the buffer
 * struct with ucb_buffer_free (which only frees the struct). Scrub the used
 * bytes with ucb_buffer_zero before release if needed.
 * @see ucb_buffer_init_static
 * @return a pointer to the new buffer or UCB_NULL on any error.
 */
UCB_API ucb_buffer* ucb_buffer_new_static(void* data, size_t size);

/**
 * Initates a static buffer.
 * The buffers capacity will be set to size, used to 0.
 * @p data is borrowed: ucb_buffer_release and ucb_buffer_free never free it.
 * @param buf pointer to a zeroed buffer struct
 * @param data pointer to the data to use as buffer, must be non-null
 * @param size the size of the data in bytes, must be non-zero
 */
UCB_API bool ucb_buffer_init_static(ucb_buffer* buf, void* data, size_t size);

/**
 * Allocates and returns a new initiated heap buffer
 * Free the buffer and its data with ucb_buffer_free.
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
 * @brief Free all owned resources within buffer
 *
 * The buffer struct itself is not freed. Owned data is freed; borrowed (static)
 * data is not. The struct is left non-zeroed, so it must be re-initialized
 * before reuse. Call ucb_buffer_zero first to scrub the used bytes.
 *
 * @param buf the buffer to release
 */
UCB_API void ucb_buffer_release(ucb_buffer* buf);

/**
 * @brief Free the buffer and all of its resources.
 * For buffers allocated with ucb_buffer_new_*
 * @param buf the buffer to free
 */
UCB_API void ucb_buffer_free(ucb_buffer* buf);

/**
 * @brief Zero the used bytes of the buffer
 *
 * Sets @c data[0..size) to zero and leaves @c size, @c alloc and the
 * implementation intact. Intended to scrub sensitive contents before
 * ucb_buffer_release/ucb_buffer_free, which do not scrub on their own.
 * A no-op if the buffer is UCB_NULL, has UCB_NULL data or is empty.
 *
 * @warning A plain memset may be optimized away by the compiler. This call is
 * suitable for determinism, not for guaranteed memory scrubbing.
 *
 * @param buf the buffer
 */
UCB_API void ucb_buffer_zero(ucb_buffer* buf);

/**
 * @brief Check if the buffer can be transferred
 * @param buf the buffer to check
 * @return true if the buffer supports ucb_buffer_transfer
 */
UCB_API bool ucb_buffer_can_transfer(ucb_buffer* buf);

/**
 * Release the buffer from being managed, if allowed.
 * The buffer will then become invalid. Either call ucb_buffer_free or
 * reinitialize it.
 * @param buf the buffer to transfer
 * @param out_data where to store the transferred data pointer
 * @param out_size where to store the transferred used size, may be UCB_NULL
 * @param out_capacity where to store the transferred capacity, may be UCB_NULL
 * @param perr optional location to store the error on failure
 * @return true on success
 */
UCB_API bool ucb_buffer_transfer(ucb_buffer* buf,
                                 void** out_data,
                                 size_t* out_size,
                                 size_t* out_capacity,
                                 const ucb_error** perr);

/**
 * @brief Check if the buffer can be resized
 * @param buf the buffer to check
 * @return true if the buffer supports ucb_buffer_resize
 */
UCB_API bool ucb_buffer_can_resize(ucb_buffer* buf);

/**
 * @brief Check whether @p size additional free bytes can be ensured
 *
 * A non-aborting pre-check for ucb_buffer_grow/ucb_buffer_ensure/
 * ucb_buffer_push. Returns true when @p size bytes already fit, or when the
 * buffer supports resize and the resulting total cannot overflow @c size_t.
 *
 * This only reports that the operation is permitted and within range; it does
 * not predict whether the underlying allocation will succeed.
 *
 * @param buf the buffer, may be UCB_NULL
 * @param size number of additional free bytes required
 * @return true if the request can be attempted without aborting on a contract
 *         violation, false otherwise
 */
UCB_API bool ucb_buffer_can_ensure(const ucb_buffer* buf, size_t size);

/**
 * Resize the buffer capacity to given bytes. This may be an increase or reduction.
 * Used will be set to capacity, if capacity is reduced to be smaller than used.
 * A @p new_capacity of 0 is allowed: the owned block is freed and @c data
 * becomes UCB_NULL, @c alloc and @c size become 0. The buffer remains
 * resizable and can grow again.
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
 * If a user-defined grow function is set, it is called with the requested
 * additional bytes and must return the new total capacity of the buffer; a
 * result smaller than the current capacity is clamped and never shrinks the
 * buffer. Otherwise @p inc_capacity is added to the current capacity.
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
 * Calls ucb_buffer_grow if needed. Growth that would overflow @c size_t is a
 * contract violation and aborts; use ucb_buffer_can_ensure to check first.
 * @param buf the buffer
 * @param size size to ensure
 * @return true on success
 * @see ucb_buffer_grow
 * @see ucb_buffer_can_ensure
 */
UCB_API bool ucb_buffer_ensure(ucb_buffer* buf, size_t size);

/**
 * Read from the buffer at a given offset.
 * Out-of-bounds reads are a contract violation and abort.
 * @param buf the buffer
 * @param out_data pointer to data that will be set
 * @param size number of bytes to read
 * @param offset offset to read from
 */
UCB_API void ucb_buffer_read(ucb_buffer* buf, void* out_data, size_t size, size_t offset);

/**
 * Push data to the end of the buffer, growing the buffer if needed.
 * Calls ucb_buffer_grow if needed. @p buf and @p data must be non-NULL even
 * when @p size is 0; a zero-size push is a successful no-op.
 * @param buf the buffer
 * @param data data to push, must be at least size bytes
 * @param size number of bytes to push
 * @return true on success, false if buffer is full and cannot grow
 * @see ucb_buffer_can_ensure
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
 * Does not shrink the buffers capacity or modify it's data. @p out_data must be
 * non-NULL even when @p size is 0.
 * @param buf the buffer
 * @param out_data pointer to data that will be set
 * @param size number of bytes to pop
 */
UCB_API void ucb_buffer_pop(ucb_buffer* buf, void* out_data, size_t size);

/**
 * Marks the buffer as unused but does not shrink the buffer capacity.
 * @param buf the buffer
 */
UCB_API void ucb_buffer_clear(ucb_buffer* buf);

/**
 * Resize the capacity of the buffer to the size of the data.
 * An empty buffer is resized to capacity 0.
 * @param buf the buffer
 * @return true on success
 */
UCB_API bool ucb_buffer_fit(ucb_buffer* buf);

/* -------------------------------------------------------------------------- */
/*                               Grow functions                               */
/* -------------------------------------------------------------------------- */

/**
 * @brief Opt-in grow function that doubles the capacity as needed.
 *
 * Not the default: heap buffers start with a UCB_NULL grow_func and grow to the
 * exact requested capacity. Assign this to @c buf->grow_func to opt in.
 * Handles a zero-capacity buffer (starts from 1) and clamps at @c SIZE_MAX
 * instead of overflowing.
 *
 * @param buf the buffer to grow
 * @param size_needed number of additional bytes required
 * @return the new total capacity for ucb_buffer_resize
 */
static inline size_t ucb_buffer_grow_double(ucb_buffer* buf, size_t size_needed)
{
    size_t cap = buf->alloc;
    if (cap == 0)
        cap = 1;
    else if (cap <= SIZE_MAX / 2)
        cap *= 2;
    else
        cap = SIZE_MAX;

    while (size_needed > cap - buf->size)
    {
        if (cap > SIZE_MAX / 2)
            return SIZE_MAX;
        cap *= 2;
    }
    return cap;
}

#endif // UCB_BUFFER_H
