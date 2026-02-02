/**
 * @file pqueue_private.h
 * This file is part of the UCB project
 * - SPDX-FileCopyrightText: © 2026 Åke Svedin <ake@svedin.org>
 * - SPDX-License-Identifier: MIT
 *
 * @brief Priority queue private header
 */

#ifndef UCB_PQUEUE_PRIVATE_H
#define UCB_PQUEUE_PRIVATE_H

#include "ucb/container/pqueue.h"

#include "ucb/container/common_private.h"
#include "ucb/mutex.h"

typedef struct ucb_pqueue_bucket
{
    int priority;
    ucb_fwdlist_node* first;
    ucb_fwdlist_node* last;
    size_t num_items;
} ucb_pqueue_bucket;

struct ucb_pqueue
{
    ucb_pqueue_bucket* buckets; // Sorted by priority, highest first
    size_t num_buckets;
    size_t alloc_buckets;
    ucb_clone_func data_clone;
    ucb_free_func data_free;
    ucb_mutex* mutex; // NULL if not thread-safe
};

bool ucb_pqueue_init(ucb_pqueue* pqueue, ucb_pqueue_args args);
bool ucb_pqueue_init_mt(ucb_pqueue* pqueue, ucb_pqueue_args args);
void ucb_pqueue_release(ucb_pqueue* pqueue);

#endif // UCB_PQUEUE_PRIVATE_H
