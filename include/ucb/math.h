/**
 * This file is part of the UCB project
 * SPDX-FileCopyrightText: © 2025 Your Name <your@email.com>
 * SPDX-License-Identifier: MIT
 *
 * @brief Simple math defines and functions
 */

#ifndef UCB_MATH_H
#define UCB_MATH_H

#include "generics.h"
#include "limits.h"

#include <stdbool.h>

#define UCB_MATH_FUNCS_IMPL(TNAME, TYPE)                                                         \
    static inline TYPE ucb_min_##TNAME(const TYPE a, const TYPE b)                               \
    {                                                                                            \
        return (a < b) ? a : b;                                                                  \
    }                                                                                            \
    static inline TYPE ucb_max_##TNAME(const TYPE a, const TYPE b)                               \
    {                                                                                            \
        return (a > b) ? a : b;                                                                  \
    }                                                                                            \
    static inline TYPE ucb_clamp_##TNAME(const TYPE val, const TYPE min_val, const TYPE max_val) \
    {                                                                                            \
        return ucb_max_##TNAME(min_val, ucb_min_##TNAME(val, max_val));                          \
    }                                                                                            \
    static inline void ucb_swap_##TNAME(TYPE* a, TYPE* b)                                        \
    {                                                                                            \
        TYPE tmp = *a;                                                                           \
        *a       = *b;                                                                           \
        *b       = tmp;                                                                          \
    }                                                                                            \
    static inline int ucb_in_range_##TNAME(const TYPE val, const TYPE low, const TYPE high)      \
    {                                                                                            \
        return (val >= low) && (val <= high);                                                    \
    }

#define UCB_MATH_FUNCS_SIGNED_IMPL(TNAME, TYPE)        \
    static inline TYPE ucb_sign_##TNAME(const TYPE val) \
    {                                                  \
        return (val > 0) - (val < 0);                  \
    }                                                  \
    static inline TYPE ucb_abs_##TNAME(const TYPE val) \
    {                                                  \
        return (val < 0) ? -val : val;                 \
    }

#define UCB_MATH_FUNCS_FLOAT_IMPL(TNAME, TYPE)                                           \
    static inline bool ucb_equal_eps_##TNAME(const TYPE a, const TYPE b, const TYPE eps) \
    {                                                                                    \
        return b - a < eps;                                                              \
    }

UCB_MATH_FUNCS_IMPL(hhi, signed char)
UCB_MATH_FUNCS_IMPL(hhu, unsigned char)
UCB_MATH_FUNCS_IMPL(hi, short)
UCB_MATH_FUNCS_IMPL(hu, unsigned short)
UCB_MATH_FUNCS_IMPL(i, int)
UCB_MATH_FUNCS_IMPL(u, unsigned int)
UCB_MATH_FUNCS_IMPL(li, long)
UCB_MATH_FUNCS_IMPL(lu, unsigned long)
UCB_MATH_FUNCS_IMPL(lli, long long)
UCB_MATH_FUNCS_IMPL(llu, unsigned long long)
UCB_MATH_FUNCS_IMPL(f, float)
UCB_MATH_FUNCS_IMPL(d, double)

UCB_MATH_FUNCS_SIGNED_IMPL(hhi, signed char)
UCB_MATH_FUNCS_SIGNED_IMPL(hi, short)
UCB_MATH_FUNCS_SIGNED_IMPL(i, int)
UCB_MATH_FUNCS_SIGNED_IMPL(li, long)
UCB_MATH_FUNCS_SIGNED_IMPL(lli, long long)
UCB_MATH_FUNCS_SIGNED_IMPL(f, float)
UCB_MATH_FUNCS_SIGNED_IMPL(d, double)

UCB_MATH_FUNCS_FLOAT_IMPL(f, float)
UCB_MATH_FUNCS_FLOAT_IMPL(d, double)

static inline bool ucb_equal_f(float a, float b)
{
    return ucb_equal_eps_f(a, b, __FLT_EPSILON__);
}

static inline bool ucb_equal_d(double a, double b)
{
    return ucb_equal_eps_d(a, b, __DBL_EPSILON__);
}

#undef UCB_MATH_FUNCS_IMPL
#undef UCB_MATH_FUNCS_SIGNED_IMPL
#undef UCB_MATH_FUNCS_FLOAT_IMPL

#ifndef __cplusplus

#define ucb_min(a, b)               UCB_GENERIC_FUNC_2(ucb_min, a, b)
#define ucb_max(a, b)               UCB_GENERIC_FUNC_2(ucb_max, a, b)
#define ucb_clamp(val, min, max)    UCB_GENERIC_FUNC_3(ucb_clamp, val, min, max)
#define ucb_in_range(val, min, max) UCB_GENERIC_FUNC_3(ucb_in_range, val, min, max)
#define ucb_swap(a, b)              UCB_GENERIC_FUNC_2(ucb_swap, a, b)
#define ucb_sign(a)                 UCB_GENERIC_FUNC_SIGNED_1(ucb_sign, a)
#define ucb_abs(a)                  UCB_GENERIC_FUNC_SIGNED_1(ucb_abs, a)
#define ucb_equal_eps(a, b, eps)    UCB_GENERIC_FUNC_FLOAT_3(ucb_equal_eps, a, b, eps)
#define ucb_equal(a, b)             UCB_GENERIC_FUNC_FLOAT_2(ucb_equal, a, b)

#endif // __cplusplus

#endif // UCB_MATH_H
