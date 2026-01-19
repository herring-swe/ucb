/**
 * @file common_private.h
 * This file is part of the UCB project
 * - SPDX-FileCopyrightText: © 2026 Åke Svedin <ake@svedin.org>
 * - SPDX-License-Identifier: MIT
 *
 * @brief Container common private header
 */

#ifndef UCB_CONTAINER_COMMON_PRIVATE_H
#define UCB_CONTAINER_COMMON_PRIVATE_H

#define UCB_CONTAINER_MT true
#define UCB_CONTAINER_ST false

typedef struct ucb_fwdlist_node
{
    void* data;
    struct ucb_fwdlist_node* next;
} ucb_fwdlist_node;

#endif // UCB_CONTAINER_COMMON_PRIVATE_H
