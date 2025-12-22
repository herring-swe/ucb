/**
 * This file is part of the UCB project
 * SPDX-FileCopyrightText: 2025 Åke Svedin <ake@svedin.org>
 * SPDX-License-Identifier: MIT
 *
 * @brief Windows specific UTF-8 implementation and helpers
 */

#ifndef _WIN32
#error "This file is only for Windows"
#endif

#include "ucb/defines.h"
#include "ucb/errcodes.h"
#include "ucb/error_private.h"
#include "ucb/memory.h"
#include "ucb/string_private.h"

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <Windows.h>

#include <stdlib.h>

ucb_str_t* ucb_str_from_wchar(const wchar_t* wstr, size_t wlen)
{
    if (!wstr)
    {
        ucb_set_last_error(UCB_ERROR_INVALID_ARG);
        return UCB_NULL;
    }
    // Check string length if needed, once
    // Include NULL character in query and the output will also include it
    int query_wlen = (int)wlen;
    if (wlen == 0)
    {
        // Special case, empty string
        if (wstr[0] == L'\0')
            return ucb_str_new("", 0, UCB_STR_DEFAULT);

        wlen       = wcslen(wstr);
        query_wlen = (int)wlen + 1;
    }
    else if (wstr[wlen] != L'\0')
    {
        wlen = 0; // Indicate the need for extra NULL character
    }

    size_t str_size = 0;
    int ret =
        WideCharToMultiByte(CP_UTF8, WC_ERR_INVALID_CHARS, wstr, query_wlen, NULL, 0, NULL, NULL);
    if (ret <= 0)
    {
        ucb_set_last_win32_error();
        return UCB_NULL;
    }
    str_size = (size_t)ret;
    if (wlen == 0)
        str_size += 1;
    char* buffer = ucb_calloc_type(1, char);
    if (!buffer)
        return UCB_NULL;

    ret = WideCharToMultiByte(CP_UTF8, WC_ERR_INVALID_CHARS, wstr, query_wlen, buffer,
                              (int)str_size, NULL, NULL);
    if (ret == 0)
    {
        ucb_set_last_win32_error();
        ucb_free(buffer);
        return UCB_NULL;
    }

    return ucb_str_new(buffer, str_size - 1, UCB_STR_NO_VERIFY);
}

wchar_t* ucb_str_to_wchar(const ucb_str_t* str, size_t* wlen_out)
{
    if (wlen_out)
        *wlen_out = 0;
    if (!str)
    {
        ucb_set_last_error(UCB_ERROR_INVALID_ARG);
        return UCB_NULL;
    }
    if (str->size == 0)
    {
        // Empty string
        return ucb_calloc_type(1, wchar_t);
    }

    // We already know the string length, include NULL character
    // Output wstr will be NULL terminated
    int ret = MultiByteToWideChar(CP_UTF8, 0, str->data, (int)str->size + 1, NULL, 0);
    if (ret == 0)
    {
        ucb_set_last_win32_error();
        return UCB_NULL;
    }
    size_t wstr_size = (size_t)ret;
    wchar_t* wstr    = ucb_malloc_type(wstr_size, wchar_t);
    if (!wstr)
        return UCB_NULL;

    ret = MultiByteToWideChar(CP_UTF8, 0, str->data, (int)str->size + 1, wstr, (int)wstr_size);
    if (ret == 0)
    {
        ucb_set_last_win32_error();
        ucb_free(wstr);
        return UCB_NULL;
    }

    if (wlen_out)
        *wlen_out = wstr_size - 1;
    return wstr;
}
