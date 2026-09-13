/**
 * @file algorithm.c
 * This file is part of the UCB project
 * - SPDX-FileCopyrightText: © 2026 Åke Svedin <ake@svedin.org>
 * - SPDX-License-Identifier: MIT
 *
 * @brief Algorithmic functions implementation
 */

#include <ucb/algorithm.h>

#include <stdlib.h>

#ifdef _MSC_VER
typedef struct
{
    ucb_cmp_func_ctx func;
    void* ctx;
} qsort_ctx_wrapper;

static int cmp_func_ctx_wrapper(void* ctx, const void* a, const void* b)
{
    qsort_ctx_wrapper* ctx_wrap = (qsort_ctx_wrapper*)ctx;
    return ctx_wrap->func(a, b, ctx_wrap->ctx);
}
#endif

void ucb_qsort_ctx(void* base, size_t num, size_t size, ucb_cmp_func_ctx func, void* ctx)
{
#ifdef _MSC_VER
    qsort_ctx_wrapper ctx_wrap = {
        .func = func,
        .ctx = ctx,
    };
    return qsort_s(base, num, size, cmp_func_ctx_wrapper, &ctx_wrap);
#else
    return qsort_r(base, num, size, func, ctx);
#endif
}
