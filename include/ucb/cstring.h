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

#include "export.h"

#include <stdarg.h>
#include <stddef.h>

UCB_API size_t ucb_cstr_len(const char* str);
UCB_API size_t ucb_cstr_nlen(const char* str, size_t max_len);

UCB_API char* ucb_cstr_dup(const char* str);
UCB_API char* ucb_cstr_ndup(const char* str, size_t max_len);

UCB_API int ucb_cstr_comp(const char* a, const char* b);
UCB_API int ucb_cstr_icomp(const char* a, const char* b);

UCB_API int ucb_cstr_sprintf(char* UCB_RESTRICT buffer, size_t buffer_size,
                             const char* UCB_RESTRICT fmt, ...);
UCB_API int ucb_cstr_vsprintf(char* UCB_RESTRICT buffer, size_t buffer_size,
                              const char* UCB_RESTRICT fmt, va_list vlist);

UCB_API int ucb_cstr_snprintf(char* UCB_RESTRICT buffer, size_t buffer_size,
                              const char* UCB_RESTRICT fmt, ...);
UCB_API int ucb_cstr_vsnprintf(char* UCB_RESTRICT buffer, size_t buffer_size,
                               const char* UCB_RESTRICT fmt, va_list vlist);

UCB_API int ucb_cstr_asprintf(char** UCB_RESTRICT pstr, const char* UCB_RESTRICT fmt, ...);
UCB_API int ucb_cstr_vasprintf(char** UCB_RESTRICT pstr, const char* UCB_RESTRICT fmt,
                               va_list args);

#endif // UCB_CSTRING_H
