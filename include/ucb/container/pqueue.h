/**
 * @file pqueue.h
 * This file is part of the UCB project
 * - SPDX-FileCopyrightText: © 2026 Åke Svedin <ake@svedin.org>
 * - SPDX-License-Identifier: MIT
 *
 * @brief Generic priority queue
 */

#ifndef UCB_CONTAINER_PQUEUE_H
#define UCB_CONTAINER_PQUEUE_H

#include "../export.h"
#include "common.h"

/**
 * @struct ucb_pqueue
 * @brief Priority queue
 *
 * This is a generic data container that stores elements in a priority order and otherwise in a FIFO
 * order.
 * The priority is an integer, where higher values are higher priority.
 * Items can be push with priority using @ref ucb_pqueue_push and popped (highest priority) using
 * @ref ucb_pqueue_pop.
 *
 * It supports both single and multi-threaded access depending on the allocator used.
 *
 * Updating items priority or removing a specific item is not supported.
 *
 * Implemented as a priority bucket list, sorted by priority.
 * Each bucket has a linked of data items, with an additional last pointer.
 * Reasoning is that number of unique priorities should be extremely small, compared to number of
 * items in the queue.
 *
 * The number of buckets will grow to accomodate data, but will never shrink automatically.
 * Use @ref ucb_pqueue_fit to shrink, if needed.
 *
 * Push cost (M is number of buckets):
 *  - Best case: O(log M) - Push into existing highiest priority bucket.
 *  - Worst case: O(M) - Push of highest non-existing priority or existing lowest priority.
 *
 * Pop cost:
 *  - Best case: O(1) - Pop from any bucket and not emptying the bucket.
 *  - Worst case: O(M) - Pop last from a bucket, requires a shift of lower priority buckets.
 *
 * Example usage:
 * @code
 * ucb_pqueue_args args = {0};
 * args.data_clone = my_clone; // user-provided function
 * args.data_free  = my_free;  // user-provided function
 *
 * ucb_pqueue* pq = ucb_pqueue_new(args);
 * ucb_pqueue_push(pq, my_item, 0);
 * void* item = ucb_pqueue_pop(pq);
 * ucb_pqueue_free(pq);
 * @endcode
 */
typedef struct ucb_pqueue ucb_pqueue;

/**
 * @struct ucb_pqueue_args
 * @brief Priority queue arguments
 */
typedef struct ucb_pqueue_args
{
    /**
     * @brief Clone function for the data entries
     * Any data pushed will be cloned with this function.
     * If not set, the data entries must outlive the queue.
     * If set, the free function must also be set.
     */
    ucb_clone_func data_clone;
    /**
     * @brief Free function for the data entries.
     * Used to free remaining data items in ucb_pqueue_free or ucb_pqueue_clear.
     */
    ucb_free_func data_free;
} ucb_pqueue_args;

/**
 * @brief Allocates and initiates a new priority queue
 * @param args the arguments
 * @return a pqueue pointer
 */
UCB_API ucb_pqueue* ucb_pqueue_new(ucb_pqueue_args args);
/**
 * @brief Allocates and initiates a new thread-safe priority queue
 * @param args the arguments
 * @return a pqueue pointer
 */
UCB_API ucb_pqueue* ucb_pqueue_new_mt(ucb_pqueue_args args);

/**
 * @brief Free the queue and all its data
 * @param pqueue the queue previously allocated with ucb_pqueue_new
 */
UCB_API void ucb_pqueue_free(ucb_pqueue* pqueue);

/**
 * @brief Clear the queue
 * Removes all items from the queue.
 * Items will be free'd if a free function has been provided.
 * @note This will not re-adjust internal sizes.
 * @param pqueue the queue
 */
UCB_API void ucb_pqueue_clear(ucb_pqueue* pqueue);

/**
 * @brief Adjust the allocated of the queue to fit items held
 * Internally adjust the queue to fit the current number of items.
 * This may be handy if a lot of different priorities have been added and
 * later popped.
 * @note This call may, or may not, adjust anything depending on the internal implementation.
 * @param pqueue the queue
 */
UCB_API void ucb_pqueue_fit(ucb_pqueue* pqueue);

/**
 * @brief Push data onto the queue
 * The item will be cloned, if a clone function has been provided.
 * @param pqueue the queue
 * @param data the data to push onto the queue
 * @param priority the priority of the item, higher value means higher priority
 * @return insert position of the item or SIZE_MAX on failure
 */
UCB_API size_t ucb_pqueue_push(ucb_pqueue* pqueue, void* data, int priority);

/**
 * @brief Pop the top item from the queue
 * The pqueue will no longer manage the data, and it is the caller's responsibility to free it.
 * @param pqueue the queue
 * @return the data or UCB_NULL if the queue is empty or on failure
 */
UCB_API void* ucb_pqueue_pop(ucb_pqueue* pqueue);

/**
 * @brief Peek at the first item in the queue
 * Without removing it from the queue.
 * The pqueue will still manage the data, and it must not be freed.
 * @param pqueue the queue
 * @return the data or UCB_NULL if the queue is empty or on failure
 */
UCB_API const void* ucb_pqueue_peek(const ucb_pqueue* pqueue);

/**
 * @brief Get the number of items in the queue
 * @param pqueue the queue
 * @return the number of items in the queue or 0 on failure
 */
UCB_API size_t ucb_pqueue_size(const ucb_pqueue* pqueue);

/**
 * @brief Check if the queue is empty
 * @param pqueue the queue
 * @return true if empty
 */
UCB_API bool ucb_pqueue_empty(ucb_pqueue* pqueue);

#endif // UCB_CONTAINER_PQUEUE_H
