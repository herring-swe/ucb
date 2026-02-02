/**
 * @file pqueue.c
 * This file is part of the UCB project
 * - SPDX-FileCopyrightText: © 2026 Åke Svedin <ake@svedin.org>
 * - SPDX-License-Identifier: MIT
 *
 * @brief Priority queue implementation
 */

#include "pqueue_private.h"

#include "ucb/debug.h"
#include "ucb/error.h"
#include "ucb/memory.h"
#include "ucb/mutex.h"

#include <stdint.h>
#include <stdlib.h>
#include <string.h>

/*
 * Possible improvements:
 * - Support checking if a node is already in the queue
 * - Support removing a node from the queue
 * - Support changing the priority of a node
 * All above require a comparison function on the data object itself.
 *
 * - Support for batch push/pop
 */

// clang-format off
#define LOCK_MUTEX(pq)   do { if (pq->mutex) ucb_mutex_lock(pq->mutex); } while (0)
#define UNLOCK_MUTEX(pq) do { if (pq->mutex) ucb_mutex_unlock(pq->mutex); } while (0)
// clang-format on

#define PQUEUE_INITIAL_NUM_BUCKETS_INITIAL 16
#define PQUEUE_BUCKETS_GROW_FACTOR         2

static bool ucb_pqueue_init_common(ucb_pqueue* pq, ucb_pqueue_args args, bool mt)
{
    UCB_VERIFY_ARGS(pq);
    if (args.data_clone && !args.data_free)
    {
        UCB_VERIFY(false, UCB_ERROR_INVALID_ARG, "clone_func requires free_func to be set");
    }

    pq->buckets = ucb_malloc_type(PQUEUE_INITIAL_NUM_BUCKETS_INITIAL, ucb_pqueue_bucket);
    if (pq->buckets)
    {
        pq->data_clone = args.data_clone;
        pq->data_free  = args.data_free;

        pq->num_buckets   = 0;
        pq->alloc_buckets = PQUEUE_INITIAL_NUM_BUCKETS_INITIAL;
        pq->mutex         = mt ? ucb_mutex_new() : UCB_NULL;
    }
    return pq->buckets != UCB_NULL;
}

/**
 * @brief Search for a bucket with the given priority
 * @param pq the priority queue
 * @param prio the priority
 * @return int positive index if found, otherwise returns -index-1, where index is insert point.
 */
static int ucb_pqueue_search_bucket(ucb_pqueue* pq, int prio)
{
    // Binary search for the bucket with the given priority
    int low  = 0;
    int high = (int)pq->num_buckets - 1;

    while (low <= high)
    {
        int mid = low + (high - low) / 2;
        if (pq->buckets[mid].priority == prio)
            return mid;
        else if (pq->buckets[mid].priority >= prio)
            low = mid + 1;
        else
            high = mid - 1;
    }
    return -(low + 1);
}

static size_t ucb_pqueue_push_node(ucb_pqueue* pq, int prio, ucb_fwdlist_node* node)
{
    LOCK_MUTEX(pq);

    ucb_pqueue_bucket* bucket;
    int idx = ucb_pqueue_search_bucket(pq, prio);
    if (idx >= 0)
    {
        bucket = &pq->buckets[idx];
        UCB_ASSERT(bucket->priority == prio, UCB_ERROR_INVALID_STATE, "Bucket priority mismatch");
        UCB_ASSERT(bucket->num_items > 0, UCB_ERROR_INVALID_STATE, "Bucket should have items");

        bucket->last->next = node;
        bucket->last       = node;
        bucket->num_items++;
    }
    else
    {
        idx = -idx - 1;
        // Resize if necessary
        if (pq->num_buckets == pq->alloc_buckets)
        {
            size_t new_size = pq->alloc_buckets * PQUEUE_BUCKETS_GROW_FACTOR;
            ucb_pqueue_bucket* new_buckets =
                ucb_realloc_type(pq->buckets, new_size, ucb_pqueue_bucket);
            if (!new_buckets)
                return SIZE_MAX;
            pq->alloc_buckets = new_size;
            pq->buckets       = new_buckets;
        }

        if (idx == pq->num_buckets)
        {
            // Append new bucket
            bucket = &pq->buckets[pq->num_buckets];
        }
        else
        {
            // Shift to accomodate bucket
            memmove(&pq->buckets[idx + 1], &pq->buckets[idx],
                    (pq->num_buckets - idx) * sizeof(ucb_pqueue_bucket));
            bucket = &pq->buckets[idx];
        }

        bucket->num_items = 1;
        bucket->priority  = prio;
        bucket->first     = node;
        bucket->last      = node;
        node->next        = UCB_NULL;
        pq->num_buckets++;
    }

    size_t ins_pos = pq->buckets[idx].num_items - 1;
    idx--;
    while (idx >= 0)
    {
        ins_pos += pq->buckets[idx].num_items;
        idx--;
    }

    UNLOCK_MUTEX(pq);

    return ins_pos;
}

static ucb_fwdlist_node* ucb_pqueue_pop_node(ucb_pqueue* pq)
{
    ucb_fwdlist_node* node = UCB_NULL;

    LOCK_MUTEX(pq);

    if (pq->num_buckets)
    {
        // Always pop from head of first bucket
        node                 = pq->buckets[0].first;
        pq->buckets[0].first = node->next;
        node->next           = UCB_NULL;
        pq->buckets->num_items--;

        if (pq->buckets[0].num_items == 0)
        {
            if (pq->num_buckets > 1)
            {
                // Shift buckets to the left
                memmove(&pq->buckets[0], &pq->buckets[1],
                        (pq->num_buckets - 1) * sizeof(ucb_pqueue_bucket));
            }
            pq->num_buckets--;
        }
    }

    UNLOCK_MUTEX(pq);

    return node;
}

ucb_pqueue* ucb_pqueue_new(ucb_pqueue_args args)
{
    ucb_pqueue* pq = ucb_malloc_type(1, ucb_pqueue);
    if (pq && !ucb_pqueue_init_common(pq, args, UCB_CONTAINER_ST))
    {
        ucb_free(pq);
        pq = UCB_NULL;
    }
    return pq;
}

ucb_pqueue* ucb_pqueue_new_mt(ucb_pqueue_args args)
{
    ucb_pqueue* pq = ucb_malloc_type(1, ucb_pqueue);
    if (pq && !ucb_pqueue_init_common(pq, args, UCB_CONTAINER_MT))
    {
        ucb_free(pq);
        pq = UCB_NULL;
    }
    return pq;
}

bool ucb_pqueue_init(ucb_pqueue* pq, ucb_pqueue_args args)
{
    return ucb_pqueue_init_common(pq, args, UCB_CONTAINER_ST);
}

bool ucb_pqueue_init_mt(ucb_pqueue* pq, ucb_pqueue_args args)
{
    return ucb_pqueue_init_common(pq, args, UCB_CONTAINER_MT);
}

void ucb_pqueue_release(ucb_pqueue* pq)
{
    ucb_pqueue_clear(pq);

    if (pq->mutex)
        ucb_mutex_free(pq->mutex);

    ucb_free(pq->buckets);
    pq->alloc_buckets = 0;

    // Clear what hasn't been cleared above
    pq->data_clone = UCB_NULL;
    pq->data_free  = UCB_NULL;
    pq->mutex      = UCB_NULL;
}

void ucb_pqueue_free(ucb_pqueue* pq)
{
    ucb_pqueue_release(pq);
    ucb_free(pq);
}

void ucb_pqueue_clear(ucb_pqueue* pq)
{
    UCB_VERIFY_ARGS(pq);

    LOCK_MUTEX(pq);

    for (size_t bi = 0; bi < pq->num_buckets; bi++)
    {
        ucb_pqueue_bucket* bucket = &pq->buckets[bi];

        ucb_fwdlist_node* first = bucket->first;
        while (first)
        {
            ucb_fwdlist_node* next = first->next;
            if (pq->data_free)
                pq->data_free(first->data);
            ucb_free(first);
            first = next;
        }
    }
    pq->num_buckets = 0;

    UNLOCK_MUTEX(pq);
}

void ucb_pqueue_fit(ucb_pqueue* pq)
{
    UCB_VERIFY_ARGS(pq);

    LOCK_MUTEX(pq);

    if (pq->num_buckets > PQUEUE_INITIAL_NUM_BUCKETS_INITIAL && pq->num_buckets < pq->alloc_buckets)
    {
        pq->buckets       = ucb_realloc(pq->buckets, pq->num_buckets * sizeof(ucb_pqueue_bucket));
        pq->alloc_buckets = pq->num_buckets;
    }

    UNLOCK_MUTEX(pq);
}

size_t ucb_pqueue_push(ucb_pqueue* pq, void* data, int prio)
{
    UCB_VERIFY_ARGS(pq && data);

    ucb_fwdlist_node* node;
    node       = ucb_malloc_type(1, ucb_fwdlist_node);
    node->next = UCB_NULL;
    if (pq->data_clone)
    {
        node->data = pq->data_clone(data);
        if (!node->data)
        {
            ucb_free(node);
            return SIZE_MAX;
        }
    }
    else
    {
        node->data = data;
    }

    return ucb_pqueue_push_node(pq, prio, node);
}

void* ucb_pqueue_pop(ucb_pqueue* pq)
{
    UCB_VERIFY_ARGS(pq);

    void* data             = UCB_NULL;
    ucb_fwdlist_node* node = ucb_pqueue_pop_node(pq);
    if (node)
    {
        data = node->data;
        ucb_free(node);
    }

    return data;
}

const void* ucb_pqueue_peek(const ucb_pqueue* pq)
{
    UCB_VERIFY_ARGS(pq);
    void* data = UCB_NULL;
    LOCK_MUTEX(pq);
    if (pq->num_buckets)
    {
        data = pq->buckets[0].first->data;
    }
    UNLOCK_MUTEX(pq);
    return data;
}

size_t ucb_pqueue_size(const ucb_pqueue* pq)
{
    UCB_VERIFY_ARGS(pq);
    size_t ret = 0;
    LOCK_MUTEX(pq);
    for (size_t i = 0; i < pq->num_buckets; i++)
    {
        ret += pq->buckets[i].num_items;
    }
    UNLOCK_MUTEX(pq);
    return ret;
}

bool ucb_pqueue_empty(ucb_pqueue* pq)
{
    UCB_VERIFY_ARGS(pq);
    bool ret = false;
    LOCK_MUTEX(pq);
    ret = pq->num_buckets == 0;
    UNLOCK_MUTEX(pq);
    return ret;
}
