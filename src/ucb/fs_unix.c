/**
 * @file fs_unix.c
 *
 * This file is part of the UCB project
 * - SPDX-FileCopyrightText: © 2026 Åke Svedin <ake@svedin.org>
 * - SPDX-License-Identifier: MIT
 *
 * @brief POSIX implementation of the filesystem API
 */

#if !defined(_WIN32)
#define _FILE_OFFSET_BITS 64
#endif
#if !defined(_POSIX_C_SOURCE)
#define _POSIX_C_SOURCE 200809L
#endif

#include "fs_private.h"

#include <ucb/errcodes.h>
#include <ucb/error.h>
#include <ucb/memory.h>

#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <time.h>
#include <unistd.h>

static ucb_fs_kind fs_unix_kind_from_mode(mode_t mode)
{
    if (S_ISREG(mode))
        return UCB_FS_KIND_FILE;
    if (S_ISDIR(mode))
        return UCB_FS_KIND_DIR;
    if (S_ISLNK(mode))
        return UCB_FS_KIND_SYMLINK;
    return UCB_FS_KIND_OTHER;
}

static int fs_unix_stat(const char* path, unsigned flags, struct stat* st)
{
    if (flags & UCB_FS_NOFOLLOW)
        return lstat(path, st);
    return stat(path, st);
}

bool ucb_fs_plat_kind(const char* path, unsigned flags, ucb_fs_kind* out, ucb_error** perr)
{
    struct stat st;
    if (fs_unix_stat(path, flags, &st) != 0)
    {
        ucb_throw_errno(perr, errno, "filesystem query failed");
        return false;
    }

    *out = fs_unix_kind_from_mode(st.st_mode);
    return true;
}

bool ucb_fs_plat_stat(const char* path, unsigned flags, ucb_fs_stat* out, ucb_error** perr)
{
    struct stat st;
    if (fs_unix_stat(path, flags, &st) != 0)
    {
        ucb_throw_errno(perr, errno, "filesystem stat failed");
        return false;
    }

    memset(out, 0, sizeof(*out));
    out->kind = fs_unix_kind_from_mode(st.st_mode);
    out->size = S_ISREG(st.st_mode) ? (uint64_t)st.st_size : 0u;
    out->mtime = (ucb_time)st.st_mtime;
    out->atime = (ucb_time)st.st_atime;
    out->ctime = (ucb_time)st.st_ctime;
    out->mode = (unsigned)(st.st_mode & 07777);
    out->is_readonly = (st.st_mode & (S_IWUSR | S_IWGRP | S_IWOTH)) == 0;
    return true;
}

bool ucb_fs_plat_stat_ns(const char* path, unsigned flags, ucb_fs_stat_ns* out, ucb_error** perr)
{
    struct stat st;
    if (fs_unix_stat(path, flags, &st) != 0)
    {
        ucb_throw_errno(perr, errno, "filesystem stat failed");
        return false;
    }

    out->mtime_ns = (int64_t)st.st_mtim.tv_sec * 1000000000 + st.st_mtim.tv_nsec;
    out->atime_ns = (int64_t)st.st_atim.tv_sec * 1000000000 + st.st_atim.tv_nsec;
    out->ctime_ns = (int64_t)st.st_ctim.tv_sec * 1000000000 + st.st_ctim.tv_nsec;
    return true;
}

bool ucb_fs_plat_mkdir(const char* path, ucb_error** perr)
{
    if (mkdir(path, 0777) != 0)
        return !ucb_throw_errno(perr, errno, "ucb_fs_mkdir: mkdir failed");
    return true;
}

bool ucb_fs_plat_remove_file(const char* path, ucb_error** perr)
{
    if (unlink(path) != 0)
        return !ucb_throw_errno(perr, errno, "ucb_fs_remove: unlink failed");
    return true;
}

bool ucb_fs_plat_remove_dir(const char* path, ucb_error** perr)
{
    if (rmdir(path) != 0)
        return !ucb_throw_errno(perr, errno, "ucb_fs_remove: rmdir failed");
    return true;
}

bool ucb_fs_plat_rename(const char* src, const char* dst, unsigned flags, ucb_error** perr)
{
    UCB_UNUSED(flags);

    if (rename(src, dst) != 0)
        return !ucb_throw_errno(perr, errno, "ucb_fs_move: rename failed");
    return true;
}

bool ucb_fs_plat_set_mode(const char* path, unsigned mode, ucb_error** perr)
{
    if (chmod(path, (mode_t)mode) != 0)
        return !ucb_throw_errno(perr, errno, "ucb_fs_copy: chmod failed");
    return true;
}

bool ucb_fs_plat_set_mtime(const char* path, ucb_time mtime, ucb_error** perr)
{
    struct timespec times[2];
    times[0].tv_sec = 0;
    times[0].tv_nsec = UTIME_OMIT;
    times[1].tv_sec = (time_t)mtime;
    times[1].tv_nsec = 0;

    if (utimensat(AT_FDCWD, path, times, 0) != 0)
        return !ucb_throw_errno(perr, errno, "ucb_fs_copy: utimensat failed");
    return true;
}

bool ucb_fs_plat_read_link(const char* path, char** out, ucb_error** perr)
{
    size_t cap = 256;
    char* buf = ucb_malloc(cap);
    if (!buf)
        return false;

    for (;;)
    {
        ssize_t n = readlink(path, buf, cap);
        if (n < 0)
        {
            ucb_throw_errno(perr, errno, "ucb_fs: readlink failed");
            ucb_free(buf);
            return false;
        }
        if ((size_t)n < cap)
        {
            buf[n] = '\0';
            *out = buf;
            return true;
        }

        cap *= 2;
        buf = ucb_realloc(buf, cap);
        if (!buf)
            return false;
    }
}

bool ucb_fs_plat_create_link(const char* target,
                             const char* linkpath,
                             bool target_is_dir,
                             ucb_error** perr)
{
    UCB_UNUSED(target_is_dir);

    if (symlink(target, linkpath) != 0)
        return !ucb_throw_errno(perr, errno, "ucb_fs_copy: symlink failed");
    return true;
}

ucb_str* ucb_fs_plat_cwd(void)
{
    size_t cap = 256;
    char* buf = ucb_malloc(cap);
    if (!buf)
        return UCB_NULL;

    for (;;)
    {
        if (getcwd(buf, cap) != UCB_NULL)
            break;
        if (errno != ERANGE)
        {
            ucb_free(buf);
            return UCB_NULL;
        }
        cap *= 2;
        buf = ucb_realloc(buf, cap);
        if (!buf)
            return UCB_NULL;
    }

    ucb_str* result = ucb_str_new(buf, 0);
    ucb_free(buf);
    return result;
}

bool ucb_fs_plat_chdir(const char* path, ucb_error** perr)
{
    if (chdir(path) != 0)
        return !ucb_throw_errno(perr, errno, "ucb_fs_chdir: chdir failed");
    return true;
}
