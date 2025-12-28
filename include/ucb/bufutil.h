/**
 * This file is part of the UCB project
 * SPDX-FileCopyrightText: © 2025 Åke Svedin <ake@svedin.org>
 * SPDX-License-Identifier: MIT
 *
 * @brief Buffer utility functions
 */

#ifndef UCB_BUFUTIL_H
#define UCB_BUFUTIL_H

#include "export.h"

#include <stdalign.h>
#include <stddef.h>

/**
 * @brief Cast buffer into a type.
 * See UCB_BUFCAST for proper usage. This is the worker method
 *
 * @param buf the original buffer
 * @param size size of original buffer
 * @param align_size alignment size of type
 * @param elem_size size of type
 * @param out_count the number of elements that fit into the buffer
 * @return the buffer or a new buffer to satisfy alignment
 */
UCB_API void* ucb_bufcast_align(void* buf, size_t size, size_t align_size, size_t elem_size,
                                size_t* out_count);

/**
 * Cast buffer into a type.
 * - If properly aligned to type, the same buffer will return as cast.
 * - If not properly aligned, a new buffer will be allocated and the data copied.
 *
 * Note that:
 * - If alignment is zero, the function will abort.
 * - If buf is UCB_NULL, the function will return UCB_NULL.
 * - If no elements fit in the buffer, the function will return UCB_NULL.
 * - If a new buffer is allocated, the user must free it with ucb_free.
 *   Compare the pointer to the original.
 * - The new buffer may be truncated to only contain complete elements.
 */
#ifdef __cplusplus
#define UCB_BUFCAST(type, buf, size, out_count) \
    reinterpret_cast<type*>(                    \
        ucb_bufcast_align((buf), (size), alignof(type), sizeof(type), out_count))
#else
#define UCB_BUFCAST(type, buf, size, out_count) \
    (type*)ucb_bufcast_align((buf), (size), alignof(type), sizeof(type), out_count)
#endif

#endif // UCB_BUFUTIL_H
