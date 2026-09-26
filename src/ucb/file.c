/**
 * @file file.c
 *
 * This file is part of the UCB project
 * - SPDX-FileCopyrightText: © 2026 Åke Svedin <ake@svedin.org>
 * - SPDX-License-Identifier: MIT
 *
 * @brief Unified file handle, shared platform independent logic
 */

#ifndef _WIN32
#define _FILE_OFFSET_BITS 64
#endif

#include "file_private.h"

#include <ucb/buffer.h>
#include <ucb/errcodes.h>
#include <ucb/memory.h>
#include <ucb/once.h>
#include <ucb/pipe.h>

#include <string.h>

#define UCB_FILE_READ_CHUNK 4096

ucb_file* ucb_file_alloc(void)
{
    ucb_file* file = ucb_calloc_type(1, ucb_file);
    if (!file)
        return UCB_NULL;

    file->kind = UCB_FILE_KIND_UNKNOWN;
    file->caps = 0;
    file->flags = 0;
    file->own = false;
    file->is_static = false;
#ifdef _WIN32
    file->handle = UCB_NULL;
    file->crt_fd = -1;
#else
    file->fd = -1;
#endif
    return file;
}

void ucb_file_set_io_caps(ucb_file* file)
{
    if (file->flags & UCB_FILE_READ)
        file->caps |= UCB_FILE_CAP_READ;
    if (file->flags & UCB_FILE_WRITE)
        file->caps |= UCB_FILE_CAP_WRITE;
}

void ucb_file_apply_kind_caps(ucb_file* file)
{
    if (file->kind == UCB_FILE_KIND_REGULAR || file->kind == UCB_FILE_KIND_DEVICE)
        file->caps |= UCB_FILE_CAP_SEEK;
    if (file->kind != UCB_FILE_KIND_UNKNOWN)
        file->caps |= UCB_FILE_CAP_POLL;
}

static void ucb_file_invalidate(ucb_file* file)
{
    file->kind = UCB_FILE_KIND_UNKNOWN;
    file->caps = 0;
    file->flags = 0;
    file->own = false;
#ifdef _WIN32
    file->handle = UCB_NULL;
    file->crt_fd = -1;
#else
    file->fd = -1;
#endif
}

static void ucb_file_throw_closed(ucb_error** perr, const char* op)
{
    ucb_throw_format(perr, UCB_ERROR_INVALID_STATE, "ucb_file_%s: handle is not open", op);
}

static void ucb_file_throw_cap(ucb_error** perr, const char* op, const char* cap)
{
    ucb_throw_format(perr,
                     UCB_ERROR_NOT_IMPLEMENTED,
                     "ucb_file_%s: handle does not support %s",
                     op,
                     cap);
}

/* -------------------------------------------------------------------------- */
/*                              Open and lifetime                             */
/* -------------------------------------------------------------------------- */

ucb_file* ucb_file_open_mode(const char* path, unsigned flags, unsigned mode, ucb_error** perr)
{
    UCB_VERIFY_ARGS(path);

    if ((flags & (UCB_FILE_READ | UCB_FILE_WRITE)) == 0)
    {
        ucb_throw(perr,
                  UCB_ERROR_INVALID_ARG,
                  "ucb_file_open: flags must include UCB_FILE_READ or UCB_FILE_WRITE");
        return UCB_NULL;
    }

    return ucb_file_plat_open(path, flags, mode, perr);
}

ucb_file* ucb_file_open(const char* path, unsigned flags, ucb_error** perr)
{
    return ucb_file_open_mode(path, flags, 0666u, perr);
}

bool ucb_file_close(ucb_file* file, ucb_error** perr)
{
    if (!file || file->is_static)
        return true;

    if (file->own && ucb_file_plat_has_resource(file))
    {
        if (!ucb_file_plat_close(file, perr))
            return false;
    }

    ucb_file_invalidate(file);
    return true;
}

void ucb_file_free(ucb_file* file)
{
    if (!file || file->is_static)
        return;

    if (file->own && ucb_file_plat_has_resource(file))
        (void)ucb_file_plat_close(file, UCB_NULL);

    ucb_file_invalidate(file);
    ucb_free(file);
}

/* -------------------------------------------------------------------------- */
/*                               Native interop                               */
/* -------------------------------------------------------------------------- */

ucb_file* ucb_file_from_fd(int fd, bool own, ucb_error** perr)
{
    if (fd < 0)
    {
        ucb_throw(perr, UCB_ERROR_INVALID_ARG, "ucb_file_from_fd: fd must be non-negative");
        return UCB_NULL;
    }

    return ucb_file_plat_from_fd(fd, own, perr);
}

int ucb_file_get_fd(const ucb_file* file)
{
    if (!ucb_file_is_valid(file))
        return -1;
    return ucb_file_plat_get_fd(file);
}

#ifdef _WIN32
ucb_file* ucb_file_from_handle(void* handle, bool own, ucb_error** perr)
{
    if (!handle || handle == (void*)(intptr_t)-1)
    {
        ucb_throw(perr, UCB_ERROR_INVALID_ARG, "ucb_file_from_handle: handle is invalid");
        return UCB_NULL;
    }

    return ucb_file_plat_from_handle(handle, own, perr);
}

void* ucb_file_get_handle(const ucb_file* file)
{
    if (!ucb_file_is_valid(file))
        return UCB_NULL;
    return ucb_file_plat_get_handle(file);
}
#endif // _WIN32

/* -------------------------------------------------------------------------- */
/*                            Duplication and sharing                         */
/* -------------------------------------------------------------------------- */

ucb_file* ucb_file_dup(ucb_file* file, ucb_error** perr)
{
    UCB_VERIFY_ARGS(file);

    if (!ucb_file_is_valid(file))
    {
        ucb_file_throw_closed(perr, "dup");
        return UCB_NULL;
    }

    return ucb_file_plat_dup(file, perr);
}

bool ucb_file_set_inherit(ucb_file* file, bool inherit, ucb_error** perr)
{
    UCB_VERIFY_ARGS(file);

    if (!ucb_file_is_valid(file))
    {
        ucb_file_throw_closed(perr, "set_inherit");
        return false;
    }

    return ucb_file_plat_set_inherit(file, inherit, perr);
}

/* -------------------------------------------------------------------------- */
/*                                    I/O                                     */
/* -------------------------------------------------------------------------- */

ucb_ssize ucb_file_read(ucb_file* file, void* buf, size_t size, ucb_error** perr)
{
    UCB_VERIFY_ARGS(file);

    if (size == 0)
        return 0;
    UCB_VERIFY_ARGS(buf);

    if (!ucb_file_is_valid(file))
    {
        ucb_file_throw_closed(perr, "read");
        return -1;
    }
    if (!(file->caps & UCB_FILE_CAP_READ))
    {
        ucb_file_throw_cap(perr, "read", "read");
        return -1;
    }

    return ucb_file_plat_read(file, buf, size, perr);
}

ucb_ssize ucb_file_write(ucb_file* file, const void* buf, size_t size, ucb_error** perr)
{
    UCB_VERIFY_ARGS(file);

    if (size == 0)
        return 0;
    UCB_VERIFY_ARGS(buf);

    if (!ucb_file_is_valid(file))
    {
        ucb_file_throw_closed(perr, "write");
        return -1;
    }
    if (!(file->caps & UCB_FILE_CAP_WRITE))
    {
        ucb_file_throw_cap(perr, "write", "write");
        return -1;
    }

    return ucb_file_plat_write(file, buf, size, perr);
}

bool ucb_file_read_full(ucb_file* file,
                        void* buf,
                        size_t capacity,
                        size_t* out_size,
                        ucb_error** perr)
{
    UCB_VERIFY_ARGS(file);
    UCB_VERIFY_ARGS(out_size);

    if (!ucb_file_is_valid(file))
    {
        ucb_file_throw_closed(perr, "read_full");
        return false;
    }
    if (!(file->caps & UCB_FILE_CAP_READ))
    {
        ucb_file_throw_cap(perr, "read_full", "read");
        return false;
    }
    if (!(file->caps & UCB_FILE_CAP_SEEK))
    {
        ucb_throw(perr,
                  UCB_ERROR_NOT_IMPLEMENTED,
                  "ucb_file_read_full: a seekable handle is required");
        return false;
    }

    int64_t cur = 0;
    int64_t end = 0;
    if (!ucb_file_tell(file, &cur, perr))
        return false;
    if (!ucb_file_seek(file, 0, UCB_FILE_SEEK_END, perr))
        return false;
    if (!ucb_file_tell(file, &end, perr))
        return false;
    if (!ucb_file_seek(file, cur, UCB_FILE_SEEK_SET, perr))
        return false;

    if (end < cur)
        end = cur;
    size_t remaining = (size_t)(end - cur);

    if (capacity < remaining)
    {
        *out_size = remaining;
        ucb_throw_format(perr,
                         UCB_ERROR_OUT_OF_BOUNDS,
                         "ucb_file_read_full: buffer too small (%zu bytes required)",
                         remaining);
        return false;
    }

    if (remaining > 0 && !buf)
    {
        UCB_REPORT(UCB_ERROR_INVALID_ARG, "ucb_file_read_full: buffer is NULL");
        return false;
    }

    char* dst = (char*)buf;
    size_t got = 0;
    while (got < remaining)
    {
        ucb_ssize n = ucb_file_read(file, dst + got, remaining - got, perr);
        if (n < 0)
            return false;
        if (n == 0)
            break;
        got += (size_t)n;
    }

    *out_size = got;
    return true;
}

bool ucb_file_write_full(ucb_file* file, const void* buf, size_t size, ucb_error** perr)
{
    UCB_VERIFY_ARGS(file);

    if (size == 0)
        return true;
    UCB_VERIFY_ARGS(buf);

    const char* data = (const char*)buf;
    size_t written = 0;
    while (written < size)
    {
        ucb_ssize n = ucb_file_write(file, data + written, size - written, perr);
        if (n < 0)
            return false;
        if (n == 0)
        {
            ucb_throw(perr, UCB_ERROR_INTERNAL, "ucb_file_write_full: write made no progress");
            return false;
        }
        written += (size_t)n;
    }
    return true;
}

bool ucb_file_read_buffer(ucb_file* file, ucb_buffer* out, ucb_error** perr)
{
    UCB_VERIFY_ARGS(file);
    UCB_VERIFY_ARGS(out);

    // Use exponential growth for the duration of this call so appending chunks
    // does not force a realloc on every read. The caller's grow function is
    // restored afterwards.
    ucb_buffer_grow_func prev_grow = out->grow_func;
    if (!prev_grow)
        out->grow_func = ucb_buffer_grow_double;

    char chunk[UCB_FILE_READ_CHUNK];
    bool ok = true;
    for (;;)
    {
        ucb_ssize n = ucb_file_read(file, chunk, sizeof(chunk), perr);
        if (n < 0)
        {
            ok = false;
            break;
        }
        if (n == 0)
            break;
        if (!ucb_buffer_push(out, chunk, (size_t)n))
        {
            ucb_throw(perr, UCB_ERROR_OUT_OF_MEMORY, "ucb_file_read_buffer: failed to grow buffer");
            ok = false;
            break;
        }
    }

    out->grow_func = prev_grow;
    return ok;
}

bool ucb_file_write_buffer(ucb_file* file, const ucb_buffer* buf, ucb_error** perr)
{
    UCB_VERIFY_ARGS(file);
    UCB_VERIFY_ARGS(buf);

    if (buf->size == 0)
        return true;
    UCB_VERIFY_ARGS(buf->data);

    return ucb_file_write_full(file, buf->data, buf->size, perr);
}

/* -------------------------------------------------------------------------- */
/*                             Position and sync                              */
/* -------------------------------------------------------------------------- */

bool ucb_file_seek(ucb_file* file, int64_t offset, ucb_file_whence whence, ucb_error** perr)
{
    UCB_VERIFY_ARGS(file);

    if (!ucb_file_is_valid(file))
    {
        ucb_file_throw_closed(perr, "seek");
        return false;
    }
    if (!(file->caps & UCB_FILE_CAP_SEEK))
    {
        ucb_file_throw_cap(perr, "seek", "seek");
        return false;
    }

    return ucb_file_plat_seek(file, offset, whence, perr);
}

bool ucb_file_tell(const ucb_file* file, int64_t* out_pos, ucb_error** perr)
{
    UCB_VERIFY_ARGS(file);
    UCB_VERIFY_ARGS(out_pos);

    if (!ucb_file_is_valid(file))
    {
        ucb_file_throw_closed(perr, "tell");
        return false;
    }
    if (!(file->caps & UCB_FILE_CAP_SEEK))
    {
        ucb_file_throw_cap(perr, "tell", "seek");
        return false;
    }

    return ucb_file_plat_tell(file, out_pos, perr);
}

bool ucb_file_flush(ucb_file* file, ucb_error** perr)
{
    UCB_VERIFY_ARGS(file);

    if (!ucb_file_is_valid(file))
    {
        ucb_file_throw_closed(perr, "flush");
        return false;
    }

    // The data plane is unbuffered; nothing to flush.
    return true;
}

bool ucb_file_sync(ucb_file* file, ucb_error** perr)
{
    UCB_VERIFY_ARGS(file);

    if (!ucb_file_is_valid(file))
    {
        ucb_file_throw_closed(perr, "sync");
        return false;
    }

    return ucb_file_plat_sync(file, perr);
}

/* -------------------------------------------------------------------------- */
/*                                   Query                                    */
/* -------------------------------------------------------------------------- */

ucb_file_kind ucb_file_get_kind(const ucb_file* file)
{
    if (!file)
        return UCB_FILE_KIND_UNKNOWN;
    return file->kind;
}

unsigned ucb_file_get_caps(const ucb_file* file)
{
    if (!file)
        return 0u;
    return file->caps;
}

bool ucb_file_is_valid(const ucb_file* file)
{
    if (!file || file->kind == UCB_FILE_KIND_UNKNOWN || file->caps == 0u)
        return false;
#ifdef _WIN32
    return file->handle != UCB_NULL;
#else
    return file->fd >= 0;
#endif
}

/* -------------------------------------------------------------------------- */
/*                              Standard streams                              */
/* -------------------------------------------------------------------------- */

static ucb_file* s_ucb_std[3];
static ucb_once s_ucb_std_once = UCB_ONCE_INIT;

static void ucb_file_std_init(void)
{
    s_ucb_std[0] = ucb_file_plat_std(0);
    s_ucb_std[1] = ucb_file_plat_std(1);
    s_ucb_std[2] = ucb_file_plat_std(2);
}

static ucb_file* ucb_file_std(unsigned id)
{
    ucb_once_run(&s_ucb_std_once, ucb_file_std_init);
    return s_ucb_std[id];
}

ucb_file* ucb_file_stdin(void)
{
    return ucb_file_std(0);
}

ucb_file* ucb_file_stdout(void)
{
    return ucb_file_std(1);
}

ucb_file* ucb_file_stderr(void)
{
    return ucb_file_std(2);
}

/* -------------------------------------------------------------------------- */
/*                                Wait helpers                                */
/* -------------------------------------------------------------------------- */

static bool ucb_file_wait_common(ucb_file* file, bool writable, int timeout_ms, ucb_error** perr)
{
    UCB_VERIFY_ARGS(file);

    if (!ucb_file_is_valid(file))
    {
        ucb_file_throw_closed(perr, writable ? "wait_writable" : "wait_readable");
        return false;
    }
    if (!(file->caps & UCB_FILE_CAP_POLL))
    {
        ucb_file_throw_cap(perr, writable ? "wait_writable" : "wait_readable", "poll");
        return false;
    }

    return ucb_file_plat_wait(file, writable, timeout_ms, perr);
}

bool ucb_file_wait_readable(ucb_file* file, int timeout_ms, ucb_error** perr)
{
    return ucb_file_wait_common(file, false, timeout_ms, perr);
}

bool ucb_file_wait_writable(ucb_file* file, int timeout_ms, ucb_error** perr)
{
    return ucb_file_wait_common(file, true, timeout_ms, perr);
}

/* -------------------------------------------------------------------------- */
/*                                   Pipes                                    */
/* -------------------------------------------------------------------------- */

bool ucb_pipe_create(ucb_file** out_read, ucb_file** out_write, unsigned flags, ucb_error** perr)
{
    UCB_VERIFY_ARGS(out_read);
    UCB_VERIFY_ARGS(out_write);

    *out_read = UCB_NULL;
    *out_write = UCB_NULL;

    return ucb_pipe_plat_create(out_read, out_write, flags, perr);
}
