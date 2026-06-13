/**
 * @file my_type.h
 * This file is part of the UCB project
 * - SPDX-FileCopyrightText: © 2026 Åke Svedin <ake@svedin.org>
 * - SPDX-License-Identifier: MIT
 * 
 * @brief Custom type for container tests
 */

#ifndef TESTS_CONTAINER_MY_TYPE_H
#define TESTS_CONTAINER_MY_TYPE_H

typedef struct MyType
{
    int id;
    char name[32];
} MyType;

#ifdef __cplusplus
extern "C" {
#endif

MyType* mytype_new(int id, const char* name);
MyType* mytype_clone(const MyType* src);
void mytype_free(MyType* obj);
int mytype_cmp(const MyType* a, const MyType* b);

#ifdef __cplusplus
}
#endif

#endif // TESTS_CONTAINER_MY_TYPE_H
