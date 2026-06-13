/**
 * @file cast.c
 * This file is part of the UCB project
 * - SPDX-FileCopyrightText: © 2026 Åke Svedin <ake@svedin.org>
 * - SPDX-License-Identifier: MIT
 *
 * @brief C cast testing iplementation
 */

#include "ucb/cast.h"

#include <float.h>
#include <limits.h>

int test_c_cast()
{
    int i;
    int int_vals[] = {1234, INT_MIN, INT_MAX, 0, INT_MIN + 1, INT_MAX - 1};
    float flt_vals[] = {1234.5678f, FLT_MIN, FLT_MAX, 0.0, FLT_MIN + 1, FLT_MAX - 1};
    double dbl_vals[] = {1234.5678, DBL_MIN, DBL_MAX, 0.0, DBL_MIN + 1, DBL_MAX - 1};

    for (i = 0; i < sizeof(int_vals) / sizeof(int_vals[0]); i++)
    {
        void* ptr = ucb_int2ptr(int_vals[i]);
        if (ucb_ptr2int(ptr) != int_vals[i])
            return 0;
    }
    for (i = 0; i < sizeof(flt_vals) / sizeof(flt_vals[0]); i++)
    {
        void* ptr = ucb_flt2ptr(flt_vals[i]);
        if (ucb_ptr2flt(ptr) != flt_vals[i])
            return 0;
    }

#if UCB_WITH_CAST_DBL_TO_PTR
    for (i = 0; i < sizeof(dbl_vals) / sizeof(dbl_vals[0]); i++)
    {
        void* ptr = ucb_dbl2ptr(dbl_vals[i]);
        if (ucb_ptr2dbl(ptr) != dbl_vals[i])
            return 0;
    }
#else
    std::cout << "Double to pointer tests not available" << std::endl;
#endif
    return 1;
}
