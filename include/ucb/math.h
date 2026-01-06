/**
 * This file is part of the UCB project
 * SPDX-FileCopyrightText: © 2025 Your Name <your@email.com>
 * SPDX-License-Identifier: MIT
 *
 * @brief Common generic math functions
 */

#ifndef UCB_MATH_H
#define UCB_MATH_H

#include "generics.h"

#include <float.h>
#include <limits.h>
#include <stdbool.h>

#define UCB_EPSILON_FLT FLT_EPSILON
#define UCB_EPSILON_DBL DBL_EPSILON

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
    static inline bool ucb_in_range_##TNAME(const TYPE val, const TYPE low, const TYPE high)     \
    {                                                                                            \
        return (val >= low) && (val <= high);                                                    \
    }

#define UCB_MATH_FUNCS_SIGNED_IMPL(TNAME, TYPE)        \
    static inline int ucb_sign_##TNAME(const TYPE val) \
    {                                                  \
        return (val > 0) - (val < 0);                  \
    }                                                  \
    static inline TYPE ucb_abs_##TNAME(const TYPE val) \
    {                                                  \
        return (val < 0) ? -val : val;                 \
    }

#define UCB_MATH_FUNCS_FLOAT_IMPL(TNAME, TMACRO, TYPE)                                          \
    static inline bool ucb_equal_eps_##TNAME(const TYPE a, const TYPE b, const TYPE eps)        \
    {                                                                                           \
        return ucb_abs_##TNAME(a - b) < eps;                                                    \
    }                                                                                           \
    static inline bool ucb_equal_##TNAME(const TYPE a, const TYPE b)                            \
    {                                                                                           \
        return ucb_equal_eps_##TNAME(a, b, UCB_EPSILON_##TMACRO);                               \
    }                                                                                           \
    static inline int ucb_comp_eps_##TNAME(const TYPE a, const TYPE b, const TYPE eps)          \
    {                                                                                           \
        return ucb_equal_eps_##TNAME(a, b, eps) ? 0 : ucb_sign_##TNAME(a - b);                  \
    }                                                                                           \
    static inline int ucb_comp_##TNAME(const TYPE a, const TYPE b)                              \
    {                                                                                           \
        return ucb_comp_eps_##TNAME(a, b, UCB_EPSILON_##TMACRO);                                \
    }                                                                                           \
    static inline bool ucb_approx_equal_eps_##TNAME(const TYPE a, const TYPE b, const TYPE eps) \
    {                                                                                           \
        return ucb_abs_##TNAME(a - b) <=                                                        \
               ucb_max_##TNAME(ucb_abs_##TNAME(a), ucb_abs_##TNAME(b)) * eps;                   \
    }                                                                                           \
    static inline bool ucb_approx_equal_##TNAME(const TYPE a, const TYPE b)                     \
    {                                                                                           \
        return ucb_approx_equal_eps_##TNAME(a, b, UCB_EPSILON_##TMACRO);                        \
    }                                                                                           \
    static inline int ucb_approx_comp_eps_##TNAME(const TYPE a, const TYPE b, const TYPE eps)   \
    {                                                                                           \
        return ucb_approx_equal_eps_##TNAME(a, b, eps) ? 0 : ucb_sign_##TNAME(a - b);           \
    }                                                                                           \
    static inline int ucb_approx_comp_##TNAME(const TYPE a, const TYPE b, const TYPE eps)       \
    {                                                                                           \
        return ucb_approx_comp_eps_##TNAME(a, b, eps);                                          \
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

UCB_MATH_FUNCS_FLOAT_IMPL(f, FLT, float)
UCB_MATH_FUNCS_FLOAT_IMPL(d, DBL, double)

#undef UCB_MATH_FUNCS_IMPL
#undef UCB_MATH_FUNCS_SIGNED_IMPL
#undef UCB_MATH_FUNCS_FLOAT_IMPL

#ifndef __cplusplus

/**
 * @brief Get smallest value
 * @return min of a and b
 * Works for all primitives
 */
#define ucb_min(a, b) UCB_GENERIC_FUNC_2(ucb_min, a, b)

/**
 * @brief Get largest value
 * @return max of a and b
 * Works for all primitives
 */
#define ucb_max(a, b) UCB_GENERIC_FUNC_2(ucb_max, a, b)

/**
 * @brief Clamp val between min and max
 * @return val if val is between min and max, otherwise min or max
 * Works for all primitives
 */
#define ucb_clamp(val, min, max) UCB_GENERIC_FUNC_3(ucb_clamp, val, min, max)

/**
 * @brief Check if val is between min and max
 * @return bool true if val is between min and max
 * Works for all primitives
 */
#define ucb_in_range(val, min, max) UCB_GENERIC_FUNC_3(ucb_in_range, val, min, max)

/**
 * @brief Swap value of two variables
 * @param a address of first value
 * @param b address of second value
 * Works for all primitives
 */
#define ucb_swap(a, b) UCB_GENERIC_FUNC_2(ucb_swap, a, b)

/**
 * @brief Return sign of val
 * @return int -1, 0 or 1
 * Works for signed primitives
 */
#define ucb_sign(val) UCB_GENERIC_FUNC_SIGNED_1(ucb_sign, val)

/**
 * @brief Get absolute value
 * @return val or -val if val < 0
 * Works for signed primitives
 */
#define ucb_abs(val) UCB_GENERIC_FUNC_SIGNED_1(ucb_abs, val)

/**
 * @brief Check if two values are equal within epsilon
 * @return bool true if a and b are equal within epsilon
 * Works for floating point primitives
 */
#define ucb_equal_eps(a, b, eps) UCB_GENERIC_FUNC_FLOAT_3(ucb_equal_eps, a, b, eps)

/**
 * @brief Check if two values are equal within the smallest epsilon for that type
 * @return bool true if a and b are equal within epsilon
 * Works for floating point primitives
 */
#define ucb_equal(a, b) UCB_GENERIC_FUNC_FLOAT_2(ucb_equal, a, b)

/**
 * @brief Compare two values within epsilon
 * @return int 0 if equal, -1 if a < b, 1 if a > b
 * Works for floating point primitives
 */
#define ucb_comp_eps(a, b, eps) UCB_GENERIC_FUNC_FLOAT_3(ucb_comp_eps, a, b, eps)

/**
 * @brief Compare two values within the smallest epsilon for that type
 * @return int 0 if equal, -1 if a < b, 1 if a > b
 * Works for floating point primitives
 */
#define ucb_comp(a, b) UCB_GENERIC_FUNC_FLOAT_2(ucb_comp, a, b)

/**
 * @brief Approximate equality for floating point values
 * @return bool true if a and b are approximately equal
 * Approximate equality is defined as a and b being within epsilon * max(|a|, |b|)
 * Works for floating point primitives
 */
#define ucb_approx_equal_eps(a, b, eps) UCB_GENERIC_FUNC_FLOAT_3(ucb_approx_equal_eps, a, b, eps)

/**
 * @brief Approximate equality for floating point values using the smallest epsilon for that type
 * @return bool true if a and b are approximately equal
 * @see ucb_approx_equal_eps
 * Works for floating point primitives
 */
#define ucb_approx_equal(a, b) UCB_GENERIC_FUNC_FLOAT_2(ucb_approx_equal, a, b)

/**
 * @brief Approximate comparison for floating point values
 * @return int 0 if a and b are approximately equal, -1 if a < b, 1 if a > b
 * @see ucb_approx_equal_eps
 * Works for floating point primitives
 */
#define ucb_approx_comp_eps(a, b, eps) UCB_GENERIC_FUNC_FLOAT_3(ucb_approx_comp_eps, a, b, eps)

/**
 * @brief Approximate comparison for floating point values using the smallest epsilon for that type
 * @return int 0 if a and b are approximately equal, -1 if a < b, 1 if a > b
 * @see ucb_approx_equal_eps
 * Works for floating point primitives
 */
#define ucb_approx_comp(a, b) UCB_GENERIC_FUNC_FLOAT_2(ucb_approx_comp, a, b)

#endif // __cplusplus

#endif // UCB_MATH_H
