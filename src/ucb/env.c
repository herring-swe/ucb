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
#include "ucb/string.h"

#include <string.h>

#ifdef _WIN32
#include <stdbool.h>

#include <Windows.h>
#else
#include <stdlib.h>
#endif

bool ucb_env_has(const ucb_str* name)
{
#ifdef _WIN32
    wchar_t* name_w = ucb_cstr_to_wchar(ucb_str_cstr(name), 0, UCB_NULL, UCB_NULL);
    if (!name_w)
    {
        return false;
    }

    DWORD size = GetEnvironmentVariableW(name_w, UCB_NULL, 0);
    ucb_free(name_w);
    return size > 0;
#else
    return getenv(ucb_str_cstr(name)) != NULL;
#endif
}

ucb_str* ucb_env_get(const ucb_str* name)
{
#ifdef _WIN32
    wchar_t* name_w = ucb_cstr_to_wchar(ucb_str_cstr(name), 0, UCB_NULL, UCB_NULL);
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
    ucb_free(name_w);

    ucb_str* value = ucb_str_from_wchar(buffer, 0, UCB_NULL);
    ucb_free(buffer);
    return value;
#else
    const char* cur = getenv(ucb_str_cstr(name));
    if (!cur)
        return UCB_NULL;

    // TODO(utf8): POSIX environment values are assumed UTF-8. A non-UTF-8 value
    // aborts in ucb_str_new_c; detection and conversion are deferred to the
    // Encoding API (see TODO.md).
    return ucb_str_new_c(cur);
#endif
}

bool ucb_env_set(const ucb_str* name, const ucb_str* value, bool overwrite)
{
#ifdef _WIN32
    wchar_t* name_w = ucb_cstr_to_wchar(ucb_str_cstr(name), 0, UCB_NULL, UCB_NULL);
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
    wchar_t* value_w = ucb_cstr_to_wchar(ucb_str_cstr(value), 0, UCB_NULL, UCB_NULL);

    // Use _wputenv_s to set the environment variable.
    // This updates both the CRT and the Windows environment.
    bool ret = _wputenv_s(name_w, value_w) == 0;
    ucb_free(name_w);
    ucb_free(value_w);
    return ret;
#else
    return setenv(ucb_str_cstr(name), ucb_str_cstr(value), overwrite ? 1 : 0) == 0;
#endif
}

bool ucb_env_unset(const ucb_str* name)
{
#ifdef _WIN32
    wchar_t* name_w = ucb_cstr_to_wchar(ucb_str_cstr(name), 0, UCB_NULL, UCB_NULL);
    bool ret = _wputenv_s(name_w, L"") == 0;
    ucb_free(name_w);
    return ret;
#else
    return unsetenv(ucb_str_cstr(name)) == 0;
#endif
}

bool ucb_env_append(const ucb_str* name, const ucb_str* value, const ucb_str* sep)
{
    ucb_str* cur = ucb_env_get(name);
    if (!cur)
        return ucb_env_set(name, value, true);

    ucb_str updated = ucb_str_make();
    bool ok = ucb_str_init(&updated, ucb_str_cstr(cur), ucb_str_len(cur));
    if (ok)
    {
        if (sep)
            ucb_str_append(&updated, sep);
        ucb_str_append(&updated, value);
        ok = ucb_env_set(name, &updated, true);
    }

    ucb_str_release(&updated);
    ucb_str_free(cur);
    return ok;
}

bool ucb_env_prepend(const ucb_str* name, const ucb_str* value, const ucb_str* sep)
{
    ucb_str* cur = ucb_env_get(name);
    if (!cur)
        return ucb_env_set(name, value, true);

    ucb_str updated = ucb_str_make();
    bool ok = ucb_str_init(&updated, ucb_str_cstr(value), ucb_str_len(value));
    if (ok)
    {
        if (sep)
            ucb_str_append(&updated, sep);
        ucb_str_append(&updated, cur);
        ok = ucb_env_set(name, &updated, true);
    }

    ucb_str_release(&updated);
    ucb_str_free(cur);
    return ok;
}
