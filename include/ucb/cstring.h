/**
 * @file cstring.h
 * This file is part of the UCB project
 * - SPDX-FileCopyrightText: © 2026 Åke Svedin <ake@svedin.org>
 * - SPDX-License-Identifier: MIT
 *
 * @brief Cross-platform C string functions
 *
 * @remark Not aware of encodings. For unicode use @ref string.h or @ref unicode.h
 */

#ifndef UCB_CSTRING_H
#define UCB_CSTRING_H

#include <ucb/export.h>

#include <stdarg.h>
#include <stddef.h>

#ifdef _WIN32
#include <ucb/error.h>

#include <wchar.h>
#endif

UCB_API size_t ucb_cstr_len(const char* str);
UCB_API size_t ucb_cstr_nlen(const char* str, size_t max_len);

UCB_API char* ucb_cstr_dup(const char* str);
UCB_API char* ucb_cstr_ndup(const char* str, size_t max_len);

UCB_API int ucb_cstr_comp(const char* a, const char* b);
UCB_API int ucb_cstr_icomp(const char* a, const char* b);

UCB_API char* ucb_cstr_concat(const char* str, ...);
UCB_API char* ucb_cstr_concatv(const char* str, va_list args);

UCB_API int ucb_cstr_sprintf(char* UCB_RESTRICT buffer,
                             size_t buffer_size,
                             const char* UCB_RESTRICT fmt,
                             ...);
UCB_API int ucb_cstr_vsprintf(char* UCB_RESTRICT buffer,
                              size_t buffer_size,
                              const char* UCB_RESTRICT fmt,
                              va_list vlist);

UCB_API int ucb_cstr_snprintf(char* UCB_RESTRICT buffer,
                              size_t buffer_size,
                              const char* UCB_RESTRICT fmt,
                              ...);
UCB_API int ucb_cstr_vsnprintf(char* UCB_RESTRICT buffer,
                               size_t buffer_size,
                               const char* UCB_RESTRICT fmt,
                               va_list vlist);

UCB_API int ucb_cstr_asprintf(char** UCB_RESTRICT pstr, const char* UCB_RESTRICT fmt, ...);
UCB_API int ucb_cstr_vasprintf(char** UCB_RESTRICT pstr,
                               const char* UCB_RESTRICT fmt,
                               va_list args);

#ifdef _WIN32
/**
 * Creates an UTF-8 encoded C-string from a Windows wide string
 * @param wstr UTF-16 string
 * @param wlen length of the UTF-16 string, can be 0 if the string is null-terminated and length
 *             will be determined.
 * @param slen_out optional pointer, set to length of the returned string
 * @return char* UTF-8 string or UCB_NULL on error
 */
UCB_API char* ucb_cstr_from_wchar(const wchar_t* wstr,
                                  size_t wlen,
                                  size_t* slen_out,
                                  ucb_error** perr);

/**
 * Creates a Windows wide string from a UTF-8 encoded C-string
 * @param str UTF-8 C-string
 * @param slen length of the UTF-8 string, can be 0 if the string is null-terminated and length will
 *             be determined
 * @param wlen_out optional pointer, set to length of the returned string
 * @return wchar_t* UTF-16 string or UCB_NULL on error
 */
UCB_API wchar_t* ucb_cstr_to_wchar(const char* cstr,
                                   size_t slen,
                                   size_t* wlen_out,
                                   ucb_error** perr);

/**
 * Creates an UTF-8 encoded C-string from a Windows wide string, into a caller provided buffer.
 * The buffer is always NULL terminated, provided there is room for the data.
 * @param wstr UTF-16 string
 * @param wlen length of the UTF-16 string, excluding the terminating NULL character.
 *             0 means the length is determined from the NULL character.
 * @param buffer destination buffer, must be writable
 * @param buffer_size size of the destination buffer
 * @param slen_out optional pointer, set to length of the result, excluding the NULL character
 * @param perr optional error, set to UCB_ERROR_BUFFER if the result does not fit in the buffer
 * @return number of bytes written, including the terminating NULL character, or 0 on error.
 *         If the result does not fit in the buffer, nothing is written.
 */
UCB_API size_t ucb_cstr_from_wchar_buf(const wchar_t* wstr,
                                       size_t wlen,
                                       char* UCB_RESTRICT buffer,
                                       size_t buffer_size,
                                       size_t* slen_out,
                                       ucb_error** perr);

/**
 * Creates a Windows wide string from an UTF-8 encoded C-string, into a caller provided buffer.
 * The buffer is always NULL terminated, provided there is room for the data.
 * @param cstr UTF-8 C-string
 * @param slen length of the UTF-8 string, excluding the terminating NULL character.
 *             0 means the length is determined from the NULL character.
 * @param buffer destination buffer, must be writable
 * @param buffer_size size of the destination buffer, in number of wchar_t elements
 * @param wlen_out optional pointer, set to length of the result, excluding the NULL character
 * @param perr optional error, set to UCB_ERROR_BUFFER if the result does not fit in the buffer
 * @return number of wchar_t elements written, including the terminating NULL character, or 0 on
 *         error. If the result does not fit in the buffer, nothing is written.
 */
UCB_API size_t ucb_cstr_to_wchar_buf(const char* cstr,
                                     size_t slen,
                                     wchar_t* UCB_RESTRICT buffer,
                                     size_t buffer_size,
                                     size_t* wlen_out,
                                     ucb_error** perr);

#endif // _WIN32

#endif // UCB_CSTRING_H
