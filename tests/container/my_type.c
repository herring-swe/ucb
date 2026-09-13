/**
 * @file my_type.c
 * This file is part of the UCB project
 * - SPDX-FileCopyrightText: © 2026 Åke Svedin <ake@svedin.org>
 * - SPDX-License-Identifier: MIT
 *
 * @brief Custom type implementation for container tests
 */

#include "my_type.h"

#include "ucb/memory.h"

#include <string.h>

MyType* mytype_new(int id, const char* name)
{
    MyType* obj = ucb_malloc_type(1, MyType);
    if (obj)
    {
        obj->id = id;
        strncpy(obj->name, name, sizeof(obj->name));
        obj->name[sizeof(obj->name) - 1] = '\0';
    }
    return obj;
}

MyType* mytype_clone(const MyType* src)
{
    MyType* clone = ucb_malloc_type(1, MyType);
    if (clone)
    {
        clone->id = src->id;
        strncpy(clone->name, src->name, sizeof(clone->name));
        clone->name[sizeof(clone->name) - 1] = '\0';
    }
    return clone;
}

void mytype_free(MyType* obj)
{
    ucb_free(obj);
}

int mytype_cmp(const MyType* a, const MyType* b)
{
    if (a->id != b->id)
        return (a->id > b->id) - (a->id < b->id);
    return strncmp(a->name, b->name, sizeof(a->name));
}
