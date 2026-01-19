/**
 * @file tgfunc.h
 * This file is part of the UCB project
 * - SPDX-FileCopyrightText: © 2026 Åke Svedin <ake@svedin.org>
 * - SPDX-License-Identifier: MIT
 *
 * @brief Type generic functions
 */

#ifndef UCB_FUNC_H
#define UCB_FUNC_H

#include "generics.h"

#include <float.h>
#include <limits.h>
#include <stdbool.h>

#define UCB_EPSILON_FLT FLT_EPSILON
#define UCB_EPSILON_DBL DBL_EPSILON

/** @cond INTERNAL */

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
    static inline bool ucb_in_range_##TNAME(const TYPE val, const TYPE low, const TYPE high)     \
    {                                                                                            \
        return (val >= low) && (val <= high);                                                    \
    }                                                                                            \
    static inline void ucb_swap_##TNAME(TYPE* a, TYPE* b)                                        \
    {                                                                                            \
        TYPE tmp = *a;                                                                           \
        *a       = *b;                                                                           \
        *b       = tmp;                                                                          \
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
        return ucb_equal_eps_##TNAME(a, b, eps) ? 0 : (a < b) ? -1 : 1;                         \
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
        return ucb_approx_equal_eps_##TNAME(a, b, eps) ? 0 : (a < b) ? -1 : 1;                  \
    }                                                                                           \
    static inline int ucb_approx_comp_##TNAME(const TYPE a, const TYPE b)                       \
    {                                                                                           \
        return ucb_approx_comp_eps_##TNAME(a, b, UCB_EPSILON_##TMACRO);                         \
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

/** @endcond */

#ifndef __cplusplus

/**
 * @brief Get smallest value
 * 
 * Works for all primitives
 * @return min of a and b
 */
#define ucb_min(a, b) UCB_GENERIC_FUNC_2(ucb_min, a, b)

/**
 * @brief Get largest value
 * 
 * Works for all primitives
 * @return max of a and b
 */
#define ucb_max(a, b) UCB_GENERIC_FUNC_2(ucb_max, a, b)

/**
 * @brief Clamp val between min and max
 * 
 * Works for all primitives
 * @return val if val is between min and max, otherwise min or max
 */
#define ucb_clamp(val, min, max) UCB_GENERIC_FUNC_3(ucb_clamp, val, min, max)

/**
 * @brief Check if val is between min and max
 * 
 * Works for all primitives
 * @return bool true if val is between min and max
 */
#define ucb_in_range(val, min, max) UCB_GENERIC_FUNC_3(ucb_in_range, val, min, max)

/**
 * @brief Swap value of two variables
 * 
 * Works for all primitives
 * @param a address of first value
 * @param b address of second value
 */
#define ucb_swap(a, b) UCB_GENERIC_FUNC_2(ucb_swap, a, b)

/**
 * @brief Return sign of val
 * 
 * Works for signed primitives
 * @return int -1, 0 or 1
 */
#define ucb_sign(val) UCB_GENERIC_FUNC_SIGNED_1(ucb_sign, val)

/**
 * @brief Get absolute value
 * 
 * Works for signed primitives
 * @return val or -val if val < 0
 */
#define ucb_abs(val) UCB_GENERIC_FUNC_SIGNED_1(ucb_abs, val)

/**
 * @brief Check if two values are equal within epsilon
 * 
 * Works for floating point primitives
 * @return bool true if a and b are equal within epsilon
 */
#define ucb_equal_eps(a, b, eps) UCB_GENERIC_FUNC_FLOAT_3(ucb_equal_eps, a, b, eps)

/**
 * @brief Check if two values are equal within the smallest epsilon for that type
 * 
 * Works for floating point primitives
 * @return bool true if a and b are equal within epsilon
 */
#define ucb_equal(a, b) UCB_GENERIC_FUNC_FLOAT_2(ucb_equal, a, b)

/**
 * @brief Compare two values within epsilon
 * 
 * Works for floating point primitives
 * @return int 0 if equal, -1 if a < b, 1 if a > b
 */
#define ucb_comp_eps(a, b, eps) UCB_GENERIC_FUNC_FLOAT_3(ucb_comp_eps, a, b, eps)

/**
 * @brief Compare two values within the smallest epsilon for that type
 * 
 * Works for floating point primitives
 * @return int 0 if equal, -1 if a < b, 1 if a > b
 */
#define ucb_comp(a, b) UCB_GENERIC_FUNC_FLOAT_2(ucb_comp, a, b)

/**
 * @brief Approximate equality for floating point values
 * 
 * Approximate equality is defined as a and b being within epsilon * max(|a|, |b|)
 * Works for floating point primitives
 * @return bool true if a and b are approximately equal
 */
#define ucb_approx_equal_eps(a, b, eps) UCB_GENERIC_FUNC_FLOAT_3(ucb_approx_equal_eps, a, b, eps)

/**
 * @brief Approximate equality for floating point values using the smallest epsilon for that type
 * 
 * Works for floating point primitives
 * @return bool true if a and b are approximately equal
 * @see ucb_approx_equal_eps
 */
#define ucb_approx_equal(a, b) UCB_GENERIC_FUNC_FLOAT_2(ucb_approx_equal, a, b)

/**
 * @brief Approximate comparison for floating point values
 * 
 * Works for floating point primitives
 * @return int 0 if a and b are approximately equal, -1 if a < b, 1 if a > b
 * @see ucb_approx_equal_eps
 */
#define ucb_approx_comp_eps(a, b, eps) UCB_GENERIC_FUNC_FLOAT_3(ucb_approx_comp_eps, a, b, eps)

/**
 * @brief Approximate comparison for floating point values using the smallest epsilon for that type
 * 
 * Works for floating point primitives
 * @return int 0 if a and b are approximately equal, -1 if a < b, 1 if a > b
 * @see ucb_approx_equal_eps
 */
#define ucb_approx_comp(a, b) UCB_GENERIC_FUNC_FLOAT_2(ucb_approx_comp, a, b)

/**
 * @brief Type generic macro for swapping two values of the same type
 * @param T The type of the values to swap
 * @param a The first value
 * @param b The second value
 * @note This macro is not type safe
 */
#define UCB_SWAP(T, a, b) \
    do                    \
    {                     \
        T _tmp = (a);     \
        (a)    = (b);     \
        (b)    = _tmp;    \
    } while (0)

#else // __cplusplus

/*
 * C++11 versions of generic macros for completeness.
 * These simply mimmick how the generic macros work, so it allows different
 * types for arguments, as long as they fulfill the requirements of the function.
 */

#include <type_traits>

template <typename T, typename U>
constexpr std::common_type_t<T, U> ucb_min(T a, U b) noexcept
{
    static_assert(std::is_arithmetic<T>::value && std::is_arithmetic<U>::value,
                  "ucb_min: T and U must be arithmetic types");
    return (a < b) ? a : b;
}

template <typename T, typename U>
constexpr std::common_type_t<T, U> ucb_max(T a, U b) noexcept
{
    static_assert(std::is_arithmetic<T>::value && std::is_arithmetic<U>::value,
                  "ucb_max: T and U must be arithmetic types");
    return (a > b) ? a : b;
}

template <typename T, typename U, typename V>
constexpr std::common_type_t<T, U, V> ucb_clamp(T value, U min, V max) noexcept
{
    static_assert(std::is_arithmetic<T>::value && std::is_arithmetic<U>::value &&
                      std::is_arithmetic<V>::value,
                  "ucb_clamp: T, U, and V must be arithmetic types");
    return ucb_min(ucb_max(value, min), max);
}

template <typename T, typename U, typename V>
constexpr bool ucb_in_range(T value, U min, V max)
{
    static_assert(std::is_arithmetic<T>::value && std::is_arithmetic<U>::value &&
                      std::is_arithmetic<V>::value,
                  "ucb_in_range: T, U, and V must be arithmetic types");
    return (value >= min) && (value <= max);
}

template <typename T>
constexpr void ucb_swap(T& a, T& b) noexcept
{
    T tmp = a;
    a     = b;
    b     = tmp;
}

template <typename T>
constexpr int ucb_sign(T val) noexcept
{
    static_assert(std::is_arithmetic<T>::value && std::is_signed<T>::value,
                  "ucb_sign: T must be a signed arithmetic type");
    return (val > 0) - (val < 0);
}

template <typename T>
constexpr T ucb_abs(T val) noexcept
{
    static_assert(std::is_arithmetic<T>::value && std::is_signed<T>::value,
                  "ucb_abs: T must be a signed arithmetic type");
    return (val < 0) ? -val : val;
}

template <typename T, typename U>
constexpr bool ucb_equal_eps(T a, U b, std::common_type_t<T, U> epsilon) noexcept
{
    static_assert(std::is_floating_point<T>::value && std::is_floating_point<U>::value,
                  "ucb_equal_eps: T and U must be floating point types");
    return ucb_abs(a - b) < epsilon;
}

template <typename T, typename U>
constexpr bool ucb_equal(T a, U b) noexcept
{
    using common_t         = std::common_type_t<T, U>;
    const common_t epsilon = std::is_same<common_t, float>::value
                                 ? UCB_EPSILON_F
                                 : UCB_EPSILON_D; // Default to double epsilon
    return ucb_equal_eps(a, b, epsilon);
}

template <typename T, typename U>
constexpr int ucb_comp_eps(T a, U b, std::common_type_t<T, U> epsilon) noexcept
{
    return ucb_equal_eps(a, b, epsilon) ? 0 : (a < b) ? -1 : 1;
}

template <typename T, typename U>
constexpr int ucb_comp(T a, U b) noexcept
{
    return ucb_equal(a, b) ? 0 : (a < b) ? -1 : 1;
}

template <typename T, typename U>
constexpr bool ucb_approx_equal_eps(T a, U b, std::common_type_t<T, U> epsilon) noexcept
{
    static_assert(std::is_floating_point<T>::value && std::is_floating_point<U>::value,
                  "ucb_approx_equal_eps: T and U must be floating point types");
    return ucb_abs(a - b) <= ucb_max(ucb_abs(a), ucb_abs(b)) * epsilon;
}

template <typename T, typename U>
constexpr bool ucb_approx_equal(T a, U b) noexcept
{
    using common_t         = std::common_type_t<T, U>;
    const common_t epsilon = std::is_same<common_t, float>::value
                                 ? UCB_EPSILON_F
                                 : UCB_EPSILON_D; // Default to double epsilon
    return ucb_approx_equal_eps(a, b, epsilon);
}

template <typename T, typename U>
constexpr int ucb_approx_comp_eps(T a, U b, std::common_type_t<T, U> epsilon) noexcept
{
    return ucb_approx_equal_eps(a, b, epsilon) ? 0 : (a < b) ? -1 : 1;
}

template <typename T, typename U>
constexpr int ucb_approx_comp(T a, U b) noexcept
{
    return ucb_approx_equal(a, b) ? 0 : (a < b) ? -1 : 1;
}

#endif // end __cplusplus

#endif // UCB_FUNC_H
