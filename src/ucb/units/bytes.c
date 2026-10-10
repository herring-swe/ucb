/**
 * @file bytes.c
 *
 * This file is part of the UCB project
 * - SPDX-FileCopyrightText: © 2026 Åke Svedin <ake@svedin.org>
 * - SPDX-License-Identifier: MIT
 *
 * @brief Byte size units implementation
 */

#include "ucb/units/bytes.h"

#include "ucb/cstring.h"
#include "ucb/memory.h"

#include <ctype.h>
#include <errno.h>
#include <math.h>
#include <stdlib.h>
#include <string.h>

/* -------------------------------------------------------------------------- */
/*                                   Tables                                   */
/* -------------------------------------------------------------------------- */

typedef struct bytes_def
{
    const char* suffix; ///< Printed suffix ("B", "kB", "KiB", ...)
    double factor;      ///< Value in bytes of one unit
} bytes_def;

/* Indexed by the concrete ucb_bytes_unit value: SI units then IEC units. */
static const bytes_def BYTES_UNITS[13] = {
    {"B", 1.0},
    {"kB", 1e3},
    {"MB", 1e6},
    {"GB", 1e9},
    {"TB", 1e12},
    {"PB", 1e15},
    {"EB", 1e18},
    {"KiB", 1024.0},
    {"MiB", 1048576.0},
    {"GiB", 1073741824.0},
    {"TiB", 1099511627776.0},
    {"PiB", 1125899906842624.0},
    {"EiB", 1152921504606846976.0},
};

/* -------------------------------------------------------------------------- */
/*                                 Formatting                                 */
/* -------------------------------------------------------------------------- */

ucb_bytes_value ucb_bytes_convert(ucb_bytes bytes, ucb_bytes_unit unit)
{
    int idx;

    if (unit == UCB_BYTES_AUTO_SI)
    {
        idx = UCB_BYTES_B;
        for (int i = UCB_BYTES_EB; i > UCB_BYTES_B; i--)
        {
            if ((double)bytes >= BYTES_UNITS[i].factor)
            {
                idx = i;
                break;
            }
        }
    }
    else if (unit == UCB_BYTES_AUTO_IEC)
    {
        idx = UCB_BYTES_B;
        for (int i = UCB_BYTES_EIB; i >= UCB_BYTES_KIB; i--)
        {
            if ((double)bytes >= BYTES_UNITS[i].factor)
            {
                idx = i;
                break;
            }
        }
    }
    else
    {
        UCB_VERIFY(unit >= UCB_BYTES_B && unit <= UCB_BYTES_EIB,
                   UCB_ERROR_INVALID_ARG,
                   "Invalid byte unit");
        idx = (int)unit;
    }

    ucb_bytes_value result;
    result.unit = (ucb_bytes_unit)idx;
    result.value = (double)bytes / BYTES_UNITS[idx].factor;
    return result;
}

ucb_str* ucb_bytes_to_str(ucb_bytes_value value, int precision)
{
    UCB_VERIFY_ARGS(value.unit >= UCB_BYTES_B && value.unit <= UCB_BYTES_EIB);
    UCB_VERIFY(precision >= 0, UCB_ERROR_INVALID_ARG, "precision must be non-negative");

    char* tmp = UCB_NULL;
    int n = ucb_cstr_asprintf(&tmp,
                              "%.*f %s",
                              precision,
                              value.value,
                              BYTES_UNITS[(int)value.unit].suffix);
    if (n < 0 || !tmp)
        return UCB_NULL;

    ucb_str* str = ucb_str_new_empty();
    if (!str)
    {
        ucb_free(tmp);
        return UCB_NULL;
    }
    ucb_str_adopt_c(str, tmp);
    return str;
}

/* -------------------------------------------------------------------------- */
/*                                   Parsing                                  */
/* -------------------------------------------------------------------------- */

static bool bytes_is_space(char c)
{
    return (c == ' ' || c == '\t') != 0;
}

static bool bytes_is_digit(char c)
{
    return (c >= '0' && c <= '9') != 0;
}

/* 2^64 as a double; the first value a ucb_bytes cannot hold. */
static const double BYTES_LIMIT = 18446744073709551616.0;

bool ucb_bytes_parse(const char* str, ucb_bytes_parsed* out, ucb_error** perr)
{
    UCB_VERIFY_ARGS(str && out);

    const char* p = str;
    while (bytes_is_space(*p))
        p++;

    const char* num_start = p;
    if (!bytes_is_digit(*p))
    {
        ucb_throw(perr, UCB_ERROR_INVALID_ARG, "size must start with a digit");
        return false;
    }

    bool seen_dot = false;
    while (bytes_is_digit(*p) || (*p == '.' && !seen_dot))
    {
        if (*p == '.')
            seen_dot = true;
        p++;
    }
    const char* num_end = p;
    if (num_end[-1] == '.')
    {
        ucb_throw(perr, UCB_ERROR_INVALID_ARG, "size has a trailing decimal point");
        return false;
    }

    /* The scan already limited the number to digits and one dot, so strtod
       cannot wander into an exponent or hex form; compare its end exactly. */
    errno = 0;
    char* end = UCB_NULL;
    double value = strtod(num_start, &end);
    if (end != num_end)
    {
        ucb_throw(perr, UCB_ERROR_INVALID_ARG, "invalid size number");
        return false;
    }
    if (errno == ERANGE || !isfinite(value))
    {
        ucb_throw(perr, UCB_ERROR_OUT_OF_BOUNDS, "size does not fit a 64-bit count");
        return false;
    }

    while (bytes_is_space(*p))
        p++;

    bool binary = false;
    int power = 0;

    /* Collect the suffix letters so trailing whitespace is handled uniformly. */
    char suffix[8];
    size_t slen = 0;
    while (isalpha((unsigned char)*p))
    {
        if (slen + 1 >= sizeof(suffix))
        {
            ucb_throw(perr, UCB_ERROR_INVALID_ARG, "size unit is too long");
            return false;
        }
        suffix[slen++] = (char)tolower((unsigned char)*p);
        p++;
    }
    suffix[slen] = '\0';

    while (bytes_is_space(*p))
        p++;
    if (*p != '\0')
    {
        ucb_throw(perr, UCB_ERROR_INVALID_ARG, "unexpected characters after size unit");
        return false;
    }

    if (slen == 0 || (slen == 1 && suffix[0] == 'b'))
    {
        /* Bare number or "B": bytes. */
    }
    else
    {
        static const char PREFIXES[] = "kmgtpe";
        const char* found = strchr(PREFIXES, suffix[0]);
        if (!found)
        {
            ucb_throw(perr, UCB_ERROR_INVALID_ARG, "unknown size unit");
            return false;
        }
        power = (int)(found - PREFIXES) + 1;

        const char* rest = suffix + 1;
        size_t rlen = slen - 1;
        if (rlen == 0 || (rlen == 1 && rest[0] == 'i') ||
            (rlen == 2 && rest[0] == 'i' && rest[1] == 'b'))
        {
            binary = true; /* "K", "Ki" or "KiB" -> binary */
        }
        else if (rlen == 1 && rest[0] == 'b')
        {
            binary = false; /* "KB" -> decimal */
        }
        else
        {
            ucb_throw(perr, UCB_ERROR_INVALID_ARG, "invalid size unit");
            return false;
        }
    }

    int idx = binary ? (UCB_BYTES_KIB + power - 1) : power;
    double factor = BYTES_UNITS[idx].factor;

    double scaled = value * factor;
    double rounded = floor(scaled + 0.5);
    if (!(scaled >= 0.0) || rounded >= BYTES_LIMIT)
    {
        ucb_throw(perr, UCB_ERROR_OUT_OF_BOUNDS, "size does not fit a 64-bit count");
        return false;
    }

    out->bytes = (ucb_bytes)rounded;
    out->unit = (ucb_bytes_unit)idx;
    return true;
}
