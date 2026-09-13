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

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <limits.h>
#include <stdlib.h>

#include <Windows.h>

char* ucb_cstr_from_wchar(const wchar_t* wstr, size_t wlen, size_t* slen_out, ucb_error** perr)
{
    UCB_VERIFY_ARGS(wstr);

    if (slen_out)
        *slen_out = 0;

    // Check string length if needed, once
    // Include NULL character in query and the output will also include it
    int query_wlen = (int)wlen;
    if (wlen == 0)
    {
        // Special case, empty string
        if (wstr[0] == L'\0')
        {
            return ucb_calloc_type(1, char);
        }

        wlen = wcslen(wstr);
        query_wlen = (int)wlen + 1;
    }
    else if (wstr[wlen] != L'\0')
    {
        wlen = 0; // Indicate the need for extra NULL character
    }

    size_t str_size = 0;
    int ret =
        WideCharToMultiByte(CP_UTF8, WC_ERR_INVALID_CHARS, wstr, query_wlen, NULL, 0, NULL, NULL);
    if (ret == 0)
    {
        ucb_throw_win32(perr, GetLastError(), UCB_NULL);
        return UCB_NULL;
    }
    str_size = (size_t)ret;
    if (wlen == 0)
        str_size += 1;
    char* buffer = ucb_calloc_type(str_size + 1, char);
    if (!buffer)
        return UCB_NULL;

    ret = WideCharToMultiByte(CP_UTF8,
                              WC_ERR_INVALID_CHARS,
                              wstr,
                              query_wlen,
                              buffer,
                              (int)str_size,
                              NULL,
                              NULL);
    if (ret == 0)
    {
        ucb_throw_win32(perr, GetLastError(), UCB_NULL);
        ucb_free(buffer);
        return UCB_NULL;
    }

    if (slen_out)
        *slen_out = str_size - 1; // Exclude NULL character
    return buffer;
}

wchar_t* ucb_cstr_to_wchar(const char* str, size_t slen, size_t* wlen_out, ucb_error** perr)
{
    UCB_VERIFY_ARGS(str);

    if (wlen_out)
        *wlen_out = 0;

    if (!slen)
    {
        slen = strlen(str);
    }
    if (!slen)
    {
        // Empty string
        return ucb_calloc_type(1, wchar_t);
    }

    // We already know the string length, include NULL character
    // Output wstr will be NULL terminated
    int ret = MultiByteToWideChar(CP_UTF8, 0, str, (int)slen + 1, NULL, 0);
    if (ret == 0)
    {
        ucb_throw_win32(perr, GetLastError(), UCB_NULL);
        return UCB_NULL;
    }
    size_t wstr_size = (size_t)ret;
    wchar_t* wstr = ucb_malloc_type(wstr_size, wchar_t);
    if (!wstr)
        return UCB_NULL;

    ret = MultiByteToWideChar(CP_UTF8, 0, str, (int)slen + 1, wstr, (int)wstr_size);
    if (ret == 0)
    {
        ucb_throw_win32(perr, GetLastError(), UCB_NULL);
        ucb_free(wstr);
        return UCB_NULL;
    }

    if (wlen_out)
        *wlen_out = wstr_size - 1;
    return wstr;
}

size_t ucb_cstr_from_wchar_buf(const wchar_t* wstr,
                               size_t wlen,
                               char* buffer,
                               size_t buffer_size,
                               size_t* slen_out,
                               ucb_error** perr)
{
    UCB_VERIFY_ARGS(wstr);
    UCB_VERIFY_ARGS(buffer);

    if (slen_out)
        *slen_out = 0;

    // Check string length if needed, once
    if (wlen == 0)
    {
        // Special case, empty string
        if (wstr[0] == L'\0')
        {
            if (buffer_size < 1)
            {
                ucb_throw(perr, UCB_ERROR_BUFFER, "Buffer too small for UTF-8 conversion");
                return 0;
            }
            buffer[0] = '\0';
            return 1;
        }
        wlen = wcslen(wstr);
    }

    if (wlen > (size_t)INT_MAX)
    {
        ucb_throw(perr, UCB_ERROR_INVALID_ARG, "String length too large");
        return 0;
    }

    int ret =
        WideCharToMultiByte(CP_UTF8, WC_ERR_INVALID_CHARS, wstr, (int)wlen, NULL, 0, NULL, NULL);
    if (ret == 0)
    {
        ucb_throw_win32(perr, GetLastError(), UCB_NULL);
        return 0;
    }
    size_t str_size = (size_t)ret + 1; // Include NULL character
    if (str_size > buffer_size)
    {
        ucb_throw(perr, UCB_ERROR_BUFFER, "Buffer too small for UTF-8 conversion");
        return 0;
    }

    ret = WideCharToMultiByte(CP_UTF8,
                              WC_ERR_INVALID_CHARS,
                              wstr,
                              (int)wlen,
                              buffer,
                              (int)(str_size - 1),
                              NULL,
                              NULL);
    if (ret == 0)
    {
        ucb_throw_win32(perr, GetLastError(), UCB_NULL);
        return 0;
    }
    buffer[str_size - 1] = '\0';

    if (slen_out)
        *slen_out = str_size - 1; // Exclude NULL character
    return str_size;
}

size_t ucb_cstr_to_wchar_buf(const char* str,
                             size_t slen,
                             wchar_t* buffer,
                             size_t buffer_size,
                             size_t* wlen_out,
                             ucb_error** perr)
{
    UCB_VERIFY_ARGS(str);
    UCB_VERIFY_ARGS(buffer);

    if (wlen_out)
        *wlen_out = 0;

    // Check string length if needed, once
    if (slen == 0)
    {
        // Special case, empty string
        if (str[0] == '\0')
        {
            if (buffer_size < 1)
            {
                ucb_throw(perr, UCB_ERROR_BUFFER, "Buffer too small for UTF-16 conversion");
                return 0;
            }
            buffer[0] = L'\0';
            return 1;
        }
        slen = strlen(str);
    }

    if (slen > (size_t)INT_MAX)
    {
        ucb_throw(perr, UCB_ERROR_INVALID_ARG, "String length too large");
        return 0;
    }

    int ret = MultiByteToWideChar(CP_UTF8, 0, str, (int)slen, NULL, 0);
    if (ret == 0)
    {
        ucb_throw_win32(perr, GetLastError(), UCB_NULL);
        return 0;
    }
    size_t wstr_size = (size_t)ret + 1; // Include NULL character
    if (wstr_size > buffer_size)
    {
        ucb_throw(perr, UCB_ERROR_BUFFER, "Buffer too small for UTF-16 conversion");
        return 0;
    }

    ret = MultiByteToWideChar(CP_UTF8, 0, str, (int)slen, buffer, (int)(wstr_size - 1));
    if (ret == 0)
    {
        ucb_throw_win32(perr, GetLastError(), UCB_NULL);
        return 0;
    }
    buffer[wstr_size - 1] = L'\0';

    if (wlen_out)
        *wlen_out = wstr_size - 1; // Exclude NULL character
    return wstr_size;
}
