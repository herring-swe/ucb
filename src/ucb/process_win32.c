/**
 * @file process_win32.c
 *
 * This file is part of the UCB project
 * - SPDX-FileCopyrightText: © 2026 Åke Svedin <ake@svedin.org>
 * - SPDX-License-Identifier: MIT
 *
 * @brief Windows implementation of the process launching backend
 *
 * The command line follows the MSVCRT quoting rules, the environment block is
 * built as UTF-16, and handle inheritance defaults to a precise
 * @c PROC_THREAD_ATTRIBUTE_HANDLE_LIST of the three standard handles.
 */

#ifndef _WIN32
#error "This file is only for Windows"
#endif

#include "envmap_private.h"
#include "process_private.h"

#include <ucb/cstring.h>
#include <ucb/errcodes.h>
#include <ucb/error.h>
#include <ucb/memory.h>
#include <ucb/string.h>

#include <string.h>
#include <wchar.h>

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <Windows.h>

/* -------------------------------------------------------------------------- */
/*                                  Helpers                                   */
/* -------------------------------------------------------------------------- */

static bool process_win32_valid_handle(HANDLE handle)
{
    return handle != UCB_NULL && handle != INVALID_HANDLE_VALUE;
}

static void process_win32_append_char(ucb_str* str, char c)
{
    ucb_str_append_cstr(str, &c, 1);
}

static bool process_win32_arg_needs_quotes(const char* arg)
{
    if (arg[0] == '\0')
        return true;

    for (const char* p = arg; *p; ++p)
    {
        if (*p == ' ' || *p == '\t' || *p == '"')
            return true;
    }
    return false;
}

/**
 * @brief Build a UTF-16 command line from argv using the MSVCRT quoting rules
 *
 * An argument is quoted when it is empty or contains a space, tab or quote. A
 * run of backslashes immediately before a quote is doubled, and a trailing run
 * of backslashes is doubled before the closing quote.
 */
static wchar_t* process_win32_build_command_line(const char* const* argv, ucb_error** perr)
{
    ucb_str* cmd = ucb_str_new_empty();
    if (!cmd)
    {
        ucb_throw(perr, UCB_ERROR_OUT_OF_MEMORY, "ucb_process_spawn: allocation failed");
        return UCB_NULL;
    }

    for (size_t i = 0; argv[i]; ++i)
    {
        if (i)
            process_win32_append_char(cmd, ' ');

        const char* arg = argv[i];
        bool quote = process_win32_arg_needs_quotes(arg);
        if (quote)
            process_win32_append_char(cmd, '"');

        size_t backslashes = 0;
        for (const char* p = arg; *p; ++p)
        {
            if (*p == '\\')
            {
                ++backslashes;
                continue;
            }

            if (*p == '"')
            {
                for (size_t n = 0; n < backslashes * 2 + 1; ++n)
                    process_win32_append_char(cmd, '\\');
                process_win32_append_char(cmd, '"');
                backslashes = 0;
                continue;
            }

            for (size_t n = 0; n < backslashes; ++n)
                process_win32_append_char(cmd, '\\');
            backslashes = 0;
            process_win32_append_char(cmd, *p);
        }

        for (size_t n = 0; n < (quote ? backslashes * 2 : backslashes); ++n)
            process_win32_append_char(cmd, '\\');
        if (quote)
            process_win32_append_char(cmd, '"');
    }

    wchar_t* wide = ucb_cstr_to_wchar(ucb_str_cstr(cmd), 0, UCB_NULL, perr);
    ucb_str_free(cmd);
    return wide;
}

/**
 * @brief Build a double-NULL-terminated UTF-16 environment block
 */
static wchar_t* process_win32_build_env_block(const ucb_envmap* env, ucb_error** perr)
{
    char** envp = ucb_envmap_to_envp(env, perr);
    if (!envp)
        return UCB_NULL;

    size_t count = 0;
    while (envp[count])
        ++count;

    wchar_t** wide = (wchar_t**)ucb_malloc((count ? count : 1) * sizeof(wchar_t*));
    if (!wide)
    {
        ucb_envmap_envp_free(envp);
        ucb_throw(perr, UCB_ERROR_OUT_OF_MEMORY, "ucb_process_spawn: allocation failed");
        return UCB_NULL;
    }

    size_t total = 1; /* trailing extra NULL */
    size_t converted = 0;
    for (size_t i = 0; i < count; ++i)
    {
        wide[i] = ucb_cstr_to_wchar(envp[i], 0, UCB_NULL, perr);
        if (!wide[i])
        {
            for (size_t j = 0; j < converted; ++j)
                ucb_free(wide[j]);
            ucb_free(wide);
            ucb_envmap_envp_free(envp);
            return UCB_NULL;
        }
        total += wcslen(wide[i]) + 1;
        ++converted;
    }
    if (total < 2)
        total = 2; /* an empty environment is still a double-NUL block */

    wchar_t* block = (wchar_t*)ucb_malloc(total * sizeof(wchar_t));
    if (!block)
    {
        for (size_t j = 0; j < converted; ++j)
            ucb_free(wide[j]);
        ucb_free(wide);
        ucb_envmap_envp_free(envp);
        ucb_throw(perr, UCB_ERROR_OUT_OF_MEMORY, "ucb_process_spawn: allocation failed");
        return UCB_NULL;
    }

    wchar_t* cursor = block;
    for (size_t i = 0; i < count; ++i)
    {
        size_t len = wcslen(wide[i]);
        memcpy(cursor, wide[i], len * sizeof(wchar_t));
        cursor[len] = L'\0';
        cursor += len + 1;
        ucb_free(wide[i]);
    }
    *cursor = L'\0';
    if (cursor == block)
        cursor[1] = L'\0'; /* keep the empty environment double-NUL */

    ucb_free(wide);
    ucb_envmap_envp_free(envp);
    return block;
}

static HANDLE process_win32_std_handle(ucb_file* file, DWORD std_id)
{
    if (file)
        return (HANDLE)ucb_file_get_handle(file);
    return GetStdHandle(std_id);
}

/* -------------------------------------------------------------------------- */
/*                                  Backend                                   */
/* -------------------------------------------------------------------------- */

ucb_process* ucb_process_plat_spawn(const ucb_process_opts* opts, ucb_error** perr)
{
    wchar_t* cmd = UCB_NULL;
    wchar_t* env_block = UCB_NULL;
    wchar_t* cwd = UCB_NULL;
    PROCESS_INFORMATION pi;
    STARTUPINFOEXW si_ex;
    STARTUPINFOW si_plain;
    LPSTARTUPINFOW si_ptr = UCB_NULL;
    LPPROC_THREAD_ATTRIBUTE_LIST attrs = UCB_NULL;
    bool attrs_initialized = false;
    HANDLE std_handles[3];
    DWORD saved_flags[3] = {0, 0, 0};
    bool usable[3] = {false, false, false};
    HANDLE inherit_list[3];
    UINT inherit_count = 0;
    BOOL inherit_handles = FALSE;
    DWORD creation;
    bool inherit_all;
    bool created = false;

    memset(&pi, 0, sizeof(pi));
    memset(&si_ex, 0, sizeof(si_ex));
    memset(&si_plain, 0, sizeof(si_plain));

    cmd = process_win32_build_command_line(opts->argv, perr);
    if (!cmd)
        return UCB_NULL;

    if (opts->env)
    {
        env_block = process_win32_build_env_block(opts->env, perr);
        if (!env_block)
        {
            ucb_free(cmd);
            return UCB_NULL;
        }
    }

    if (opts->cwd)
    {
        cwd = ucb_cstr_to_wchar(opts->cwd, 0, UCB_NULL, perr);
        if (!cwd)
        {
            ucb_free(cmd);
            ucb_free(env_block);
            return UCB_NULL;
        }
    }

    std_handles[0] = process_win32_std_handle(opts->in, STD_INPUT_HANDLE);
    std_handles[1] = process_win32_std_handle(opts->out, STD_OUTPUT_HANDLE);
    std_handles[2] = process_win32_std_handle(opts->err, STD_ERROR_HANDLE);

    inherit_all = (opts->flags & UCB_PROCESS_INHERIT_HANDLES) != 0;

    for (int i = 0; i < 3; ++i)
    {
        if (!process_win32_valid_handle(std_handles[i]))
            continue;
        if (!GetHandleInformation(std_handles[i], &saved_flags[i]))
            continue;

        if (inherit_all)
            continue;

        if (!(saved_flags[i] & HANDLE_FLAG_INHERIT))
        {
            if (!SetHandleInformation(std_handles[i], HANDLE_FLAG_INHERIT, HANDLE_FLAG_INHERIT))
                continue;
        }
        usable[i] = true;

        bool duplicate = false;
        for (UINT k = 0; k < inherit_count; ++k)
        {
            if (inherit_list[k] == std_handles[i])
            {
                duplicate = true;
                break;
            }
        }
        if (!duplicate)
            inherit_list[inherit_count++] = std_handles[i];
    }

    if (!inherit_all && inherit_count > 0)
    {
        SIZE_T attr_size = 0;
        InitializeProcThreadAttributeList(UCB_NULL, 1, 0, &attr_size);

        attrs = (LPPROC_THREAD_ATTRIBUTE_LIST)ucb_malloc(attr_size);
        if (!attrs)
        {
            ucb_throw(perr, UCB_ERROR_OUT_OF_MEMORY, "ucb_process_spawn: allocation failed");
            goto cleanup;
        }

        if (!InitializeProcThreadAttributeList(attrs, 1, 0, &attr_size))
        {
            ucb_throw_win32(perr, GetLastError(), "ucb_process_spawn: handle list setup failed");
            goto cleanup;
        }
        attrs_initialized = true;

        if (!UpdateProcThreadAttribute(attrs,
                                       0,
                                       PROC_THREAD_ATTRIBUTE_HANDLE_LIST,
                                       inherit_list,
                                       (SIZE_T)(inherit_count * sizeof(HANDLE)),
                                       UCB_NULL,
                                       UCB_NULL))
        {
            ucb_throw_win32(perr, GetLastError(), "ucb_process_spawn: handle list setup failed");
            goto cleanup;
        }

        si_ex.StartupInfo.cb = sizeof(si_ex);
        si_ex.lpAttributeList = attrs;
        si_ptr = &si_ex.StartupInfo;
        inherit_handles = TRUE;
    }
    else
    {
        si_plain.cb = sizeof(si_plain);
        si_ptr = &si_plain;
        inherit_handles = inherit_all ? TRUE : FALSE;
    }

    /* Assign the resolved handles as the child's standard streams. This is what
       actually performs the redirection; the handle list only controls which
       handles may cross the process boundary. */
    si_ptr->dwFlags |= STARTF_USESTDHANDLES;
    si_ptr->hStdInput = std_handles[0];
    si_ptr->hStdOutput = std_handles[1];
    si_ptr->hStdError = std_handles[2];

    creation = CREATE_UNICODE_ENVIRONMENT;
    if (attrs)
        creation |= EXTENDED_STARTUPINFO_PRESENT;
    if (opts->flags & UCB_PROCESS_DETACH)
        creation |= DETACHED_PROCESS | CREATE_NEW_PROCESS_GROUP;
    if (opts->flags & UCB_PROCESS_NO_WINDOW)
        creation |= CREATE_NO_WINDOW;

    if (!CreateProcessW(UCB_NULL,
                        cmd,
                        UCB_NULL,
                        UCB_NULL,
                        inherit_handles,
                        creation,
                        env_block,
                        cwd,
                        si_ptr,
                        &pi))
    {
        ucb_throw_win32(perr, GetLastError(), "ucb_process_spawn: CreateProcessW failed");
        goto cleanup;
    }
    created = true;

cleanup:
    for (int i = 0; i < 3; ++i)
    {
        if (usable[i] && !(saved_flags[i] & HANDLE_FLAG_INHERIT))
            (void)SetHandleInformation(std_handles[i], HANDLE_FLAG_INHERIT, 0);
    }

    if (attrs)
    {
        if (attrs_initialized)
            DeleteProcThreadAttributeList(attrs);
        ucb_free(attrs);
    }

    ucb_free(cmd);
    ucb_free(env_block);
    ucb_free(cwd);

    if (!created)
        return UCB_NULL;

    CloseHandle(pi.hThread);

    {
        ucb_process* proc = ucb_calloc_type(1, ucb_process);
        if (!proc)
        {
            TerminateProcess(pi.hProcess, 1);
            CloseHandle(pi.hProcess);
            ucb_throw(perr, UCB_ERROR_OUT_OF_MEMORY, "ucb_process_spawn: allocation failed");
            return UCB_NULL;
        }

        proc->pid = (ucb_pid)pi.dwProcessId;
        proc->reaped = false;
        proc->handle = pi.hProcess;
        return proc;
    }
}

bool ucb_process_plat_wait(ucb_process* proc, int timeout_ms, bool* out_exited, ucb_error** perr)
{
    *out_exited = false;

    HANDLE handle = (HANDLE)proc->handle;
    DWORD wait_ms = timeout_ms < 0 ? INFINITE : (DWORD)timeout_ms;
    DWORD result = WaitForSingleObject(handle, wait_ms);

    if (result == WAIT_OBJECT_0)
    {
        DWORD code = 0;
        if (!GetExitCodeProcess(handle, &code))
        {
            ucb_throw_win32(perr, GetLastError(), "ucb_process_wait: GetExitCodeProcess failed");
            return false;
        }

        proc->reaped = true;
        proc->exit_code = (int)code;
        CloseHandle(handle);
        proc->handle = UCB_NULL;
        *out_exited = true;
        return true;
    }

    if (result == WAIT_TIMEOUT)
        return true;

    ucb_throw_win32(perr, GetLastError(), "ucb_process_wait: WaitForSingleObject failed");
    return false;
}

bool ucb_process_plat_terminate(ucb_process* proc, bool force, ucb_error** perr)
{
    UCB_UNUSED(force);

    HANDLE handle = (HANDLE)proc->handle;
    if (!process_win32_valid_handle(handle))
        return true;

    DWORD code = 0;
    if (GetExitCodeProcess(handle, &code) && code != STILL_ACTIVE)
        return true;

    if (!TerminateProcess(handle, 1))
        return !ucb_throw_win32(perr,
                                GetLastError(),
                                "ucb_process_terminate: TerminateProcess failed");

    return true;
}

void ucb_process_plat_free(ucb_process* proc)
{
    if (!proc)
        return;

    if (process_win32_valid_handle((HANDLE)proc->handle))
    {
        CloseHandle((HANDLE)proc->handle);
        proc->handle = UCB_NULL;
    }
}
