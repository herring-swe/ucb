/**
 * @file common.h
 * This file is part of the UCB project
 * - SPDX-FileCopyrightText: © 2026 Åke Svedin <ake@svedin.org>
 * - SPDX-License-Identifier: MIT
 *
 * @brief Container common header
 */

#ifndef UCB_CONTAINER_COMMON_H
#define UCB_CONTAINER_COMMON_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

/**
 * @brief Generic comparison function
 * @param a first value
 * @param b second value
 * @return int 0 if equal, negative if a < b, positive if a > b
 */
typedef int (*ucb_cmp_func)(const void* a, const void* b);

/**
 * @brief Generic clone function
 * @param data data to clone
 * @return void* cloned data or UCB_NULL on failure (OOM)
 */
typedef void* (*ucb_clone_func)(const void* data);

/**
 * @brief Generic copy function
 * @param dst destination to copy to
 * @param src source to copy from
 * @return true if copy was successful, false otherwise (OOM)
 */
typedef void* (*ucb_copy_func)(void* dst, const void* src);

/**
 * @brief Generic free function
 * @param data data to free
 */
typedef void (*ucb_free_func)(void* data);

/**
 * @brief Generic release function
 *
 * Frees all data but not the data pointer itself
 * @param data data to release
 */
typedef void (*ucb_release_func)(void* data);

static inline int ucb_comp_func_int(const void* a, const void* b)
{
    return (*(int*)a) - (*(int*)b);
}

static inline int ucb_comp_func_flt(const void* a, const void* b)
{
    float diff = (*(float*)a) - (*(float*)b);
    return (diff > 0.0f) ? 1 : (diff < 0.0f) ? -1 : 0;
}

static inline int ucb_comp_func_dbl(const void* a, const void* b)
{
    double diff = (*(double*)a) - (*(double*)b);
    return (diff > 0.0) ? 1 : (diff < 0.0) ? -1 : 0;
}

static inline int ucb_comp_func_ptr(const void* a, const void* b)
{
    uintptr_t ua = (uintptr_t)a;
    uintptr_t ub = (uintptr_t)b;
    return (ua > ub) ? 1 : (ua < ub) ? -1 : 0;
}

#endif // UCB_CONTAINER_COMMON_H
