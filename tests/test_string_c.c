/**
 * @file test_string_c.c
 *
 * This file is part of the UCB project
 * - SPDX-FileCopyrightText: © 2026 Åke Svedin <ake@svedin.org>
 * - SPDX-License-Identifier: MIT
 *
 * @brief string C tests implementation
 *
 * Compiled as C to exercise the _Generic branch of UCB_CSTR.
 */

#include "test_string_c.h"

#include "ucb/defines.h"
#include "ucb/string_ex.h"

#include <string.h>

int test_string_c(void)
{
    int errs = 0;

    char buf[] = "buffer";
    const char* cstr = "const";
    ucb_str* heap = ucb_str_new_c("heap");
    ucb_str stack = ucb_str_make();
    ucb_path* path = ucb_path_new(UCB_STR("some/path"));

    const char* p_buf = UCB_CSTR(buf);
    const char* p_cstr = UCB_CSTR(cstr);
    const char* p_lit = UCB_CSTR("literal");
    const char* p_heap = UCB_CSTR(heap);
    const char* p_heap_const = UCB_CSTR((const ucb_str*)heap);
    const char* p_stack = UCB_CSTR(&stack);
    const char* p_path = UCB_CSTR(path);
    const char* p_path_const = UCB_CSTR((const ucb_path*)path);

    if (strcmp(p_buf, "buffer") != 0)
        errs++;
    if (strcmp(p_cstr, "const") != 0)
        errs++;
    if (strcmp(p_lit, "literal") != 0)
        errs++;
    if (strcmp(p_heap, "heap") != 0)
        errs++;
    if (p_heap != p_heap_const)
        errs++;
    if (strcmp(p_stack, "") != 0)
        errs++;
    if (p_path != p_path_const)
        errs++;

    const ucb_str* s_lit = UCB_STR("literal");
    const ucb_str* s_cstr = UCB_STR(cstr);
    const ucb_str* s_buf = UCB_STR(buf);
    const ucb_str* s_empty = UCB_STR("");
    const ucb_str* s_null = UCB_STR((const char*)NULL);

    if (s_lit->size != 7 || s_lit->alloc != 0 || strcmp(s_lit->data, "literal") != 0)
        errs++;
    if (s_cstr->size != 5 || s_cstr->data != cstr)
        errs++;
    if (s_buf->size != 6 || s_buf->data != buf)
        errs++;
    if (s_empty->size != 0 || s_empty->data[0] != '\0')
        errs++;
    if (s_null->size != 0 || s_null->data[0] != '\0')
        errs++;

    ucb_str_release(&stack);
    ucb_str_free(heap);
    ucb_path_free(path);

    return errs;
}
