/**
 * @file dir_win32.c
 *
 * This file is part of the UCB project
 * - SPDX-FileCopyrightText: © 2026 Åke Svedin <ake@svedin.org>
 * - SPDX-License-Identifier: MIT
 *
 * @brief Windows implementation of the directory iterator
 */

#ifndef _WIN32
#error "This file is only for Windows"
#endif

#include "dir_private.h"
#include "fs_private.h"

#include <ucb/cstring.h>
#include <ucb/errcodes.h>
#include <ucb/error.h>
#include <ucb/memory.h>

#include <string.h>

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <Windows.h>

typedef struct dir_win32_state
{
    HANDLE find;    ///< FindFirstFileW handle, or INVALID_HANDLE_VALUE
    bool active;    ///< True when @c find was opened successfully
    bool has_entry; ///< True when @c entry holds an unread entry
    int dot_index;  ///< 0 pending ".", 1 pending "..", 2 done
    WIN32_FIND_DATAW entry;
} dir_win32_state;

static ucb_fs_kind dir_win32_kind_from_attrs(DWORD attrs)
{
    if (attrs & FILE_ATTRIBUTE_REPARSE_POINT)
        return UCB_FS_KIND_SYMLINK;
    if (attrs & FILE_ATTRIBUTE_DIRECTORY)
        return UCB_FS_KIND_DIR;
    if (attrs & FILE_ATTRIBUTE_DEVICE)
        return UCB_FS_KIND_OTHER;
    return UCB_FS_KIND_FILE;
}

bool ucb_dir_plat_open(ucb_dir* dir, ucb_error** perr)
{
    dir_win32_state* state = ucb_calloc_type(1, dir_win32_state);
    if (!state)
        return false;

    state->find = INVALID_HANDLE_VALUE;
    state->dot_index = (dir->flags & UCB_DIR_INCLUDE_DOT) ? 0 : 2;

    char* pattern = ucb_fs_join(dir->path, "*");
    if (!pattern)
    {
        ucb_free(state);
        return false;
    }

    wchar_t* wpattern = ucb_cstr_to_wchar(pattern, 0, UCB_NULL, perr);
    ucb_free(pattern);
    if (!wpattern)
    {
        ucb_free(state);
        return false;
    }

    state->find = FindFirstFileW(wpattern, &state->entry);
    ucb_free(wpattern);

    if (state->find == INVALID_HANDLE_VALUE)
    {
        DWORD err = GetLastError();
        if (err == ERROR_FILE_NOT_FOUND)
        {
            // An empty directory has no matching entries.
            state->active = false;
            state->has_entry = false;
            dir->state = state;
            return true;
        }
        ucb_free(state);
        ucb_throw_win32(perr, err, "ucb_dir_open: FindFirstFile failed");
        return false;
    }

    state->active = true;
    state->has_entry = true;
    dir->state = state;
    return true;
}

bool ucb_dir_plat_next(ucb_dir* dir, ucb_dir_entry* out, ucb_error** perr)
{
    dir_win32_state* state = (dir_win32_state*)dir->state;

    for (;;)
    {
        if (state->dot_index < 2)
        {
            out->name = (state->dot_index == 0) ? "." : "..";
            out->kind = UCB_FS_KIND_DIR;
            state->dot_index++;
            return true;
        }

        if (!state->has_entry)
        {
            if (!state->active)
                return false;

            if (!FindNextFileW(state->find, &state->entry))
            {
                DWORD err = GetLastError();
                if (err == ERROR_NO_MORE_FILES)
                    return false;
                ucb_throw_win32(perr, err, "ucb_dir_next: FindNextFile failed");
                return false;
            }
            state->has_entry = true;
        }

        state->has_entry = false;

        if (wcscmp(state->entry.cFileName, L".") == 0 || wcscmp(state->entry.cFileName, L"..") == 0)
            continue;

        char* name = ucb_cstr_from_wchar(state->entry.cFileName, 0, UCB_NULL, perr);
        if (!name)
            return false;

        ucb_free(dir->name);
        dir->name = name;

        out->name = name;
        out->kind = dir_win32_kind_from_attrs(state->entry.dwFileAttributes);
        return true;
    }
}

bool ucb_dir_plat_close(ucb_dir* dir, ucb_error** perr)
{
    UCB_UNUSED(perr);

    dir_win32_state* state = (dir_win32_state*)dir->state;
    if (state)
    {
        dir->state = UCB_NULL;
        if (state->active)
            FindClose(state->find);
        ucb_free(state);
    }
    return true;
}
