/**
 * This file is part of the UCB project
 * SPDX-FileCopyrightText: © 2025 Åke Svedin <ake@svedin.org>
 * SPDX-License-Identifier: MIT
 *
 * @brief Basic memory functions
 */

#define UCB_MEMORY_IMPL

#include "ucb/memory.h"

#include "ucb/error.h"
#include "ucb/errcodes.h"
#include "ucb/error_private.h"

#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* -------------------------------------------------------------------------- */
/*                             Core implementation                            */
/* -------------------------------------------------------------------------- */

void* ucb_malloc(size_t size)
{
    void* mem = UCB_NULL;
    if (UCB_LIKELY(size > 0))
    {
        mem = malloc(size);
        if (UCB_LIKELY(mem))
            return mem;
        ucb_set_fatal_error(UCB_ERROR_OUT_OF_MEMORY);
    }
    return mem;
}

void* ucb_calloc(size_t num, size_t size)
{
    void* mem = UCB_NULL;
    if (UCB_LIKELY(num > 0 && size > 0))
    {
        mem = calloc(num, size);
        if (UCB_LIKELY(mem))
            return mem;
        ucb_set_fatal_error(UCB_ERROR_OUT_OF_MEMORY);
    }
    return mem;
}

void* ucb_realloc(void* ptr, size_t size)
{
    return ucb_realloc2(ptr, size, true);
}

void* ucb_realloc2(void* ptr, size_t size, bool free_on_failure)
{
    void* mem = UCB_NULL;
    if (UCB_LIKELY(size > 0))
    {
        mem = realloc(ptr, size);
        if (UCB_LIKELY(mem))
            return mem;
        // Avoid unintentional leak
        if (free_on_failure && ptr)
            free(ptr);
        ucb_set_fatal_error(UCB_ERROR_OUT_OF_MEMORY);
    }
    else
    {
        // Avoid unintentional leak
        if (free_on_failure && ptr)
            free(ptr);
        mem = UCB_NULL;
    }
    return mem;
}

void ucb_free(void* ptr)
{
    if (UCB_LIKELY(ptr))
        free(ptr);
}

ucb_error_t ucb_memcpy(void* dest, size_t dest_size, const void* src, size_t src_size)
{
#if defined(__STDC_LIB_EXT1__) || defined(_WIN32)
    return ucb_wrap_errno(memcpy_s(dest, dest_size, src, src_size));
#else
    if (!dest || !src)
        return UCB_ERROR_INVALID_ARG;
    if (dest_size < src_size)
        return UCB_ERROR_RANGE;
    memcpy(dest, src, src_size);
    return UCB_OK;
#endif
}
