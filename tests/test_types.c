#include "test_types.h"

#include "ucb/diag.h"
#include "ucb/math.h"

#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

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

    char buf[256];

    UCB_DIAG_PUSH
    UCB_DIAG_IGN_FORMAT_NONLITERAL
    UCB_DIAG_IGN_DBL_PROM

    sprintf(buf, "Test unsigned char = %s\n", UCB_FMT(val_uchar));
    printf(buf, val_uchar);

    sprintf(buf, "Test unsigned short = %s\n", UCB_FMT(val_ushort));
    printf(buf, val_ushort);

    sprintf(buf, "Test unsigned int = %s\n", UCB_FMT(val_uint));
    printf(buf, val_uint);

    sprintf(buf, "Test unsigned long = %s\n", UCB_FMT(val_ulong));
    printf(buf, val_ulong);

    sprintf(buf, "Test unsigned long long = %s\n", UCB_FMT(val_ullong));
    printf(buf, val_ullong);

    sprintf(buf, "Test char = %s\n", UCB_FMT(val_char));
    printf(buf, val_char);

    sprintf(buf, "Test short = %s\n", UCB_FMT(val_short));
    printf(buf, val_short);

    sprintf(buf, "Test int = %s\n", UCB_FMT(val_int));
    printf(buf, val_int);

    sprintf(buf, "Test long = %s\n", UCB_FMT(val_long));
    printf(buf, val_long);

    sprintf(buf, "Test long long = %s\n", UCB_FMT(val_llong));
    printf(buf, val_llong);

    sprintf(buf, "Test float = %s\n", UCB_FMT(val_float));
    printf(buf, val_float);

    sprintf(buf, "Test double = %s\n", UCB_FMT(val_double));
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

    sprintf(buf, "Test size_t = %s\n", UCB_FMT(val_size_t));
    printf(buf, val_size_t);

    sprintf(buf, "Test int8_t = %s\n", UCB_FMT(val_int8_t));
    printf(buf, val_int8_t);

    sprintf(buf, "Test int32_t = %s\n", UCB_FMT(val_int32_t));
    printf(buf, val_int32_t);

    sprintf(buf, "Test uint64_t = %s\n", UCB_FMT(val_uint64_t));
    printf(buf, val_uint64_t);

    UCB_DIAG_POP

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

    UCB_DIAG_PUSH
    UCB_DIAG_IGN_UNREACHABLE_CODE
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
    UCB_DIAG_POP
    return errs;
}
