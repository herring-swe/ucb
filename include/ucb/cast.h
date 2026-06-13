#ifndef UCB_CAST_H
#define UCB_CAST_H

/**
 * @file cast.h
 * This file is part of the UCB project
 * - SPDX-FileCopyrightText: © 2026 Åke Svedin <ake@svedin.org>
 * - SPDX-License-Identifier: MIT
 *
 * @brief Provides type casting utilities
 */

#include <stddef.h>
#include <stdint.h>

#if defined(__GNUC__) || defined(__clang__)
#define UCB_WITH_CAST_DBL_TO_PTR (__SIZEOF_DOUBLE__ <= __SIZEOF_POINTER__)
#elif defined(_WIN64)
#define UCB_WITH_CAST_DBL_TO_PTR 1
#else // let's not assume
#define UCB_WITH_CAST_DBL_TO_PTR 0
#endif

typedef union ucb_cast_union
{
    void* ptr;
    float f;
    double d;
} ucb_cast_union;

/* -------------------------------------------------------------------------- */

/**
 * @brief Cast int to void*
 * @param val The value to cast
 * @return void* representation of int
 */
static inline void* ucb_int2ptr(int val)
{
    return (void*)(intptr_t)val;
}

/**
 * @brief Cast void* to int
 * @param ptr The pointer to cast
 * @return int representation of pointer
 */
static inline int ucb_ptr2int(void* ptr)
{
    return (int)(intptr_t)ptr;
}

/* -------------------------------------------------------------------------- */

/**
 * @brief Cast uintptr_t to void* - for integer-to-pointer conversion
 * @param val The pointer to cast
 * @return void* representation of unsigned int
 */
static inline void* ucb_uint2ptr(unsigned int val)
{
    return (void*)(uintptr_t)val;
}

/**
 * @brief Cast void* to unsigned int
 * @param ptr The pointer to cast
 * @return unsigned int representation of pointer
 */
static inline unsigned int ucb_ptr2uint(void* ptr)
{
    return (unsigned int)(uintptr_t)ptr;
}

/* -------------------------------------------------------------------------- */

/**
 * @brief Cast float to void*
 *
 * This will take the bit pattern of the float. To get the original float back,
 * use ucb_ptr2flt.
 * @param val The value to cast
 * @return void* representation of float
 */
static inline void* ucb_flt2ptr(float val)
{
    ucb_cast_union u;
    u.f = val;
    return u.ptr;
}

/**
 * @brief Cast void* to float
 *
 * This convert an address bit pattern as a float. Used in conjuction with ucb_ptr2flt.
 * @param ptr The pointer to cast
 * @return float representation of pointer
 */
static inline float ucb_ptr2flt(void* ptr)
{
    ucb_cast_union u;
    u.ptr = ptr;
    return u.f;
}

/* -------------------------------------------------------------------------- */

#if UCB_WITH_CAST_DBL_TO_PTR

/**
 * @brief Cast double to void*
 *
 * This will take the bit pattern of the double. To get the original double back,
 * use ucb_ptr2dbl.
 * @param val The value to cast
 * @return void* representation of double
 */
static inline void* ucb_dbl2ptr(double val)
{
    ucb_cast_union u;
    u.d = val;
    return u.ptr;
}

/**
 * @brief Cast void* to double
 *
 * This convert an address bit pattern as a double. Used in conjuction with ucb_dbl2ptr.
 * @param ptr The pointer to cast
 * @return double representation of pointer
 */
static inline double ucb_ptr2dbl(void* ptr)
{
    ucb_cast_union u;
    u.ptr = ptr;
    return u.d;
}

#endif // UCB_WITH_CAST_DBL_TO_PTR

#endif // UCB_CAST_H
