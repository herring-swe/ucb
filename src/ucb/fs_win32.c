/**
 * @file fs_win32.c
 *
 * This file is part of the UCB project
 * - SPDX-FileCopyrightText: © 2026 Åke Svedin <ake@svedin.org>
 * - SPDX-License-Identifier: MIT
 *
 * @brief Windows implementation of the filesystem API
 *
 * Metadata is queried through a native handle opened with
 * @c FILE_FLAG_OPEN_REPARSE_POINT when @ref UCB_FS_NOFOLLOW is requested, so a
 * symbolic link or junction can be inspected without resolving it.
 */

#ifndef _WIN32
#error "This file is only for Windows"
#endif

#include "fs_private.h"

#include <ucb/cstring.h>
#include <ucb/errcodes.h>
#include <ucb/error.h>
#include <ucb/memory.h>

#include <string.h>

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef _WIN32_WINNT
#define _WIN32_WINNT 0x0601
#endif
// clang-format off
#include <Windows.h>
#include <winioctl.h>
// clang-format on

#ifndef SYMBOLIC_LINK_FLAG_ALLOW_UNPRIVILEGED_CREATE
#define SYMBOLIC_LINK_FLAG_ALLOW_UNPRIVILEGED_CREATE 0x2
#endif

#ifndef MAXIMUM_REPARSE_DATA_BUFFER_SIZE
#define MAXIMUM_REPARSE_DATA_BUFFER_SIZE 16384
#endif

#ifndef IO_REPARSE_TAG_SYMLINK
#define IO_REPARSE_TAG_SYMLINK 0xA000000C
#endif
#ifndef IO_REPARSE_TAG_MOUNT_POINT
#define IO_REPARSE_TAG_MOUNT_POINT 0xA0000003
#endif

/*
 * The reparse data layouts are declared locally instead of relying on the
 * REPARSE_DATA_BUFFER SDK type, which is gated behind SDK version macros that
 * are not consistently visible in a language-conformance build.
 */
typedef struct fs_win32_reparse_header
{
    ULONG tag;
    USHORT data_length;
    USHORT reserved;
} fs_win32_reparse_header;

typedef struct fs_win32_reparse_symlink
{
    fs_win32_reparse_header header;
    USHORT substitute_offset;
    USHORT substitute_length;
    USHORT print_offset;
    USHORT print_length;
    ULONG flags;
    WCHAR path[1];
} fs_win32_reparse_symlink;

typedef struct fs_win32_reparse_mount
{
    fs_win32_reparse_header header;
    USHORT substitute_offset;
    USHORT substitute_length;
    USHORT print_offset;
    USHORT print_length;
    WCHAR path[1];
} fs_win32_reparse_mount;

#define UCB_FS_WIN_EPOCH_100NS 116444736000000000ULL

static ucb_fs_kind fs_win32_kind_from_attrs(DWORD attrs)
{
    if (attrs & FILE_ATTRIBUTE_REPARSE_POINT)
        return UCB_FS_KIND_SYMLINK;
    if (attrs & FILE_ATTRIBUTE_DIRECTORY)
        return UCB_FS_KIND_DIR;
    if (attrs & FILE_ATTRIBUTE_DEVICE)
        return UCB_FS_KIND_OTHER;
    return UCB_FS_KIND_FILE;
}

static HANDLE fs_win32_open_info(const char* path, unsigned flags, ucb_error** perr)
{
    wchar_t* wpath = ucb_cstr_to_wchar(path, 0, UCB_NULL, perr);
    if (!wpath)
        return INVALID_HANDLE_VALUE;

    DWORD attr_flags = FILE_FLAG_BACKUP_SEMANTICS;
    if (flags & UCB_FS_NOFOLLOW)
        attr_flags |= FILE_FLAG_OPEN_REPARSE_POINT;

    HANDLE handle = CreateFileW(wpath,
                                0,
                                FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE,
                                UCB_NULL,
                                OPEN_EXISTING,
                                attr_flags,
                                UCB_NULL);
    DWORD err = GetLastError();
    ucb_free(wpath);

    if (handle == INVALID_HANDLE_VALUE)
        ucb_throw_win32(perr, err, "filesystem query failed");
    return handle;
}

bool ucb_fs_plat_kind(const char* path, unsigned flags, ucb_fs_kind* out, ucb_error** perr)
{
    HANDLE handle = fs_win32_open_info(path, flags, perr);
    if (handle == INVALID_HANDLE_VALUE)
        return false;

    FILE_ATTRIBUTE_TAG_INFO info;
    if (!GetFileInformationByHandleEx(handle, FileAttributeTagInfo, &info, sizeof(info)))
    {
        DWORD err = GetLastError();
        CloseHandle(handle);
        ucb_throw_win32(perr, err, "filesystem query failed");
        return false;
    }

    CloseHandle(handle);
    *out = fs_win32_kind_from_attrs(info.FileAttributes);
    return true;
}

static int64_t fs_win32_time_ns(LARGE_INTEGER value)
{
    if (value.QuadPart == 0)
        return 0;
    return (int64_t)((value.QuadPart - UCB_FS_WIN_EPOCH_100NS) * 100ULL);
}

bool ucb_fs_plat_stat(const char* path, unsigned flags, ucb_fs_stat* out, ucb_error** perr)
{
    HANDLE handle = fs_win32_open_info(path, flags, perr);
    if (handle == INVALID_HANDLE_VALUE)
        return false;

    FILE_BASIC_INFO basic;
    FILE_STANDARD_INFO standard;
    memset(&basic, 0, sizeof(basic));
    memset(&standard, 0, sizeof(standard));
    bool ok = GetFileInformationByHandleEx(handle, FileBasicInfo, &basic, sizeof(basic)) &&
              GetFileInformationByHandleEx(handle, FileStandardInfo, &standard, sizeof(standard));
    DWORD err = GetLastError();
    CloseHandle(handle);

    if (!ok)
    {
        ucb_throw_win32(perr, err, "filesystem stat failed");
        return false;
    }

    int64_t mtime_ns = fs_win32_time_ns(basic.LastWriteTime);
    int64_t atime_ns = fs_win32_time_ns(basic.LastAccessTime);
    int64_t ctime_ns = fs_win32_time_ns(basic.ChangeTime);

    memset(out, 0, sizeof(*out));
    out->kind = fs_win32_kind_from_attrs(basic.FileAttributes);
    out->size = standard.Directory ? 0u : (uint64_t)standard.EndOfFile.QuadPart;
    out->mtime = (ucb_time)(mtime_ns / 1000000000);
    out->atime = (ucb_time)(atime_ns / 1000000000);
    out->ctime = (ucb_time)(ctime_ns / 1000000000);
    out->mode = 0;
    out->is_readonly = (basic.FileAttributes & FILE_ATTRIBUTE_READONLY) != 0;
    return true;
}

bool ucb_fs_plat_stat_ns(const char* path, unsigned flags, ucb_fs_stat_ns* out, ucb_error** perr)
{
    HANDLE handle = fs_win32_open_info(path, flags, perr);
    if (handle == INVALID_HANDLE_VALUE)
        return false;

    FILE_BASIC_INFO basic;
    bool ok = GetFileInformationByHandleEx(handle, FileBasicInfo, &basic, sizeof(basic));
    DWORD err = GetLastError();
    CloseHandle(handle);

    if (!ok)
    {
        ucb_throw_win32(perr, err, "filesystem stat failed");
        return false;
    }

    out->mtime_ns = fs_win32_time_ns(basic.LastWriteTime);
    out->atime_ns = fs_win32_time_ns(basic.LastAccessTime);
    out->ctime_ns = fs_win32_time_ns(basic.ChangeTime);
    return true;
}

bool ucb_fs_plat_mkdir(const char* path, ucb_error** perr)
{
    wchar_t* wpath = ucb_cstr_to_wchar(path, 0, UCB_NULL, perr);
    if (!wpath)
        return false;

    BOOL ok = CreateDirectoryW(wpath, UCB_NULL);
    DWORD err = GetLastError();
    ucb_free(wpath);

    if (!ok)
        return !ucb_throw_win32(perr, err, "ucb_fs_mkdir: CreateDirectory failed");
    return true;
}

bool ucb_fs_plat_remove_file(const char* path, ucb_error** perr)
{
    wchar_t* wpath = ucb_cstr_to_wchar(path, 0, UCB_NULL, perr);
    if (!wpath)
        return false;

    DWORD err = ERROR_SUCCESS;
    bool ok = DeleteFileW(wpath) != 0;
    if (!ok)
    {
        err = GetLastError();
        // A directory symlink or junction is removed with RemoveDirectoryW.
        ok = RemoveDirectoryW(wpath) != 0;
    }
    ucb_free(wpath);

    if (!ok)
        return !ucb_throw_win32(perr, err, "ucb_fs_remove: failed to remove file");
    return true;
}

bool ucb_fs_plat_remove_dir(const char* path, ucb_error** perr)
{
    wchar_t* wpath = ucb_cstr_to_wchar(path, 0, UCB_NULL, perr);
    if (!wpath)
        return false;

    BOOL ok = RemoveDirectoryW(wpath);
    DWORD err = GetLastError();
    ucb_free(wpath);

    if (!ok)
        return !ucb_throw_win32(perr, err, "ucb_fs_remove: RemoveDirectory failed");
    return true;
}

bool ucb_fs_plat_rename(const char* src, const char* dst, unsigned flags, ucb_error** perr)
{
    wchar_t* wsrc = ucb_cstr_to_wchar(src, 0, UCB_NULL, perr);
    if (!wsrc)
        return false;
    wchar_t* wdst = ucb_cstr_to_wchar(dst, 0, UCB_NULL, perr);
    if (!wdst)
    {
        ucb_free(wsrc);
        return false;
    }

    DWORD move_flags = (flags & UCB_FS_OVERWRITE) ? MOVEFILE_REPLACE_EXISTING : 0;
    BOOL ok = MoveFileExW(wsrc, wdst, move_flags);
    DWORD err = GetLastError();
    ucb_free(wsrc);
    ucb_free(wdst);

    if (!ok)
        return !ucb_throw_win32(perr, err, "ucb_fs_move: MoveFileEx failed");
    return true;
}

bool ucb_fs_plat_set_mode(const char* path, unsigned mode, ucb_error** perr)
{
    UCB_UNUSED(path);
    UCB_UNUSED(mode);
    UCB_UNUSED(perr);
    return true;
}

static FILETIME fs_win32_filetime_from_sec(ucb_time secs)
{
    ULARGE_INTEGER value;
    value.QuadPart = (uint64_t)secs * 10000000ULL + UCB_FS_WIN_EPOCH_100NS;

    FILETIME ft;
    ft.dwLowDateTime = value.LowPart;
    ft.dwHighDateTime = value.HighPart;
    return ft;
}

bool ucb_fs_plat_set_mtime(const char* path, ucb_time mtime, ucb_error** perr)
{
    wchar_t* wpath = ucb_cstr_to_wchar(path, 0, UCB_NULL, perr);
    if (!wpath)
        return false;

    HANDLE handle = CreateFileW(wpath,
                                FILE_WRITE_ATTRIBUTES,
                                FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE,
                                UCB_NULL,
                                OPEN_EXISTING,
                                FILE_FLAG_BACKUP_SEMANTICS,
                                UCB_NULL);
    DWORD err = GetLastError();
    ucb_free(wpath);

    if (handle == INVALID_HANDLE_VALUE)
        return !ucb_throw_win32(perr, err, "ucb_fs_copy: failed to open for mtime");

    FILETIME ft = fs_win32_filetime_from_sec(mtime);
    BOOL ok = SetFileTime(handle, UCB_NULL, UCB_NULL, &ft);
    err = GetLastError();
    CloseHandle(handle);

    if (!ok)
        return !ucb_throw_win32(perr, err, "ucb_fs_copy: SetFileTime failed");
    return true;
}

static char* fs_win32_reparse_name(const wchar_t* name, USHORT byte_len, ucb_error** perr)
{
    size_t chars = (size_t)byte_len / sizeof(wchar_t);
    while (chars > 0 && name[chars - 1] == L'\0')
        chars--;

    // Strip the "\??\" device prefix present in substitute names.
    const wchar_t* start = name;
    if (chars >= 4 && start[0] == L'\\' && start[1] == L'?' && start[2] == L'?' &&
        start[3] == L'\\')
    {
        start += 4;
        chars -= 4;
    }

    return ucb_cstr_from_wchar(start, chars, UCB_NULL, perr);
}

/**
 * @brief Extract the printable or substitute name from a reparse buffer.
 *
 * Validates the untrusted offset/length pair against @p data_length before it
 * is used as an index, so a malformed reparse point cannot cause an
 * out-of-bounds read.
 */
static char* fs_win32_reparse_pick(const wchar_t* path_buffer,
                                   USHORT data_length,
                                   USHORT print_offset,
                                   USHORT print_length,
                                   USHORT substitute_offset,
                                   USHORT substitute_length,
                                   ucb_error** perr)
{
    USHORT offset = (print_length > 0) ? print_offset : substitute_offset;
    USHORT length = (print_length > 0) ? print_length : substitute_length;

    if (length > 0)
    {
        unsigned long end = (unsigned long)offset + (unsigned long)length;
        if (offset % sizeof(wchar_t) != 0 || length % sizeof(wchar_t) != 0 ||
            end > (unsigned long)data_length)
        {
            ucb_throw(perr, UCB_ERROR_INVALID_ARG, "ucb_fs: malformed reparse point");
            return UCB_NULL;
        }
    }

    return fs_win32_reparse_name(path_buffer + offset / sizeof(wchar_t), length, perr);
}

bool ucb_fs_plat_read_link(const char* path, char** out, ucb_error** perr)
{
    wchar_t* wpath = ucb_cstr_to_wchar(path, 0, UCB_NULL, perr);
    if (!wpath)
        return false;

    HANDLE handle = CreateFileW(wpath,
                                0,
                                FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE,
                                UCB_NULL,
                                OPEN_EXISTING,
                                FILE_FLAG_BACKUP_SEMANTICS | FILE_FLAG_OPEN_REPARSE_POINT,
                                UCB_NULL);
    DWORD err = GetLastError();
    ucb_free(wpath);

    if (handle == INVALID_HANDLE_VALUE)
        return !ucb_throw_win32(perr, err, "ucb_fs: failed to open link");

    char* buffer = ucb_malloc(MAXIMUM_REPARSE_DATA_BUFFER_SIZE);
    if (!buffer)
    {
        CloseHandle(handle);
        return false;
    }

    DWORD bytes = 0;
    BOOL ok = DeviceIoControl(handle,
                              FSCTL_GET_REPARSE_POINT,
                              UCB_NULL,
                              0,
                              buffer,
                              MAXIMUM_REPARSE_DATA_BUFFER_SIZE,
                              &bytes,
                              UCB_NULL);
    err = GetLastError();
    CloseHandle(handle);

    if (!ok)
    {
        ucb_free(buffer);
        return !ucb_throw_win32(perr, err, "ucb_fs: FSCTL_GET_REPARSE_POINT failed");
    }

    fs_win32_reparse_header* reparse = (fs_win32_reparse_header*)buffer;
    char* result = UCB_NULL;

    if (bytes < sizeof(fs_win32_reparse_header) ||
        (size_t)sizeof(fs_win32_reparse_header) + (size_t)reparse->data_length > (size_t)bytes)
    {
        ucb_free(buffer);
        ucb_throw(perr, UCB_ERROR_INVALID_ARG, "ucb_fs: malformed reparse point");
        return false;
    }

    if (reparse->tag == IO_REPARSE_TAG_SYMLINK)
    {
        fs_win32_reparse_symlink* link = (fs_win32_reparse_symlink*)buffer;
        result = fs_win32_reparse_pick(link->path,
                                       reparse->data_length,
                                       link->print_offset,
                                       link->print_length,
                                       link->substitute_offset,
                                       link->substitute_length,
                                       perr);
    }
    else if (reparse->tag == IO_REPARSE_TAG_MOUNT_POINT)
    {
        fs_win32_reparse_mount* mount = (fs_win32_reparse_mount*)buffer;
        result = fs_win32_reparse_pick(mount->path,
                                       reparse->data_length,
                                       mount->print_offset,
                                       mount->print_length,
                                       mount->substitute_offset,
                                       mount->substitute_length,
                                       perr);
    }
    else
    {
        ucb_throw(perr, UCB_ERROR_NOT_IMPLEMENTED, "ucb_fs: unsupported reparse point");
    }

    ucb_free(buffer);
    if (!result)
        return false;

    *out = result;
    return true;
}

bool ucb_fs_plat_create_link(const char* target,
                             const char* linkpath,
                             bool target_is_dir,
                             ucb_error** perr)
{
    wchar_t* wtarget = ucb_cstr_to_wchar(target, 0, UCB_NULL, perr);
    if (!wtarget)
        return false;
    wchar_t* wlink = ucb_cstr_to_wchar(linkpath, 0, UCB_NULL, perr);
    if (!wlink)
    {
        ucb_free(wtarget);
        return false;
    }

    DWORD link_flags = SYMBOLIC_LINK_FLAG_ALLOW_UNPRIVILEGED_CREATE;
    if (target_is_dir)
        link_flags |= SYMBOLIC_LINK_FLAG_DIRECTORY;

    BOOL ok = CreateSymbolicLinkW(wlink, wtarget, link_flags);
    DWORD err = GetLastError();
    ucb_free(wtarget);
    ucb_free(wlink);

    if (!ok)
        return !ucb_throw_win32(perr, err, "ucb_fs_copy: CreateSymbolicLink failed");
    return true;
}

ucb_str* ucb_fs_plat_cwd(void)
{
    DWORD cap = GetCurrentDirectoryW(0, UCB_NULL);
    if (cap == 0)
        return UCB_NULL;

    wchar_t* wbuf = ucb_malloc((size_t)cap * sizeof(wchar_t));
    if (!wbuf)
        return UCB_NULL;

    DWORD len = GetCurrentDirectoryW(cap, wbuf);
    if (len == 0 || len >= cap)
    {
        ucb_free(wbuf);
        return UCB_NULL;
    }

    char* utf8 = ucb_cstr_from_wchar(wbuf, 0, UCB_NULL, UCB_NULL);
    ucb_free(wbuf);
    if (!utf8)
        return UCB_NULL;

    ucb_str* result = ucb_str_new(utf8, 0);
    ucb_free(utf8);
    return result;
}

bool ucb_fs_plat_chdir(const char* path, ucb_error** perr)
{
    wchar_t* wpath = ucb_cstr_to_wchar(path, 0, UCB_NULL, perr);
    if (!wpath)
        return false;

    BOOL ok = SetCurrentDirectoryW(wpath);
    DWORD err = GetLastError();
    ucb_free(wpath);

    if (!ok)
        return !ucb_throw_win32(perr, err, "ucb_fs_chdir: SetCurrentDirectory failed");
    return true;
}
