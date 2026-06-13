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

#include <stdlib.h>

#ifdef _WIN32
#include <Windows.h>
#endif

bool ucb_env_has(const char* name)
{
#ifdef _WIN32
    wchar_t* name_w = ucb_str_to_wchar(name, UCB_NULL, UCB_NULL);
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
    // Windows does not have a standard way to get the size of the environment variable value,
    // so we need to call GetEnvironmentVariable twice, first to get the size and then to get the
    // value.
    DWORD size = GetEnvironmentVariableW((const wchar_t*)name->data, UCB_NULL, 0);
    if (size == 0)
    {
        // Variable not found or error
        return UCB_NULL;
    }

    wchar_t* buffer = ucb_malloc_type(size, wchar_t);
    if (!buffer)
    {
        // Memory allocation failed
        return UCB_NULL;
    }

    GetEnvironmentVariableW((const wchar_t*)name->data, buffer, size);
    ucb_str* result = ucb_str_from_wchar(buffer, size - 1, UCB_NULL);
    ucb_free(buffer);
    return result;
#else
    return getenv(name);
#endif
}

bool ucb_env_set(const char* name, const char* value, bool overwrite)
{
#ifdef _WIN32
    wchar_t* name_w = ucb_str_to_wchar(name, UCB_NULL, UCB_NULL);
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
    wchar_t* value_w = ucb_str_to_wchar(value, UCB_NULL, UCB_NULL);

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
    wchar_t* name_w = ucb_str_to_wchar(name, UCB_NULL, UCB_NULL);
    bool ret = _wputenv_s(name_w, UCB_NULL) == 0;
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

    char* new;
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

    char* new;
    if (sep)
        new = ucb_cstr_concat(value, sep, cur, UCB_NULL);
    else
        new = ucb_cstr_concat(value, cur, UCB_NULL);

    bool res = ucb_env_set(name, new, true);
    ucb_free(new);
    return res;
}
