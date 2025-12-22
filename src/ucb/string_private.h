/**
 * This file is part of the UCB project
 * SPDX-FileCopyrightText: © 2025 Åke Svedin <ake@svedin.org>
 * SPDX-License-Identifier: MIT
 *
 * @brief
 */

#ifndef UCB_STRING_PRIVATE_H
#define UCB_STRING_PRIVATE_H

#include "ucb/string.h"

#include <stdalign.h>
#include <stdint.h>

struct ucb_str
{
    char* data;
    size_t size;
    uint32_t flags;
    uint32_t reserved;
};

#endif // UCB_STRING_PRIVATE_H
