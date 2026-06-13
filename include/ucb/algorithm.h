/**
 * @file algorithm.h
 * This file is part of the UCB project
 * - SPDX-FileCopyrightText: © 2026 Åke Svedin <ake@svedin.org>
 * - SPDX-License-Identifier: MIT
 *
 * @brief Algorithmic functions
 */

#include <ucb/export.h>

#include <stddef.h>

/**
 * @brief Comparison function with context
 * @param a first object to compare
 * @param b second object to compare
 * @param ctx custom context
 * @return 0 if a and b are equal, negative number if a < b or positive number if a > b
 */
typedef int (*ucb_cmp_func_ctx)(const void* a, const void* b, void* ctx);

/**
 * @brief A cross platform version of qsort with context.
 *
 * On POSIX, this is backed by qsort_r.
 * On Windows, this is backed by qsort_s, using a simple wrapper to correctly call the comparison
 * function.
 * @param base the start of the array.
 * @param num number of elements
 * @param size size of each element
 * @param func comparison function
 * @param ctx custom context
 */
UCB_API void ucb_qsort_ctx(void* base, size_t num, size_t size, ucb_cmp_func_ctx func, void* ctx);
