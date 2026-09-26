/**
 * @file file_win32.c
 *
 * This file is part of the UCB project
 * - SPDX-FileCopyrightText: © 2026 Åke Svedin <ake@svedin.org>
 * - SPDX-License-Identifier: MIT
 *
 * @brief Windows implementation of the unified file handle
 *
 * All I/O is performed through the native @c HANDLE. When a handle originates
 * from a C runtime descriptor, the descriptor is retained for
 * @ref ucb_file_get_fd but is not used for I/O, so callers must not mix CRT
 * stdio and @ref ucb_file operations on the same descriptor.
 */

#ifndef _WIN32
#error "This file is only for Windows"
#endif

#include "file_private.h"

#include <ucb/cstring.h>
#include <ucb/errcodes.h>
#include <ucb/error.h>
#include <ucb/memory.h>

#include <errno.h>
#include <fcntl.h>
#include <io.h>
#include <string.h>

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <Windows.h>

static bool ucb_file_win32_valid_handle(void* handle)
{
    return handle != UCB_NULL && handle != INVALID_HANDLE_VALUE;
}

static void ucb_file_win32_query(ucb_file* file)
{
    DWORD type = GetFileType((HANDLE)file->handle);
    switch (type)
    {
    case FILE_TYPE_DISK:
        file->kind = UCB_FILE_KIND_REGULAR;
        break;
    case FILE_TYPE_PIPE:
        file->kind = UCB_FILE_KIND_PIPE;
        break;
    case FILE_TYPE_CHAR:
    {
        DWORD mode;
        file->kind = GetConsoleMode((HANDLE)file->handle, &mode) ? UCB_FILE_KIND_CONSOLE
                                                                 : UCB_FILE_KIND_DEVICE;
        break;
    }
    default:
        file->kind = UCB_FILE_KIND_UNKNOWN;
        break;
    }

    ucb_file_apply_kind_caps(file);
}

static ucb_file* ucb_file_win32_build(void* handle,
                                      bool own,
                                      unsigned flags,
                                      int crt_fd,
                                      ucb_error** perr)
{
    if (!ucb_file_win32_valid_handle(handle))
    {
        ucb_throw_win32(perr, GetLastError(), "ucb_file: invalid handle");
        return UCB_NULL;
    }

    ucb_file* file = ucb_file_alloc();
    if (!file)
        return UCB_NULL;

    file->handle = handle;
    file->crt_fd = crt_fd;
    file->own = own;
    file->flags = flags;
    ucb_file_set_io_caps(file);
    ucb_file_win32_query(file);
    return file;
}

static DWORD ucb_file_win32_access(unsigned flags)
{
    DWORD access = 0;
    bool append = (flags & UCB_FILE_APPEND) != 0;

    if (flags & UCB_FILE_WRITE)
        access |= append ? FILE_APPEND_DATA : GENERIC_WRITE;

    // CREATE_ALWAYS and TRUNCATE_EXISTING require GENERIC_WRITE, which
    // FILE_APPEND_DATA does not satisfy.
    if (append && (flags & UCB_FILE_WRITE) && (flags & UCB_FILE_TRUNCATE))
        access |= GENERIC_WRITE;

    if (flags & UCB_FILE_READ)
        access |= GENERIC_READ;
    return access;
}

static DWORD ucb_file_win32_share(void)
{
    return FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE;
}

static DWORD ucb_file_win32_disposition(unsigned flags)
{
    if ((flags & UCB_FILE_CREATE) && (flags & UCB_FILE_EXCL))
        return CREATE_NEW;
    if (flags & UCB_FILE_TRUNCATE)
        return (flags & UCB_FILE_CREATE) ? CREATE_ALWAYS : TRUNCATE_EXISTING;
    if (flags & UCB_FILE_CREATE)
        return OPEN_ALWAYS;
    return OPEN_EXISTING;
}

static DWORD ucb_file_win32_attributes(unsigned flags)
{
    DWORD attrs = FILE_ATTRIBUTE_NORMAL;
    if (flags & UCB_FILE_DIRECT)
        attrs |= FILE_FLAG_NO_BUFFERING | FILE_FLAG_WRITE_THROUGH;
    return attrs;
}

ucb_file* ucb_file_plat_open(const char* path, unsigned flags, unsigned mode, ucb_error** perr)
{
    UCB_UNUSED(mode);

    wchar_t* wpath = ucb_cstr_to_wchar(path, 0, UCB_NULL, perr);
    if (!wpath)
        return UCB_NULL;

    SECURITY_ATTRIBUTES sa;
    sa.nLength = sizeof(sa);
    sa.lpSecurityDescriptor = UCB_NULL;
    sa.bInheritHandle = (flags & UCB_FILE_INHERIT) ? TRUE : FALSE;

    HANDLE handle = CreateFileW(wpath,
                                ucb_file_win32_access(flags),
                                ucb_file_win32_share(),
                                &sa,
                                ucb_file_win32_disposition(flags),
                                ucb_file_win32_attributes(flags),
                                UCB_NULL);
    ucb_free(wpath);

    if (!ucb_file_win32_valid_handle(handle))
    {
        ucb_throw_win32(perr, GetLastError(), "ucb_file_open: failed to open file");
        return UCB_NULL;
    }

    return ucb_file_win32_build(handle, true, flags, -1, perr);
}

bool ucb_file_plat_close(ucb_file* file, ucb_error** perr)
{
    // A descriptor wrapped via ucb_file_from_fd must be released through the CRT
    // so its descriptor table stays consistent. Closing it with CloseHandle
    // would leave a dangling descriptor that a later _close/fclose would close
    // again (possibly on a reused handle).
    if (file->crt_fd >= 0)
    {
        if (_close(file->crt_fd) != 0)
            return !ucb_throw_errno(perr, errno, "ucb_file_close: _close failed");
        return true;
    }

    if (!CloseHandle((HANDLE)file->handle))
        return !ucb_throw_win32(perr, GetLastError(), "ucb_file_close: CloseHandle failed");
    return true;
}

ucb_ssize ucb_file_plat_read(ucb_file* file, void* buf, size_t size, ucb_error** perr)
{
    DWORD chunk = (size > (size_t)MAXDWORD) ? MAXDWORD : (DWORD)size;
    DWORD read_bytes = 0;

    if (!ReadFile((HANDLE)file->handle, buf, chunk, &read_bytes, UCB_NULL))
    {
        DWORD err = GetLastError();
        // A broken pipe on the read end means the writer has closed: end of input.
        if (err == ERROR_BROKEN_PIPE || err == ERROR_HANDLE_EOF)
            return 0;
        ucb_throw_win32(perr, err, "ucb_file_read: ReadFile failed");
        return -1;
    }

    return (ucb_ssize)read_bytes;
}

ucb_ssize ucb_file_plat_write(ucb_file* file, const void* buf, size_t size, ucb_error** perr)
{
    DWORD chunk = (size > (size_t)MAXDWORD) ? MAXDWORD : (DWORD)size;
    DWORD written = 0;

    if (!WriteFile((HANDLE)file->handle, buf, chunk, &written, UCB_NULL))
    {
        ucb_throw_win32(perr, GetLastError(), "ucb_file_write: WriteFile failed");
        return -1;
    }

    return (ucb_ssize)written;
}

bool ucb_file_plat_seek(ucb_file* file, int64_t offset, ucb_file_whence whence, ucb_error** perr)
{
    DWORD method = (whence == UCB_FILE_SEEK_SET)   ? FILE_BEGIN
                   : (whence == UCB_FILE_SEEK_CUR) ? FILE_CURRENT
                                                   : FILE_END;
    LARGE_INTEGER distance;
    distance.QuadPart = offset;

    if (!SetFilePointerEx((HANDLE)file->handle, distance, UCB_NULL, method))
        return !ucb_throw_win32(perr, GetLastError(), "ucb_file_seek: SetFilePointerEx failed");
    return true;
}

bool ucb_file_plat_tell(const ucb_file* file, int64_t* out_pos, ucb_error** perr)
{
    LARGE_INTEGER distance;
    distance.QuadPart = 0;
    LARGE_INTEGER pos;

    if (!SetFilePointerEx((HANDLE)file->handle, distance, &pos, FILE_CURRENT))
        return !ucb_throw_win32(perr, GetLastError(), "ucb_file_tell: SetFilePointerEx failed");

    *out_pos = pos.QuadPart;
    return true;
}

bool ucb_file_plat_sync(ucb_file* file, ucb_error** perr)
{
    if (!FlushFileBuffers((HANDLE)file->handle))
        return !ucb_throw_win32(perr, GetLastError(), "ucb_file_sync: FlushFileBuffers failed");
    return true;
}

ucb_file* ucb_file_plat_dup(ucb_file* file, ucb_error** perr)
{
    DWORD handle_flags = 0;
    BOOL inherit = FALSE;
    if (GetHandleInformation((HANDLE)file->handle, &handle_flags))
        inherit = (handle_flags & HANDLE_FLAG_INHERIT) ? TRUE : FALSE;

    HANDLE new_handle = UCB_NULL;
    if (!DuplicateHandle(GetCurrentProcess(),
                         (HANDLE)file->handle,
                         GetCurrentProcess(),
                         &new_handle,
                         0,
                         inherit,
                         DUPLICATE_SAME_ACCESS))
    {
        ucb_throw_win32(perr, GetLastError(), "ucb_file_dup: DuplicateHandle failed");
        return UCB_NULL;
    }

    unsigned flags = file->flags & ~(UCB_FILE_CLOEXEC | UCB_FILE_INHERIT);
    flags |= inherit ? UCB_FILE_INHERIT : UCB_FILE_CLOEXEC;
    return ucb_file_win32_build(new_handle, true, flags, -1, perr);
}

bool ucb_file_plat_set_inherit(ucb_file* file, bool inherit, ucb_error** perr)
{
    if (!SetHandleInformation((HANDLE)file->handle,
                              HANDLE_FLAG_INHERIT,
                              inherit ? HANDLE_FLAG_INHERIT : 0))
        return !ucb_throw_win32(perr, GetLastError(), "ucb_file_set_inherit failed");

    file->flags &= ~(UCB_FILE_CLOEXEC | UCB_FILE_INHERIT);
    file->flags |= inherit ? UCB_FILE_INHERIT : UCB_FILE_CLOEXEC;
    return true;
}

ucb_file* ucb_file_plat_from_fd(int fd, bool own, ucb_error** perr)
{
    intptr_t os_handle = _get_osfhandle(fd);
    if (os_handle == -1)
    {
        ucb_throw_errno(perr, errno, "ucb_file_from_fd: invalid descriptor");
        return UCB_NULL;
    }

    return ucb_file_win32_build((HANDLE)os_handle, own, UCB_FILE_READ | UCB_FILE_WRITE, fd, perr);
}

ucb_file* ucb_file_plat_from_handle(void* handle, bool own, ucb_error** perr)
{
    return ucb_file_win32_build(handle, own, UCB_FILE_READ | UCB_FILE_WRITE, -1, perr);
}

int ucb_file_plat_get_fd(const ucb_file* file)
{
    return file->crt_fd;
}

bool ucb_file_plat_has_resource(const ucb_file* file)
{
    return ucb_file_win32_valid_handle(file->handle);
}

void* ucb_file_plat_get_handle(const ucb_file* file)
{
    return file->handle;
}

bool ucb_file_plat_wait(ucb_file* file, bool writable, int timeout_ms, ucb_error** perr)
{
    if (writable && file->kind == UCB_FILE_KIND_PIPE)
    {
        ucb_throw(perr,
                  UCB_ERROR_NOT_IMPLEMENTED,
                  "ucb_file_wait_writable: not supported for Windows anonymous pipes");
        return false;
    }

    if (writable)
    {
        // Disk, console and device handles are always writable from our point of view.
        return true;
    }

    DWORD timeout = (timeout_ms < 0) ? INFINITE : (DWORD)timeout_ms;
    DWORD result = WaitForSingleObject((HANDLE)file->handle, timeout);
    if (result == WAIT_OBJECT_0)
        return true;
    if (result == WAIT_TIMEOUT)
        return false;

    ucb_throw_win32(perr,
                    (result == WAIT_FAILED) ? GetLastError() : result,
                    "ucb_file_wait_readable: WaitForSingleObject failed");
    return false;
}

bool ucb_pipe_plat_create(ucb_file** out_read,
                          ucb_file** out_write,
                          unsigned flags,
                          ucb_error** perr)
{
    SECURITY_ATTRIBUTES sa;
    sa.nLength = sizeof(sa);
    sa.lpSecurityDescriptor = UCB_NULL;
    sa.bInheritHandle = (flags & UCB_FILE_INHERIT) ? TRUE : FALSE;

    HANDLE read_handle = UCB_NULL;
    HANDLE write_handle = UCB_NULL;
    if (!CreatePipe(&read_handle, &write_handle, &sa, 0))
    {
        ucb_throw_win32(perr, GetLastError(), "ucb_pipe_create: CreatePipe failed");
        return false;
    }

    unsigned common = flags & (UCB_FILE_CLOEXEC | UCB_FILE_INHERIT);
    ucb_file* read_end = ucb_file_win32_build(read_handle, true, UCB_FILE_READ | common, -1, perr);
    if (!read_end)
    {
        CloseHandle(read_handle);
        CloseHandle(write_handle);
        return false;
    }

    ucb_file* write_end =
        ucb_file_win32_build(write_handle, true, UCB_FILE_WRITE | common, -1, perr);
    if (!write_end)
    {
        ucb_file_free(read_end);
        CloseHandle(write_handle);
        return false;
    }

    *out_read = read_end;
    *out_write = write_end;
    return true;
}

static ucb_file s_ucb_std[3];

ucb_file* ucb_file_plat_std(unsigned std_id)
{
    static const DWORD std_ids[3] = {STD_INPUT_HANDLE, STD_OUTPUT_HANDLE, STD_ERROR_HANDLE};
    static const unsigned std_flags[3] = {UCB_FILE_READ, UCB_FILE_WRITE, UCB_FILE_WRITE};

    if (std_id > 2)
        std_id = 0;

    ucb_file* file = &s_ucb_std[std_id];
    memset(file, 0, sizeof(*file));

    file->handle = (void*)GetStdHandle(std_ids[std_id]);
    file->crt_fd = (int)std_id;
    file->own = false;
    file->is_static = true;
    file->flags = std_flags[std_id];

    if (!ucb_file_win32_valid_handle(file->handle))
    {
        file->handle = UCB_NULL;
        file->kind = UCB_FILE_KIND_UNKNOWN;
        file->caps = 0;
        return file;
    }

    ucb_file_set_io_caps(file);
    ucb_file_win32_query(file);
    return file;
}
