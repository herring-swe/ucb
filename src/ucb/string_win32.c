/**
 * @file string_win32.c
 *
 * This file is part of the UCB project
 * - SPDX-FileCopyrightText: © 2026 Åke Svedin <ake@svedin.org>
 * - SPDX-License-Identifier: MIT
 *
 * @brief String type windows specific implementation
 */

#ifndef _WIN32
#error "This file is only for Windows"
#endif

#include "ucb/cstring.h"
#include "ucb/defines.h"
#include "ucb/error.h"
#include "ucb/memory.h"
#include "ucb/string.h"

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <stdlib.h>

#include <Windows.h>

ucb_str* ucb_str_from_wchar(const wchar_t* wstr, size_t wlen, ucb_error** perr)
{
    UCB_VERIFY_ARGS(wstr);

    // Check string length if needed, once
    // Include NULL character in query and the output will also include it
    if (wlen == 0)
    {
        // Special case, empty string
        if (wstr[0] == L'\0')
            return ucb_str_new_empty();
    }

    size_t clen = 0;
    char* cstr = ucb_cstr_from_wchar(wstr, wlen, &clen, perr);
    if (!cstr)
        return UCB_NULL;

    ucb_str* str = ucb_str_new_empty();
    ucb_str_adopt(str, cstr, clen, clen + 1);
    return str;
}

wchar_t* ucb_str_to_wchar(const ucb_str* str, size_t* wlen_out, ucb_error** perr)
{
    UCB_VERIFY_ARGS(str);

    return ucb_cstr_to_wchar(str->data, str->size, wlen_out, perr);
}
