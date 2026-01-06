/**
 * This file is part of the UCB project
 * SPDX-FileCopyrightText: © 2025 Åke Svedin <ake@svedin.org>
 * SPDX-License-Identifier: MIT
 *
 * @brief Implementation of buffer utilities
 */

#include "ucb/bufutil.h"

#include "ucb/memory.h"

#include <stdint.h>
#include <stdlib.h>

void* ucb_bufcast_align(void* buf, size_t size, size_t align_size, size_t elem_size,
                        size_t* out_count)
{
    if (align_size == 0)
        abort();

    // Number of complete elements that fit in size
    size_t count = (size / elem_size);
    if (out_count)
        *out_count = count;

    if (!buf || count == 0)
        return UCB_NULL;

    // If already aligned, return as-is
    uintptr_t addr = (uintptr_t)buf;
    if (addr % align_size == 0 || size == 0)
        return (void*)buf;

    // Truncate according to count
    size_t new_size = count * elem_size;
    if (new_size == 0)
        return UCB_NULL;

    // Create new buffer and copy data
    void* aligned_buf = ucb_malloc(new_size);
    if (aligned_buf)
        ucb_memcpy_s(aligned_buf, new_size, buf, new_size);
    return aligned_buf;
}
