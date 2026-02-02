/**
 * @file cstring.c
 *
 * This file is part of the UCB project
 * - SPDX-FileCopyrightText: © 2026 Åke Svedin <ake@svedin.org>
 * - SPDX-License-Identifier: MIT
 *
 * @brief Cross-platform C string functions implementation
 */

#include "ucb/cstring.h"

#include "ucb/defines.h"
#include "ucb/error.h"
#include "ucb/memory.h"

#include <errno.h>
#include <stdio.h>
#include <string.h>

size_t ucb_cstr_len(const char* str)
{
    if (!str)
        return 0;
    return strlen(str);
}

size_t ucb_cstr_nlen(const char* str, size_t max_len)
{
    if (!str)
        return 0;
    return strnlen(str, max_len);
}

char* ucb_cstr_dup(const char* str)
{
    if (!str)
        return UCB_NULL;
    size_t len = ucb_cstr_len(str);
    char* dst  = ucb_malloc_type(len + 1, char);
    if (dst == UCB_NULL)
        return UCB_NULL;
    memcpy(dst, str, len);
    dst[len] = '\0';
    return dst;
}

char* ucb_cstr_ndup(const char* str, size_t max_len)
{
    if (!str)
        return UCB_NULL;
    size_t len = ucb_cstr_nlen(str, max_len);
    char* dst  = ucb_malloc_type(len + 1, char);
    if (dst == UCB_NULL)
        return UCB_NULL;
    memcpy(dst, str, len);
    dst[len] = '\0';
    return dst;
}

int ucb_cstr_comp(const char* a, const char* b)
{
    if (!a && !b)
        return 0;
    if (!a)
        return -1;
    if (!b)
        return 1;
    return strcmp(a, b);
}

int ucb_cstr_icomp(const char* a, const char* b)
{
    if (!a && !b)
        return 0;
    if (!a)
        return -1;
    if (!b)
        return 1;
#ifdef _WIN32
    return _stricmp(a, b);
#else
    return strcasecmp(a, b);
#endif // UCB_W
}

int ucb_cstr_sprintf(char* restrict buffer, size_t buffer_size, const char* restrict fmt, ...)
{
    if (!buffer)
        return -1;
    va_list ap;
    va_start(ap, fmt);
    int r = ucb_cstr_vsprintf(buffer, buffer_size, fmt, ap);
    va_end(ap);
    return r;
}

int ucb_cstr_vsprintf(char* restrict buffer, size_t buffer_size, const char* restrict fmt,
                      va_list vlist)
{
    if (!buffer)
        return -1;
#if defined(__STDC_LIB_EXT1__) || defined(_WIN32)
    return vsprintf_s(buffer, buffer_size, fmt, vlist);
#else
    UCB_UNUSED(buffer_size);
    return vsprintf(buffer, fmt, vlist);
#endif
}

int ucb_cstr_snprintf(char* restrict buffer, size_t count, const char* restrict fmt, ...)
{
    va_list ap;
    va_start(ap, fmt);
    int r = ucb_cstr_vsnprintf(buffer, count, fmt, ap);
    va_end(ap);
    return r;
}

int ucb_cstr_vsnprintf(char* buffer, size_t count, const char* fmt, va_list vlist)
{
    UCB_VERIFY_ARGS(fmt && (buffer ? count > 0 : count == 0));
    int ret = vsnprintf(buffer, count, fmt, vlist);
    if (ret < 0)
        UCB_REPORT_ERRNO(errno, "Failed call to vsnprintf");
    return ret;
}

int ucb_cstr_asprintf(char** restrict pstr, const char* restrict fmt, ...)
{
    va_list ap;
    va_start(ap, fmt);
    int r = ucb_cstr_vasprintf(pstr, fmt, ap);
    va_end(ap);
    return r;
}

int ucb_cstr_vasprintf(char** restrict pstr, const char* restrict fmt, va_list vlist)
{
    if (!pstr)
        return -1;
    if (!fmt)
    {
        *pstr = NULL;
        return 0;
    }
    va_list args_copy;
    va_copy(args_copy, vlist);
    int len = ucb_cstr_vsnprintf(NULL, 0, fmt, args_copy);
    va_end(args_copy);
    if (len < 0)
        return -1;
    char* str = ucb_malloc_type((size_t)len + 1, char);
    if (!str)
        return -1;
    len = ucb_cstr_vsnprintf(str, (size_t)len + 1, fmt, vlist);
    if (len < 0)
    {
        ucb_free(str);
        return -1;
    }
    *pstr = str;
    return len;
}
