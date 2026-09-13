/**
 * @file list.h
 * This file is part of the UCB project
 * - SPDX-FileCopyrightText: © 2026 Åke Svedin <ake@svedin.org>
 * - SPDX-License-Identifier: MIT
 *
 * @brief Generic linked list
 */

#ifndef UCB_CONTAINER_LIST_H
#define UCB_CONTAINER_LIST_H

#include <ucb/container/common.h>
#include <ucb/export.h>

#include <stddef.h>

/**
 * @struct ucb_list
 * @brief Linked list
 *
 * This is a generic linked list data container.
 * It supports both single and multi-threaded access depending on the allocator used.
 *
 * Example usage:
 * @code
 * ucb_list_args args = {0};
 * args.data_clone = my_clone; // user-provided function
 * args.data_free  = my_free;  // user-provided function
 *
 * ucb_list* list = ucb_list_new(args);
 * ucb_list_push_back(list, my_item);
 * void* item = ucb_list_pop_front(list);
 * ucb_list_free(list);
 * @endcode
 */
typedef struct ucb_list ucb_list;

typedef struct ucb_list_args
{
    ucb_cmp_func data_cmp;     // optional, if not provided, items will be compared by pointer
    ucb_clone_func data_clone; // optional, if not provided, data will be stored as-is
    ucb_free_func data_free;   // optional, if not provided, data will not be freed
} ucb_list_args;

UCB_API ucb_list* ucb_list_new(ucb_list_args args);
UCB_API ucb_list* ucb_list_new_mt(ucb_list_args args);

UCB_API void ucb_list_free(ucb_list* list);

UCB_API void ucb_list_push_back(ucb_list* list, const void* data);
UCB_API void ucb_list_push_front(ucb_list* list, const void* data);

UCB_API void* ucb_list_pop_back(ucb_list* list);
UCB_API void* ucb_list_pop_front(ucb_list* list);

UCB_API void* ucb_list_peek_back(const ucb_list* list);
UCB_API void* ucb_list_peek_front(const ucb_list* list);

UCB_API size_t ucb_list_size(const ucb_list* list);
UCB_API bool ucb_list_is_empty(const ucb_list* list);

#endif // UCB_CONTAINER_LIST_H
