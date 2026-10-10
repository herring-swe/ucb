/**
 * @file bytes.h
 *
 * This file is part of the UCB project
 * - SPDX-FileCopyrightText: © 2026 Åke Svedin <ake@svedin.org>
 * - SPDX-License-Identifier: MIT
 *
 * @brief Byte size units: human-readable formatting and parsing
 *
 * This module converts byte counts (see @ref ucb_bytes) to human-readable
 * strings and parses CLI-style size strings back into raw byte counts. It is a
 * standalone helper with no dependency on the filesystem or the argument
 * parser; other modules may build on it (for example a future argparse size
 * type).
 *
 * It lives under @c ucb/units/ so that other quantity converters (durations and
 * so on) can join the same family without sharing a prefix.
 *
 * ### Unit systems
 *
 * SI (decimal, powers of 1000) and IEC (binary, powers of 1024) share a single
 * @ref ucb_bytes_unit enum; the suffixes differ so the family is unambiguous:
 *
 * - SI: @c B, @c kB, @c MB, @c GB, @c TB, @c PB, @c EB. Note the lowercase
 *   @c k, which is the correct SI symbol.
 * - IEC: @c B, @c KiB, @c MiB, @c GiB, @c TiB, @c PiB, @c EiB.
 *
 * ### Formatting
 *
 * @ref ucb_bytes_convert scales a byte count to a unit, or chooses one
 * automatically with @ref UCB_BYTES_AUTO_SI or @ref UCB_BYTES_AUTO_IEC.
 * @ref ucb_bytes_to_str renders the result. Formatting always emits a single
 * space between the number and the suffix, and the byte suffix is always
 * @c B. @c precision is a required argument: @c 0 prints an integer and a
 * positive value prints that many decimals.
 *
 * ### Parsing
 *
 * @ref ucb_bytes_parse is CLI-friendly and accepts a decimal fraction, optional
 * surrounding whitespace and optional whitespace between the number and the
 * suffix. The suffix is case-insensitive:
 * - no suffix, or @c B, is bytes;
 * - a bare prefix letter (@c K, @c M, @c G, @c T, @c P, @c E) or an @c i form
 *   (@c Ki, @c KiB) is binary (IEC);
 * - a prefix letter followed by @c B (@c KB, @c MB) is decimal (SI).
 *
 * So @c M means MiB while @c MB means MB. Malformed input such as an unknown
 * suffix, a negative value or an exponent is reported as an error rather than
 * silently ignored.
 *
 * @section bytes_threads Thread safety
 *
 * These functions are pure and thread safe.
 */

#ifndef UCB_UNITS_BYTES_H
#define UCB_UNITS_BYTES_H

#include <ucb/defines.h>
#include <ucb/error.h>
#include <ucb/export.h>
#include <ucb/string.h>
#include <ucb/types.h>

#include <stdbool.h>
#include <stdint.h>

/**
 * @brief A byte size unit, decimal (SI) or binary (IEC)
 *
 * Concrete units are ordered so the value doubles as the index into the unit
 * table: the SI units @ref UCB_BYTES_B .. @ref UCB_BYTES_EB, then the IEC units
 * @ref UCB_BYTES_KIB .. @ref UCB_BYTES_EIB. The two negative sentinels request
 * automatic selection.
 */
typedef enum ucb_bytes_unit
{
    UCB_BYTES_AUTO_IEC = -2, ///< Pick the largest IEC unit that fits
    UCB_BYTES_AUTO_SI = -1,  ///< Pick the largest SI unit that fits

    UCB_BYTES_B = 0, ///< Byte
    UCB_BYTES_KB,    ///< Kilobyte (1000)
    UCB_BYTES_MB,    ///< Megabyte (1000^2)
    UCB_BYTES_GB,    ///< Gigabyte (1000^3)
    UCB_BYTES_TB,    ///< Terabyte (1000^4)
    UCB_BYTES_PB,    ///< Petabyte (1000^5)
    UCB_BYTES_EB,    ///< Exabyte (1000^6)

    UCB_BYTES_KIB, ///< Kibibyte (1024)
    UCB_BYTES_MIB, ///< Mebibyte (1024^2)
    UCB_BYTES_GIB, ///< Gibibyte (1024^3)
    UCB_BYTES_TIB, ///< Tebibyte (1024^4)
    UCB_BYTES_PIB, ///< Pebibyte (1024^5)
    UCB_BYTES_EIB, ///< Exbibyte (1024^6)
} ucb_bytes_unit;

/**
 * @struct ucb_bytes_value
 * @brief A byte size scaled into a unit
 *
 * Produced by @ref ucb_bytes_convert and consumed by @ref ucb_bytes_to_str.
 * The unit is a concrete unit, never one of the auto sentinels.
 */
typedef struct ucb_bytes_value
{
    double value;        ///< Scaled value in @ref unit
    ucb_bytes_unit unit; ///< The unit @ref value is expressed in
} ucb_bytes_value;

/**
 * @struct ucb_bytes_parsed
 * @brief The outcome of @ref ucb_bytes_parse
 *
 * @ref bytes is the fully resolved count and @ref unit is the unit that was
 * written (never an auto sentinel).
 */
typedef struct ucb_bytes_parsed
{
    ucb_bytes bytes;     ///< Resolved size in bytes
    ucb_bytes_unit unit; ///< The parsed unit
} ucb_bytes_parsed;

/* -------------------------------------------------------------------------- */
/*                                 Formatting                                 */
/* -------------------------------------------------------------------------- */

/**
 * @brief Scale a byte count into a unit.
 *
 * A concrete @p unit scales @p bytes into that unit. @ref UCB_BYTES_AUTO_SI or
 * @ref UCB_BYTES_AUTO_IEC picks the largest unit of that family that fits (a
 * value of @c 0 selects @ref UCB_BYTES_B).
 *
 * @param bytes size in bytes
 * @param unit a concrete @ref ucb_bytes_unit, or an auto sentinel
 * @return the scaled value and the concrete unit used
 */
UCB_API ucb_bytes_value ucb_bytes_convert(ucb_bytes bytes, ucb_bytes_unit unit);

/**
 * @brief Render a scaled byte size as a human-readable string.
 *
 * Produces, for example, @c "1.5 MB", @c "2 KiB" or @c "100 B". The result is
 * owned by the caller and released with @ref ucb_str_free.
 *
 * @param value the value and unit to render, @c value.unit must be concrete
 * @param precision decimals to print; @c 0 prints an integer, must be non-negative
 * @return a new string, or UCB_NULL on allocation failure
 */
UCB_API ucb_str* ucb_bytes_to_str(ucb_bytes_value value, int precision);

/**
 * Convencience wrapper for auto si string
 *
 * @param bytes size in bytes
 * @param precision decimals to print
 * @return a new string, or UCB_NULL on allocation failure
 */
static inline ucb_str* ucb_bytes_str_si(ucb_bytes bytes, int precision)
{
    return ucb_bytes_to_str(ucb_bytes_convert(bytes, UCB_BYTES_AUTO_SI), precision);
}

/**
 * Convencience wrapper for auto iec string
 *
 * @param bytes size in bytes
 * @param precision decimals to print
 * @return a new string, or UCB_NULL on allocation failure
 */
static inline ucb_str* ucb_bytes_str_iec(ucb_bytes bytes, int precision)
{
    return ucb_bytes_to_str(ucb_bytes_convert(bytes, UCB_BYTES_AUTO_IEC), precision);
}

/**
 * @brief Buffer size sufficient for a rendered byte size.
 *
 * Covers values such as @c "1023.99 EiB" plus the null terminator for any
 * reasonable @p precision (up to about 24 decimals). Used by @ref UCB_BYTES_CSTR.
 */
#define UCB_BYTES_STRLEN 32

/**
 * @brief Render a byte size into a caller-provided buffer.
 *
 * Converts @p bytes to @p unit (a concrete unit or @ref UCB_BYTES_AUTO_SI /
 * @ref UCB_BYTES_AUTO_IEC) and writes the human-readable result into @p buf.
 * The result is always null-terminated when @p size is non-zero; if it does
 * not fit, it is truncated.
 *
 * @param buf destination buffer, must be non-NULL
 * @param size size of @p buf in bytes, must be non-zero
 * @param bytes size in bytes
 * @param unit a concrete @ref ucb_bytes_unit, or an auto sentinel
 * @param precision decimals to print; @c 0 prints an integer, must be non-negative
 * @return @p buf
 */
UCB_API char* ucb_bytes_str_buf(char* buf,
                                size_t size,
                                ucb_bytes bytes,
                                ucb_bytes_unit unit,
                                int precision);

/**
 * @def UCB_BYTES_CSTR
 * @brief Render a byte size into a short-lived C string (C only).
 *
 * Expands to a @c char* backed by an unnamed stack buffer whose lifetime is the
 * enclosing block, so no allocation or @ref ucb_str_free is needed:
 *
 * @code
 * printf("%s", UCB_BYTES_CSTR(size, UCB_BYTES_AUTO_IEC, 2));
 * @endcode
 *
 * The pointer must not outlive the enclosing block. Multiple uses in one
 * expression each get their own buffer. Not available in C++ (compound
 * literals are non-standard there); C++ callers use @ref ucb_bytes_str_si,
 * @ref ucb_bytes_str_iec or @ref ucb_bytes_to_str instead.
 *
 * @param bytes size in bytes
 * @param unit a concrete @ref ucb_bytes_unit, or an auto sentinel
 * @param precision decimals to print; @c 0 prints an integer
 */
#ifndef __cplusplus
#define UCB_BYTES_CSTR(bytes, unit, precision) \
    ucb_bytes_str_buf((char[UCB_BYTES_STRLEN]){0}, UCB_BYTES_STRLEN, (bytes), (unit), (precision))
#endif

/* -------------------------------------------------------------------------- */
/*                                   Parsing                                  */
/* -------------------------------------------------------------------------- */

/**
 * @brief Parse a human-readable byte size into a raw byte count.
 *
 * Accepts an optional decimal fraction, optional surrounding whitespace and
 * optional whitespace between the number and the unit. The unit is
 * case-insensitive: no unit or @c B is bytes, a bare prefix letter or an @c i
 * form is binary (IEC) and a prefix followed by @c B is decimal (SI), so @c M
 * is MiB and @c MB is MB. Exponent notation is not accepted.
 *
 * On success @p out receives the rounded byte count and the parsed unit. On
 * failure the function returns false and throws into @p perr:
 * - @ref UCB_ERROR_INVALID_ARG for malformed input or an unknown suffix;
 * - @ref UCB_ERROR_OUT_OF_BOUNDS when the result does not fit a @ref ucb_bytes.
 *
 * @param str the string to parse, must be non-NULL
 * @param out destination for the result, must be non-NULL
 * @param perr optional location to store the error on failure
 * @return true on success, false on parse error
 */
UCB_API bool ucb_bytes_parse(const char* str, ucb_bytes_parsed* out, ucb_error** perr);

#endif // UCB_UNITS_BYTES_H
