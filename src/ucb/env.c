/**
 * @file env.c
 * This file is part of the UCB project
 * - SPDX-FileCopyrightText: © 2026 Åke Svedin <ake@svedin.org>
 * - SPDX-License-Identifier: MIT
 *
 * @brief Environment variables implementation
 */

#include "ucb/env.h"

#include "ucb/cstring.h"
#include "ucb/memory.h"

#ifdef _WIN32
#include <stdbool.h>

#include <Windows.h>
#else
#include <stdlib.h>
#endif

#ifdef _WIN32
// Static buffer for the last retrieved environment variable value.
// Sized to hold realistic values in UTF-8, the documented Windows maximum of 32,767 wide
// characters can expand to ~98 KiB in UTF-8 worst case, which will not fit and is reported
// as not found instead of being truncated.
#define UCB_ENV_BUFSIZE (64 * 1024)
static char s_envval[UCB_ENV_BUFSIZE];
#endif

bool ucb_env_has(const char* name)
{
#ifdef _WIN32
    wchar_t* name_w = ucb_cstr_to_wchar(name, 0, UCB_NULL, UCB_NULL);
    if (!name_w)
    {
        return false;
    }

    DWORD size = GetEnvironmentVariableW(name_w, UCB_NULL, 0);
    ucb_free(name_w);
    return size > 0;
#else
    return getenv(name) != NULL;
#endif
}

const char* ucb_env_get(const char* name)
{
#ifdef _WIN32
    wchar_t* name_w = ucb_cstr_to_wchar(name, 0, UCB_NULL, UCB_NULL);
    if (!name_w)
    {
        return UCB_NULL;
    }

    // Windows does not have a standard way to get the size of the environment variable value,
    // so we need to call GetEnvironmentVariable twice, first to get the size and then to get the
    // value.
    DWORD size = GetEnvironmentVariableW(name_w, UCB_NULL, 0);
    if (size == 0)
    {
        // Variable not found or error
        ucb_free(name_w);
        return UCB_NULL;
    }

    wchar_t* buffer = ucb_malloc_type(size, wchar_t);
    if (!buffer)
    {
        // Memory allocation failed
        ucb_free(name_w);
        return UCB_NULL;
    }

    GetEnvironmentVariableW(name_w, buffer, size);
    if (!ucb_cstr_from_wchar_buf(buffer, size - 1, s_envval, UCB_ENV_BUFSIZE, UCB_NULL, UCB_NULL))
    {
        // Conversion failed. Could be invalid unicode or buffer too small.
        ucb_free(buffer);
        ucb_free(name_w);
        return UCB_NULL;
    }

    ucb_free(buffer);
    ucb_free(name_w);
    return s_envval;
#else
    return getenv(name);
#endif
}

bool ucb_env_set(const char* name, const char* value, bool overwrite)
{
#ifdef _WIN32
    wchar_t* name_w = ucb_cstr_to_wchar(name, 0, UCB_NULL, UCB_NULL);
    if (!overwrite)
    {
        DWORD size = GetEnvironmentVariableW(name_w, UCB_NULL, 0);
        if (size > 0)
        {
            // Variable already exists and overwrite is false
            ucb_free(name_w);
            return true;
        }
    }
    wchar_t* value_w = ucb_cstr_to_wchar(value, 0, UCB_NULL, UCB_NULL);

    // Use _wputenv_s to set the environment variable.
    // This updates both the CRT and the Windows environment.
    bool ret = _wputenv_s(name_w, value_w) == 0;
    ucb_free(name_w);
    ucb_free(value_w);
    return ret;
#else
    return setenv(name, value, overwrite ? 1 : 0) == 0;
#endif
}

bool ucb_env_unset(const char* name)
{
#ifdef _WIN32
    wchar_t* name_w = ucb_cstr_to_wchar(name, 0, UCB_NULL, UCB_NULL);
    bool ret = _wputenv_s(name_w, L"") == 0;
    ucb_free(name_w);
    return ret;
#else
    return unsetenv(name) == 0;
#endif
}

bool ucb_env_append(const char* name, const char* value, const char* sep)
{
    const char* cur = ucb_env_get(name);
    if (!cur)
        return ucb_env_set(name, value, true);

    char* new = UCB_NULL;
    if (sep)
        new = ucb_cstr_concat(cur, sep, value, UCB_NULL);
    else
        new = ucb_cstr_concat(cur, value, UCB_NULL);

    bool res = ucb_env_set(name, new, true);
    ucb_free(new);
    return res;
}

bool ucb_env_prepend(const char* name, const char* value, const char* sep)
{
    const char* cur = ucb_env_get(name);
    if (!cur)
        return ucb_env_set(name, value, true);

    char* new = UCB_NULL;
    if (sep)
        new = ucb_cstr_concat(value, sep, cur, UCB_NULL);
    else
        new = ucb_cstr_concat(value, cur, UCB_NULL);

    bool res = ucb_env_set(name, new, true);
    ucb_free(new);
    return res;
}
