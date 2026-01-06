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
        ucb_fatal_format(UCB_ERROR_OUT_OF_MEMORY,
                         "ucb_malloc: Failed to allocate %zu bytes of memory", size);
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
        ucb_fatal_format(UCB_ERROR_OUT_OF_MEMORY,
                         "ucb_calloc: Failed to allocate %zu * %zu = %zu bytes of memory", num,
                         size);
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
        ucb_fatal_format(UCB_ERROR_OUT_OF_MEMORY,
                         "ucb_realloc2: Failed to allocate %zu bytes of memory", size);
        // Avoid unintentional leak
        if (free_on_failure && ptr)
            free(ptr);
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

void ucb_memcpy_s(void* UCB_RESTRICT dest, size_t dest_size, const void* UCB_RESTRICT src,
                  size_t src_size)
{
#if defined(__STDC_LIB_EXT1__) || defined(_WIN32)
    UCB_VERIFY_ERRNO(memcpy_s(dest, dest_size, src, src_size), "Failed to copy memory");
#else
    UCB_VERIFY(dest && src, UCB_ERRSYS_EINVAL, "Invalid arguments");
    UCB_VERIFY(dest_size >= src_size, UCB_ERRSYS_ERANGE, "Destination buffer too small");
    memcpy(dest, src, src_size);
#endif
}
