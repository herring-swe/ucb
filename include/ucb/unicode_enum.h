/**
 * @file unicode_enum.h
 * This file is part of the UCB project
 * - SPDX-FileCopyrightText: © 2026 Åke Svedin <ake@svedin.org>
 * - SPDX-License-Identifier: MIT
 *
 * @brief Unicode Enum
 */

#ifndef UCB_UNICODE_ENUM_H
#define UCB_UNICODE_ENUM_H

#include <ucb/export.h>

typedef enum ucb_norm_form
{
    UCB_NORM_INVALID = 0,
    /**
     * @brief NFD - Canonical Decomposition
     * Useful for case folding or searching (e.g., é → e + ´)
     */
    UCB_NORM_NFD,
    /**
     * @brief NFC - Canonical Composition
     * Same as NFD followed by canonical composition
     * Useful for reducing the size of the string (e.g., e + ´ → é)
     */
    UCB_NORM_NFC,
    /**
     * @brief NFKD - Compatibility Decomposition
     * As NFD, but also decomposes compatibility characters.
     * Useful for compatibility equivalence (e.g., ½ → 1/2).
     * NOTE: This is destructive, the produced string cannot be converted back to the original.
     */
    UCB_NORM_NFKD,
    /**
     * @brief NFKC - Compatibility Composition.
     * As NFC followed by a canonical composition (destructive).
     * NOTE: This is destructive, the produced string cannot be converted back to the original.
     */
    UCB_NORM_NFKC,
} ucb_norm_form;

/**
 * @brief Get the short name of a normalization form.
 * @param form the normalization form
 * @return a string literal ("NFC", "NFD", "NFKC" or "NFKD"), or an empty
 * string if @p form is not a known form
 */
UCB_API const char* ucb_uc_norm_form_to_str(ucb_norm_form form);

/**
 * @brief Parse a normalization form from its short name.
 *
 * The comparison is case-insensitive.
 * @param str the name to parse ("NFC", "NFD", "NFKC" or "NFKD")
 * @return the matching form, or UCB_NORM_INVALID if @p str is not recognized
 */
UCB_API ucb_norm_form ucb_uc_norm_form_from_str(const char* str);

#endif // UCB_UNICODE_ENUM_H
