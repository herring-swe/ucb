/**
 * This file is part of the UCB project
 * SPDX-FileCopyrightText: © 2025 Your Name <your@email.com>
 * SPDX-License-Identifier: MIT
 *
 * @brief
 */

#ifndef UCB_GENERICS_H
#define UCB_GENERICS_H

#ifndef __cplusplus

#include <inttypes.h>
#include <limits.h>
#include <stddef.h>
#include <stdint.h>
#include <wchar.h>

#define IS_SIGNED(T) (((T) - (T)1) < (T)0)

#define UCB_TYPE(a)                               \
    _Generic((a),                                 \
        char: "char",                             \
        signed char: "signed char",               \
        unsigned char: "unsigned char",           \
        short: "short",                           \
        unsigned short: "unsigned short",         \
        int: "int",                               \
        unsigned int: "unsigned int",             \
        long: "long",                             \
        unsigned long: "unsigned long",           \
        long long: "long long",                   \
        unsigned long long: "unsigned long long", \
        float: "float",                           \
        double: "double",                         \
        long double: "ldouble",                   \
        void*: "pointer",                         \
        char*: "string",                          \
        wchar_t*: "wstring")

#define UCB_FMT_UCHAR "%uc"
#define UCB_FMT_CHAR  "%c"

#define UCB_FMT2(a) (_Generic(a, char: UCB_FMT_CHAR))

#define UCB_FMT(a)                  \
    _Generic((a),                   \
        char: "%hhi",               \
        signed char: "%hhi",        \
        unsigned char: "%hhu",      \
        short: "%hi",               \
        unsigned short: "%hu",      \
        int: "%i",                  \
        unsigned int: "%u",         \
        long: "%li",                \
        unsigned long: "%lu",       \
        long long: "%lli",          \
        unsigned long long: "%llu", \
        float: "%g",                \
        double: "%g",               \
        void*: "%p",                \
        char*: "%s",                \
        wchar_t*: "%ls")

#define UCB_FMTS(a) \
    _Generic((a), char: "%c", wchar_t: "%lc", unsigned char: "%uc", char*: "%s", wchar_t*: "%ls")

#define UCB_GENERIC_FUNC_1(NAME, a)     \
    _Generic((a),                       \
        char: NAME##_hhi,               \
        short: NAME##_hi,               \
        int: NAME##_i,                  \
        long: NAME##_li,                \
        long long: NAME##_lli,          \
        unsigned char: NAME##_hhu,      \
        unsigned short: NAME##_hu,      \
        unsigned int: NAME##_u,         \
        unsigned long: NAME##_lu,       \
        unsigned long long: NAME##_llu, \
        float: NAME##_f,                \
        double: NAME##_d)(a)

#define UCB_GENERIC_FUNC_2(NAME, a, b)  \
    _Generic((a) + (b),                 \
        char: NAME##_hhi,               \
        unsigned char: NAME##_hhu,      \
        short: NAME##_hi,               \
        unsigned short: NAME##_hu,      \
        int: NAME##_i,                  \
        unsigned int: NAME##_u,         \
        long: NAME##_li,                \
        unsigned long: NAME##_lu,       \
        long long: NAME##_lli,          \
        unsigned long long: NAME##_llu, \
        float: NAME##_f,                \
        double: NAME##_d)(a, b)

#define UCB_GENERIC_FUNC_3(NAME, a, b, c) \
    _Generic((a) + (b) + (c),             \
        char: NAME##_hhi,                 \
        unsigned char: NAME##_hhu,        \
        short: NAME##_hi,                 \
        unsigned short: NAME##_hu,        \
        int: NAME##_i,                    \
        unsigned int: NAME##_u,           \
        long: NAME##_li,                  \
        unsigned long: NAME##_lu,         \
        long long: NAME##_lli,            \
        unsigned long long: NAME##_llu,   \
        float: NAME##_f,                  \
        double: NAME##_d)(a, b, c)

#if CHAR_MIN < 0
#define UCB_GENERIC_FUNC_SIGNED_1(NAME, a) \
    _Generic((a),                          \
        char: NAME##_hhi,                  \
        signed char: NAME##_hhi,           \
        short: NAME##_hi,                  \
        int: NAME##_i,                     \
        long: NAME##_li,                   \
        long long: NAME##_lli,             \
        float: NAME##_f,                   \
        double: NAME##_d)(a)
#else
#define UCB_GENERIC_FUNC_SIGNED_1(NAME, a) \
    _Generic((a),                          \
        signed char: NAME##_hhi,           \
        short: NAME##_hi,                  \
        int: NAME##_i,                     \
        long: NAME##_li,                   \
        long long: NAME##_lli,             \
        float: NAME##_f,                   \
        double: NAME##_d)(a)
#endif

#define UCB_GENERIC_FUNC_FLOAT_1(NAME, a) _Generic((a), float: NAME##_f, double: NAME##_d)(a)
#define UCB_GENERIC_FUNC_FLOAT_2(NAME, a, b) \
    _Generic((a) + (b), float: NAME##_f, double: NAME##_d)(a, b)
#define UCB_GENERIC_FUNC_FLOAT_3(NAME, a, b, c) \
    _Generic((a) + (b) + (c), float: NAME##_f, double: NAME##_d)(a, b, c)

#endif // __cplusplus

#endif // UCB_GENERICS_H
