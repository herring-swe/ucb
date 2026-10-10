/**
 * @file test_bytes_c.c
 *
 * This file is part of the UCB project
 * - SPDX-FileCopyrightText: © 2026 Åke Svedin <ake@svedin.org>
 * - SPDX-License-Identifier: MIT
 *
 * @brief bytes C tests implementation
 *
 * Compiled as C to exercise the C-only @ref UCB_BYTES_CSTR macro.
 */

#include "test_bytes_c.h"

#include "ucb/units/bytes.h"

#include <string.h>

int test_bytes_c(void)
{
    int errs = 0;

    /* Usable directly as an expression, no allocation or free. */
    if (strcmp(UCB_BYTES_CSTR(1000, UCB_BYTES_AUTO_SI, 0), "1 kB") != 0)
        errs++;

    if (strcmp(UCB_BYTES_CSTR(1536, UCB_BYTES_AUTO_IEC, 1), "1.5 KiB") != 0)
        errs++;

    if (strcmp(UCB_BYTES_CSTR(2000000, UCB_BYTES_MB, 2), "2.00 MB") != 0)
        errs++;

    /* Two uses in one expression each keep their own buffer. */
    const char* a = UCB_BYTES_CSTR(1000, UCB_BYTES_AUTO_SI, 0);
    const char* b = UCB_BYTES_CSTR(1024, UCB_BYTES_AUTO_IEC, 0);
    if (strcmp(a, "1 kB") != 0)
        errs++;
    if (strcmp(b, "1 KiB") != 0)
        errs++;
    if (a == b)
        errs++;

    return errs;
}
