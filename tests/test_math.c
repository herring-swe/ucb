/**
 * @file test_math.c
 * 
 * This file is part of the UCB project
 * - SPDX-FileCopyrightText: © 2026 Åke Svedin <ake@svedin.org>
 * - SPDX-License-Identifier: MIT
 * 
 * @brief math C test implementation
 */

#include "test_math.h"

#include "ucb/diag.h"
#include "ucb/math.h"

#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#define test_calls(TYPE)                                                             \
    do                                                                               \
    {                                                                                \
        TYPE a = 1;                                                                  \
        TYPE b = 5;                                                                  \
        TYPE c = 10;                                                                 \
        if (ucb_max(a, b) != b)                                                      \
        {                                                                            \
            errs++;                                                                  \
            printf("ucb_max(%d, %d) != %d\n", (int)a, (int)b, (int)b);               \
        }                                                                            \
        if (ucb_min(a, b) != a)                                                      \
        {                                                                            \
            errs++;                                                                  \
            printf("ucb_min(%d, %d) != %d\n", (int)a, (int)b, (int)a);               \
        }                                                                            \
        if (ucb_clamp(a, b, c) != b)                                                 \
        {                                                                            \
            errs++;                                                                  \
            printf("ucb_clamp(%d, %d, %d) != %d\n", (int)a, (int)b, (int)c, (int)b); \
        }                                                                            \
        if (ucb_clamp(c, a, b) != b)                                                 \
        {                                                                            \
            errs++;                                                                  \
            printf("ucb_clamp(%d, %d, %d) != %d\n", (int)c, (int)a, (int)b, (int)b); \
        }                                                                            \
        if (ucb_in_range(a, b, c) != 0)                                              \
        {                                                                            \
            errs++;                                                                  \
            printf("ucb_in_range(%d, %d, %d) != 1\n", (int)b, (int)a, (int)c);       \
        }                                                                            \
        if (ucb_in_range(b, a, c) != 1)                                              \
        {                                                                            \
            errs++;                                                                  \
            printf("ucb_in_range(%d, %d, %d) != 1\n", (int)b, (int)a, (int)c);       \
        }                                                                            \
    } while (0)

#define test_calls_signed(TYPE)                             \
    do                                                      \
    {                                                       \
        TYPE a = 1;                                         \
        TYPE b = -1;                                        \
        if (ucb_sign(a) != a)                               \
        {                                                   \
            errs++;                                         \
            printf("ucb_sign(%d) != %d\n", (int)a, (int)a); \
        }                                                   \
        if (ucb_sign(b) != b)                               \
        {                                                   \
            errs++;                                         \
            printf("ucb_sign(%d) != %d\n", (int)b, (int)b); \
        }                                                   \
        if (ucb_sign(a) == b)                               \
        {                                                   \
            errs++;                                         \
            printf("ucb_sign(%d) == %d\n", (int)a, (int)b); \
        }                                                   \
        if (ucb_abs(a) != a)                                \
        {                                                   \
            errs++;                                         \
            printf("ucb_abs(%d) != %d\n", (int)a, (int)a);  \
        }                                                   \
        if (ucb_abs(b) != a)                                \
        {                                                   \
            errs++;                                         \
            printf("ucb_abs(%d) != %d\n", (int)b, (int)b);  \
        }                                                   \
        if (ucb_abs(a) == b)                                \
        {                                                   \
            errs++;                                         \
            printf("ucb_abs(%d) == %d\n", (int)a, (int)b);  \
        }                                                   \
    } while (0)

#define test_calls_float(TYPE)                                                       \
    do                                                                               \
    {                                                                                \
        TYPE a = 1;                                                                  \
        TYPE b = 5;                                                                  \
        TYPE c = 10;                                                                 \
        if (!ucb_equal(ucb_max(a, b), b))                                            \
        {                                                                            \
            errs++;                                                                  \
            printf("ucb_max(%d, %d) != %d\n", (int)a, (int)b, (int)b);               \
        }                                                                            \
        if (!ucb_equal(ucb_min(a, b), a))                                            \
        {                                                                            \
            errs++;                                                                  \
            printf("ucb_min(%d, %d) != %d\n", (int)a, (int)b, (int)a);               \
        }                                                                            \
        if (!ucb_equal(ucb_clamp(a, b, c), b))                                       \
        {                                                                            \
            errs++;                                                                  \
            printf("ucb_clamp(%d, %d, %d) != %d\n", (int)a, (int)b, (int)c, (int)b); \
        }                                                                            \
        if (!ucb_equal(ucb_clamp(c, a, b), b))                                       \
        {                                                                            \
            errs++;                                                                  \
            printf("ucb_clamp(%d, %d, %d) != %d\n", (int)c, (int)a, (int)b, (int)b); \
        }                                                                            \
        if (ucb_in_range(a, b, c) != 0)                                              \
        {                                                                            \
            errs++;                                                                  \
            printf("ucb_in_range(%d, %d, %d) != 1\n", (int)b, (int)a, (int)c);       \
        }                                                                            \
        if (ucb_in_range(b, a, c) != 1)                                              \
        {                                                                            \
            errs++;                                                                  \
            printf("ucb_in_range(%d, %d, %d) != 1\n", (int)b, (int)a, (int)c);       \
        }                                                                            \
    } while (0)

#define test_calls_signed_float(TYPE)                               \
    do                                                              \
    {                                                               \
        TYPE a = 1.f;                                               \
        TYPE b = -1.f;                                              \
        if (!ucb_equal((TYPE)ucb_sign(a), a))                       \
        {                                                           \
            errs++;                                                 \
            printf("ucb_sign(%g) != %g\n", a, a);                   \
        }                                                           \
        if (!ucb_equal((TYPE)ucb_sign(b), b))                       \
        {                                                           \
            errs++;                                                 \
            printf("ucb_sign(%g) != %g\n", b, b);                   \
        }                                                           \
        if (ucb_equal((TYPE)ucb_sign(a), b))                        \
        {                                                           \
            errs++;                                                 \
            printf("ucb_sign(%g) == %g (%d)\n", a, b, ucb_sign(a)); \
        }                                                           \
        if (!ucb_equal(ucb_abs(a), a))                              \
        {                                                           \
            errs++;                                                 \
            printf("ucb_abs(%g) != %g\n", a, a);                    \
        }                                                           \
        if (!ucb_equal(ucb_abs(b), a))                              \
        {                                                           \
            errs++;                                                 \
            printf("ucb_abs(%g) != %g\n", b, b);                    \
        }                                                           \
        if (ucb_equal(ucb_abs(a), b))                               \
        {                                                           \
            errs++;                                                 \
            printf("ucb_abs(%g) == %g\n", a, b);                    \
        }                                                           \
    } while (0)

int test_math(void)
{
    UCB_DIAG_PUSH()
    UCB_DIAG_IGN_DBL_PROM()

    int errs = 0;

    test_calls(char);
    test_calls(unsigned char);
    test_calls(signed char);
    test_calls(short);
    test_calls(unsigned short);
    test_calls(int);
    test_calls(unsigned int);
    test_calls(long);
    test_calls(unsigned long);
    test_calls(long long);
    test_calls(unsigned long long);
    test_calls_float(float);
    test_calls_float(double);

#if !UCB_CHAR_DUPLICATE && UCB_CHAR_IS_SIGNED
    test_calls_signed(char);
#endif
    test_calls_signed(signed char);
    test_calls_signed(short);
    test_calls_signed(int);
    test_calls_signed(long);
    test_calls_signed(long long);
    test_calls_signed_float(float);
    test_calls_signed_float(double);

    UCB_DIAG_POP()

    return errs;
}
