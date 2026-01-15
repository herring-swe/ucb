/**
 * @file test_types.c
 *
 * This file is part of the UCB project
 * - SPDX-FileCopyrightText: © 2026 Åke Svedin <ake@svedin.org>
 * - SPDX-License-Identifier: MIT
 *
 * @brief types C tests implementation
 */

#include "test_types.h"

#include "ucb/cstring.h"
#include "ucb/defines.h"
#include "ucb/diag.h"
#include "ucb/tgfunc.h"

#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#define BUFSIZE 256

int test_types(void)
{
    int errs = 0;

    unsigned char val_uchar       = 1;
    unsigned short val_ushort     = 2;
    unsigned int val_uint         = 3;
    unsigned long val_ulong       = 4;
    unsigned long long val_ullong = 5;
    char val_char                 = 6;
    short val_short               = 7;
    int val_int                   = 8;
    long val_long                 = 9;
    long long val_llong           = 10;
    float val_float               = 11.0f;
    double val_double             = 12.0;
    long double val_ldouble       = 13.0L;

    char* val_pchar       = "14";
    wchar_t* val_pwchar_t = L"15";
    void* val_pvoid       = NULL;

    char buf[BUFSIZE];

    UCB_DIAG_PUSH()
    UCB_DIAG_IGN_FORMAT_NONLITERAL()
    UCB_DIAG_IGN_DBL_PROM()

    ucb_cstr_sprintf(buf, BUFSIZE, "Test unsigned char = %s\n", UCB_FMT(val_uchar));
    printf(buf, val_uchar);

    ucb_cstr_sprintf(buf, BUFSIZE, "Test unsigned short = %s\n", UCB_FMT(val_ushort));
    printf(buf, val_ushort);

    ucb_cstr_sprintf(buf, BUFSIZE, "Test unsigned int = %s\n", UCB_FMT(val_uint));
    printf(buf, val_uint);

    ucb_cstr_sprintf(buf, BUFSIZE, "Test unsigned long = %s\n", UCB_FMT(val_ulong));
    printf(buf, val_ulong);

    ucb_cstr_sprintf(buf, BUFSIZE, "Test unsigned long long = %s\n", UCB_FMT(val_ullong));
    printf(buf, val_ullong);

    ucb_cstr_sprintf(buf, BUFSIZE, "Test char = %s\n", UCB_FMT(val_char));
    printf(buf, val_char);

    ucb_cstr_sprintf(buf, BUFSIZE, "Test short = %s\n", UCB_FMT(val_short));
    printf(buf, val_short);

    ucb_cstr_sprintf(buf, BUFSIZE, "Test int = %s\n", UCB_FMT(val_int));
    printf(buf, val_int);

    ucb_cstr_sprintf(buf, BUFSIZE, "Test long = %s\n", UCB_FMT(val_long));
    printf(buf, val_long);

    ucb_cstr_sprintf(buf, BUFSIZE, "Test long long = %s\n", UCB_FMT(val_llong));
    printf(buf, val_llong);

    ucb_cstr_sprintf(buf, BUFSIZE, "Test float = %s\n", UCB_FMT(val_float));
    printf(buf, val_float);

    ucb_cstr_sprintf(buf, BUFSIZE, "Test double = %s\n", UCB_FMT(val_double));
    printf(buf, val_double);

    size_t val_size_t   = 1;
    uint8_t val_uint8_t = 2;
    // uint16_t val_uint16_t   = 3;
    uint32_t val_uint32_t = 4;
    uint64_t val_uint64_t = 5;
    int8_t val_int8_t     = 6;
    // int16_t val_int16_t     = 7;
    int32_t val_int32_t   = 8;
    int64_t val_int64_t   = 9;
    intptr_t val_intptr_t = 10;
    // uintptr_t val_uintptr_t = 11;

    ucb_cstr_sprintf(buf, BUFSIZE, "Test size_t = %s\n", UCB_FMT(val_size_t));
    printf(buf, val_size_t);

    ucb_cstr_sprintf(buf, BUFSIZE, "Test int8_t = %s\n", UCB_FMT(val_int8_t));
    printf(buf, val_int8_t);

    ucb_cstr_sprintf(buf, BUFSIZE, "Test int32_t = %s\n", UCB_FMT(val_int32_t));
    printf(buf, val_int32_t);

    ucb_cstr_sprintf(buf, BUFSIZE, "Test uint64_t = %s\n", UCB_FMT(val_uint64_t));
    printf(buf, val_uint64_t);

    UCB_DIAG_POP()

    UCB_UNUSED(val_intptr_t);
    UCB_UNUSED(val_uint32_t);
    UCB_UNUSED(val_pwchar_t);
    UCB_UNUSED(val_ldouble);
    UCB_UNUSED(val_uint8_t);
    UCB_UNUSED(val_pvoid);
    UCB_UNUSED(val_pchar);
    UCB_UNUSED(val_int64_t);

    printf("Test unsigned char = %s\n", UCB_TYPE(val_uchar));
    printf("Test short = %s\n", UCB_TYPE(val_short));
    printf("Test unsigned short = %s\n", UCB_TYPE(val_ushort));
    printf("Test int = %s\n", UCB_TYPE(val_int));
    printf("Test unsigned int = %s\n", UCB_TYPE(val_uint));
    printf("Test long = %s\n", UCB_TYPE(val_long));
    printf("Test unsigned long = %s\n", UCB_TYPE(val_ulong));
    printf("Test long long = %s\n", UCB_TYPE(val_llong));
    printf("Test unsigned long long = %s\n", UCB_TYPE(val_ullong));
    printf("Test float = %s\n", UCB_TYPE(val_float));
    printf("Test double = %s\n", UCB_TYPE(val_double));
    printf("Test long double = %s\n", UCB_TYPE(val_ldouble));
    printf("Test char* = %s\n", UCB_TYPE(val_pchar));
    printf("Test wchar_t* = %s\n", UCB_TYPE(val_pwchar_t));
    printf("Test void* = %s\n", UCB_TYPE(val_pvoid));
    printf("Test size_t = %s\n", UCB_TYPE(val_size_t));
    printf("Test int8_t = %s\n", UCB_TYPE(val_int8_t));
    printf("Test int32_t = %s\n", UCB_TYPE(val_int32_t));
    printf("Test int64_t = %s\n", UCB_TYPE(val_int64_t));
    printf("Test uint8_t = %s\n", UCB_TYPE(val_uint8_t));
    printf("Test uint32_t = %s\n", UCB_TYPE(val_uint32_t));
    printf("Test uint64_t = %s\n", UCB_TYPE(val_uint64_t));
    printf("Test intptr_t = %s\n", UCB_TYPE(val_intptr_t));

    UCB_DIAG_PUSH()
    UCB_DIAG_IGN_UNREACHABLE_CODE()
#if defined(_WIN32) // LLP64
#if defined(_WIN64)
    if (strcmp(UCB_TYPE(val_size_t), "unsigned long long") != 0)
        errs++;
#endif
#else // LP64
#if defined(__x86_64__)
    if (strcmp(UCB_TYPE(val_size_t), "unsigned long") != 0)
        errs++;
#endif
#endif
    UCB_DIAG_POP()
    return errs;
}
