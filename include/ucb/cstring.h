/**
 * This file is part of the UCB project
 * SPDX-FileCopyrightText: © 2025 Åke Svedin <ake@svedin.org>
 * SPDX-License-Identifier: MIT
 *
 * @brief Cross-platform C string functions
 * Not aware of encodings. For unicode use string.h or unicode.h
 */

#include "export.h"

#include <stdarg.h>
#include <stddef.h>

UCB_API size_t ucb_cstr_len(const char* str);
UCB_API size_t ucb_cstr_nlen(const char* str, size_t max_len);

UCB_API char* ucb_cstr_dup(const char* str);
UCB_API char* ucb_cstr_ndup(const char* str, size_t max_len);

UCB_API int ucb_cstr_comp(const char* a, const char* b);
UCB_API int ucb_cstr_icomp(const char* a, const char* b);

UCB_API int ucb_cstr_vsnprintf(char* buffer, size_t buffer_size, size_t count, const char* fmt,
                               va_list argptr);
UCB_API int ucb_cstr_asprintf(char** restrict pstr, const char* restrict fmt, ...);
UCB_API int ucb_cstr_vasprintf(char** restrict pstr, const char* restrict fmt, va_list args);
