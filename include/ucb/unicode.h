/**
 * This file is part of the UCB project
 * SPDX-FileCopyrightText: © 2025 Åke Svedin <ake@svedin.org>
 * SPDX-License-Identifier: MIT
 *
 * @brief Contains general unicode string operations
 * Operates on UTF-8 strings, unless specifically documented.
 */

#ifndef UCB_UNICODE_H
#define UCB_UNICODE_H

#include "buffer.h"
#include "diag.h"
#include "export.h"
#include "types.h"

#include <stddef.h>

#define UCB_NOCP 0xFFFF

UCB_DIAG_PUSH
UCB_DIAG_IGN_PADDED
typedef struct ucb_uc_result
{
    /**
     * @brief The resulting NULL-terminated string or UCB_NULL on error.
     * The string must be free'd with ucb_free by the caller.
     */
    char* data;
    /**
     * @brief The size of the resulting string, excluding the NULL-terminator.
     */
    size_t size;
} ucb_uc_result;
UCB_DIAG_POP

typedef enum ucb_norm_form
{
    UCB_UC_NORM_INVALID = 0,
    /**
     * @brief NFD - Canonical Decomposition
     * Useful for case folding or searching (e.g., é → e + ´)
     */
    UCB_UC_NORM_NFD,
    /**
     * @brief NFC - Canonical Composition
     * Same as NFD followed by canonical composition
     * Useful for reducing the size of the string (e.g., e + ´ → é)
     */
    UCB_UC_NORM_NFC,
    /**
     * @brief NFKD - Compatibility Decomposition
     * As NFD, but also decomposes compatibility characters.
     * Useful for compatibility equivalence (e.g., ½ → 1/2).
     * NOTE: This is destructive, the produced string cannot be converted back to the original.
     */
    UCB_UC_NORM_NFKD,
    /**
     * @brief NFKC - Compatibility Composition.
     * As NFC followed by a canonical composition (destructive).
     * NOTE: This is destructive, the produced string cannot be converted back to the original.
     */
    UCB_UC_NORM_NFKC,
} ucb_norm_form;

UCB_API const char* ucb_uc_norm_form_to_str(ucb_norm_form form);
UCB_API ucb_norm_form ucb_uc_norm_form_from_str(const char* str);

UCB_API ucb_cp ucb_uc_iter_utf8(const unsigned char** iter);

UCB_API bool ucb_uc_encode_codepoints(ucb_buffer* buf, const ucb_cp* codepoints, size_t len, const struct ucb_error** perr);

/**
 * @brief Validate a string as UTF-8
 * @param str NULL-terminated string to validate
 * @param size if not UCB_NULL, will be set to the length of the string
 * @return UCB_OK if valid
 */
UCB_API bool ucb_uc_validate(const char* str, size_t* size);
UCB_API bool ucb_uc_validate_buf(const char* str, size_t size);

UCB_API size_t ucb_uc_num_cp(const char* str);
UCB_API size_t ucb_uc_num_chars(const char* str);

UCB_API ucb_uc_result ucb_uc_to_upper(const char* str, size_t size, const struct ucb_error** perr);
UCB_API ucb_uc_result ucb_uc_to_lower(const char* str, size_t size, const struct ucb_error** perr);
UCB_API ucb_uc_result ucb_uc_to_title(const char* str, size_t size, const struct ucb_error** perr);
UCB_API ucb_uc_result ucb_uc_casefold(const char* str, size_t size, const struct ucb_error** perr);

/**
 * Normalize an utf-8 string
 * If the string is ascii, it will be copied as-is
 * @param str string to normalize, must be a valid utf-8 string
 * @param form the normalization form to use
 * @returns result, with the normalized string on success
 */
UCB_API ucb_uc_result ucb_uc_normalize(const char* str, size_t size, ucb_norm_form form,
                                       const struct ucb_error** perr);

#endif // UCB_UNICODE_H
