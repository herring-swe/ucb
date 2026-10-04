/**
 * @file dir_unix.c
 *
 * This file is part of the UCB project
 * - SPDX-FileCopyrightText: © 2026 Åke Svedin <ake@svedin.org>
 * - SPDX-License-Identifier: MIT
 *
 * @brief POSIX implementation of the directory iterator
 */

#ifndef _WIN32
#define _FILE_OFFSET_BITS 64
#endif
#if !defined(_POSIX_C_SOURCE)
#define _POSIX_C_SOURCE 200809L
#endif

#include "dir_private.h"
#include "error_private.h"
#include "fs_private.h"

#include <ucb/errcodes.h>
#include <ucb/error.h>
#include <ucb/memory.h>

#include <dirent.h>
#include <errno.h>
#include <string.h>

static ucb_fs_kind dir_unix_kind_from_dtype(unsigned char dtype)
{
    switch (dtype)
    {
    case DT_REG:
        return UCB_FS_KIND_FILE;
    case DT_DIR:
        return UCB_FS_KIND_DIR;
    case DT_LNK:
        return UCB_FS_KIND_SYMLINK;
#ifdef DT_FIFO
    case DT_FIFO:
        return UCB_FS_KIND_OTHER;
#endif
#ifdef DT_SOCK
    case DT_SOCK:
        return UCB_FS_KIND_OTHER;
#endif
#ifdef DT_CHR
    case DT_CHR:
        return UCB_FS_KIND_OTHER;
#endif
#ifdef DT_BLK
    case DT_BLK:
        return UCB_FS_KIND_OTHER;
#endif
    default:
        return UCB_FS_KIND_UNKNOWN;
    }
}

bool ucb_dir_plat_open(ucb_dir* dir, ucb_error** perr)
{
    DIR* stream = opendir(dir->path);
    if (!stream)
    {
        ucb_throw_errno(perr, errno, "ucb_dir_open: opendir failed");
        return false;
    }

    dir->state = stream;
    return true;
}

bool ucb_dir_plat_next(ucb_dir* dir, ucb_dir_entry* out, ucb_error** perr)
{
    DIR* stream = (DIR*)dir->state;

    for (;;)
    {
        errno = 0;
        struct dirent* entry = readdir(stream);
        if (!entry)
        {
            if (errno != 0)
            {
                ucb_throw_errno(perr, errno, "ucb_dir_next: readdir failed");
                return false;
            }
            return false;
        }

        if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0)
        {
            if (!(dir->flags & UCB_DIR_INCLUDE_DOT))
                continue;
        }

        ucb_fs_kind kind = dir_unix_kind_from_dtype(entry->d_type);
        if (kind == UCB_FS_KIND_UNKNOWN)
        {
            char* full = ucb_fs_join(dir->path, entry->d_name);
            if (full)
            {
                ucb_error* lerr = UCB_NULL;
                if (!ucb_fs_plat_kind(full, UCB_FS_NOFOLLOW, &kind, &lerr))
                    kind = UCB_FS_KIND_UNKNOWN;
                ucb_error_clear(&lerr);
                ucb_free(full);
            }
        }

        out->name = entry->d_name;
        out->kind = kind;
        return true;
    }
}

bool ucb_dir_plat_close(ucb_dir* dir, ucb_error** perr)
{
    DIR* stream = (DIR*)dir->state;
    if (stream)
    {
        dir->state = UCB_NULL;
        if (closedir(stream) != 0)
            return !ucb_throw_errno(perr, errno, "ucb_dir_close: closedir failed");
    }
    return true;
}
