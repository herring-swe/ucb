/**
 * This file is part of the UCB project
 * SPDX-FileCopyrightText: © 2025 Åke Svedin <ake@svedin.org>
 * SPDX-License-Identifier: MIT
 * 
 * @brief Contain string implementation and related functions
 */

#ifndef UCB_STRING_H
#define UCB_STRING_H

#include "export.h"
#include "types.h"

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

struct ucb_error;

/**
 * @brief The default string flag, guaranteed to be 0.
 */
static const uint32_t UCB_STR_DEFAULT = 0x00;

/**
 * @brief Shallow copy flag
 * When used, any new string will not own the underlying data.
 * The new strings still needs to be free'd with ucb_str_free.
 */
static const uint32_t UCB_STR_SHALLOW = 0x01;

/**
 * @brief No verification flag
 * When used, the string will not be verified to be valid UTF-8.
 */
static const uint32_t UCB_STR_NO_VERIFY = 0x02;

/**
 * Represents a UTF-8 string type.
 */
struct ucb_str;
typedef struct ucb_str ucb_str_t;

typedef struct
{
    ucb_ecode error;
    ucb_str_t* str;
} ucb_str_result_t;

UCB_API ucb_str_t* ucb_str_new(const char* cstr, size_t len, uint32_t flags);
UCB_API void ucb_str_free(ucb_str_t* str);
UCB_API bool ucb_str_is_shallow(const ucb_str_t* str);

/**
 * Get the underlying NULL-terminated C string.
 * Do not manipulate this string.
 * @param str ucb string
 * @return read-only pointer to the underlying C string or UCB_NULL
 */
UCB_API const char* ucb_str_cstr(const ucb_str_t* str);

UCB_API size_t ucb_str_size(const ucb_str_t* str);
UCB_API size_t ucb_str_len(const ucb_str_t* str);
UCB_API size_t ucb_str_num_chars(const ucb_str_t* str);

UCB_API ucb_str_t* ucb_str_copy(const ucb_str_t* str);
UCB_API ucb_str_t* ucb_str_concat(const ucb_str_t* str1, const ucb_str_t* str2);

/**
 * @brief Create a substring from a string
 * @param str string to copy from
 * @param index start index of character byte
 * @param count number of character bytes to copy
 * @param flags could be UCB_STR_DEFAULT (0) or UCB_STR_SHALLOW. If set to UCB_STR_SHALLOW the
 *              returned string will be a shallow string.
 * @return ucb_str_t*
 */
UCB_API ucb_str_t* ucb_str_substr(const ucb_str_t* str, size_t index, size_t count, int flags);

#ifdef _WIN32
/**
 * Creates a utf8 string from a Windows wide string
 * @param wstr UTF-16 string
 * @param wlen length of the UTF-16 string, can be 0 if the string is null-terminated and length
 *             will be determined.
 * @return ucb_str_t* utf8 string or NULL on error
 */
UCB_API ucb_str_t* ucb_str_from_wchar(const wchar_t* wstr, size_t wlen, struct ucb_error *err);

/**
 * Creates a Windows wide string from a utf8 string
 * @param str utf8 string
 * @param wlen_out optional pointer, set to length of the returned string
 * @return wchar_t* UTF-16 string or UCB_NULL on error
 */
UCB_API wchar_t* ucb_str_to_wchar(const ucb_str_t* str, size_t* wlen_out, struct ucb_error *err);
#endif

#endif // UCB_STRING_H
