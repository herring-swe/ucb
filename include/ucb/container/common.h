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
 * @return void* cloned data
 */
typedef void* (*ucb_clone_func)(const void* data);

/**
 * @brief Generic free function
 * @param data data to free
 */
typedef void (*ucb_free_func)(void* data);

#endif // UCB_CONTAINER_COMMON_H
