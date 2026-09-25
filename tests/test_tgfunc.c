/**
 * @file test_tgfunc.c
 *
 * This file is part of the UCB project
 * - SPDX-FileCopyrightText: © 2026 Åke Svedin <ake@svedin.org>
 * - SPDX-License-Identifier: MIT
 *
 * @brief tgfunc C test implementation
 */

#include "test_tgfunc.h"

#include "ucb/diag.h"
#include "ucb/tgfunc.h"

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

static int test_str(const char* got, const char* expected, const char* what)
{
    if (strcmp(got, expected) != 0)
    {
        printf("generics: %s: got \"%s\", expected \"%s\"\n", what, got, expected);
        return 1;
    }
    return 0;
}

int test_tgfunc_generics(void)
{
    int errs = 0;

    // UCB_TYPE string selection
    errs += test_str(UCB_TYPE((char)0), "char", "UCB_TYPE char");
    errs += test_str(UCB_TYPE((signed char)0), "signed char", "UCB_TYPE signed char");
    errs += test_str(UCB_TYPE((unsigned char)0), "unsigned char", "UCB_TYPE unsigned char");
    errs += test_str(UCB_TYPE((short)0), "short", "UCB_TYPE short");
    errs += test_str(UCB_TYPE((unsigned short)0), "unsigned short", "UCB_TYPE unsigned short");
    errs += test_str(UCB_TYPE((int)0), "int", "UCB_TYPE int");
    errs += test_str(UCB_TYPE((unsigned int)0), "unsigned int", "UCB_TYPE unsigned int");
    errs += test_str(UCB_TYPE((long)0), "long", "UCB_TYPE long");
    errs += test_str(UCB_TYPE((unsigned long)0), "unsigned long", "UCB_TYPE unsigned long");
    errs += test_str(UCB_TYPE((long long)0), "long long", "UCB_TYPE long long");
    errs += test_str(UCB_TYPE((unsigned long long)0),
                     "unsigned long long",
                     "UCB_TYPE unsigned long long");
    errs += test_str(UCB_TYPE((float)0), "float", "UCB_TYPE float");
    errs += test_str(UCB_TYPE((double)0), "double", "UCB_TYPE double");
    errs += test_str(UCB_TYPE((long double)0), "ldouble", "UCB_TYPE long double");
    errs += test_str(UCB_TYPE((void*)0), "pointer", "UCB_TYPE pointer");
    errs += test_str(UCB_TYPE((char*)0), "string", "UCB_TYPE string");
    errs += test_str(UCB_TYPE((wchar_t*)0), "wstring", "UCB_TYPE wstring");

    // UCB_FMT format selection
    errs += test_str(UCB_FMT((char)0), "%hhi", "UCB_FMT char");
    errs += test_str(UCB_FMT((signed char)0), "%hhi", "UCB_FMT signed char");
    errs += test_str(UCB_FMT((unsigned char)0), "%hhu", "UCB_FMT unsigned char");
    errs += test_str(UCB_FMT((short)0), "%hi", "UCB_FMT short");
    errs += test_str(UCB_FMT((unsigned short)0), "%hu", "UCB_FMT unsigned short");
    errs += test_str(UCB_FMT((int)0), "%i", "UCB_FMT int");
    errs += test_str(UCB_FMT((unsigned int)0), "%u", "UCB_FMT unsigned int");
    errs += test_str(UCB_FMT((long)0), "%li", "UCB_FMT long");
    errs += test_str(UCB_FMT((unsigned long)0), "%lu", "UCB_FMT unsigned long");
    errs += test_str(UCB_FMT((long long)0), "%lli", "UCB_FMT long long");
    errs += test_str(UCB_FMT((unsigned long long)0), "%llu", "UCB_FMT unsigned long long");
    errs += test_str(UCB_FMT((float)0), "%g", "UCB_FMT float");
    errs += test_str(UCB_FMT((double)0), "%g", "UCB_FMT double");
    errs += test_str(UCB_FMT((void*)0), "%p", "UCB_FMT pointer");
    errs += test_str(UCB_FMT((char*)0), "%s", "UCB_FMT string");
    errs += test_str(UCB_FMT((wchar_t*)0), "%ls", "UCB_FMT wstring");

    // UCB_FMTS single-character / string format selection
    errs += test_str(UCB_FMTS((char)0), "%c", "UCB_FMTS char");
    errs += test_str(UCB_FMTS((wchar_t)0), "%lc", "UCB_FMTS wchar_t");
    errs += test_str(UCB_FMTS((unsigned char)0), "%c", "UCB_FMTS unsigned char");
    errs += test_str(UCB_FMTS((char*)0), "%s", "UCB_FMTS string");
    errs += test_str(UCB_FMTS((wchar_t*)0), "%ls", "UCB_FMTS wstring");

    return errs;
}

int test_tgfunc(void)
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
