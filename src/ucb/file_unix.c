/**
 * @file file_unix.c
 *
 * This file is part of the UCB project
 * - SPDX-FileCopyrightText: © 2026 Åke Svedin <ake@svedin.org>
 * - SPDX-License-Identifier: MIT
 *
 * @brief POSIX implementation of the unified file handle
 */

#ifndef _WIN32
#define _FILE_OFFSET_BITS 64
#endif

#include "file_private.h"

#include <ucb/errcodes.h>
#include <ucb/error.h>
#include <ucb/memory.h>

#include <errno.h>
#include <fcntl.h>
#include <poll.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

#if defined(__linux__) || defined(__FreeBSD__) || defined(__NetBSD__) || defined(__OpenBSD__)
#define UCB_UNIX_HAVE_PIPE2 1
#endif

static int ucb_file_unix_query(ucb_file* file)
{
    struct stat st;
    if (fstat(file->fd, &st) != 0)
        return -1;

    if (isatty(file->fd))
        file->kind = UCB_FILE_KIND_CONSOLE;
    else if (S_ISREG(st.st_mode))
        file->kind = UCB_FILE_KIND_REGULAR;
    else if (S_ISFIFO(st.st_mode))
        file->kind = UCB_FILE_KIND_PIPE;
    else if (S_ISCHR(st.st_mode) || S_ISBLK(st.st_mode))
        file->kind = UCB_FILE_KIND_DEVICE;
    else
        return 1; // Directories, sockets and other unsupported types

    ucb_file_apply_kind_caps(file);
    return 0;
}

static void ucb_file_unix_detect_flags(ucb_file* file)
{
    int fl = fcntl(file->fd, F_GETFL);
    if (fl >= 0)
    {
        int acc = fl & O_ACCMODE;
        if (acc == O_RDONLY)
            file->flags |= UCB_FILE_READ;
        else if (acc == O_WRONLY)
            file->flags |= UCB_FILE_WRITE;
        else
            file->flags |= UCB_FILE_READ | UCB_FILE_WRITE;

        if (fl & O_APPEND)
            file->flags |= UCB_FILE_APPEND;
        if (fl & O_NONBLOCK)
            file->flags |= UCB_FILE_NONBLOCK;
    }

    int fdflags = fcntl(file->fd, F_GETFD);
    if (fdflags >= 0)
    {
        if (fdflags & FD_CLOEXEC)
            file->flags |= UCB_FILE_CLOEXEC;
        else
            file->flags |= UCB_FILE_INHERIT;
    }
}

static ucb_file* ucb_file_unix_from_fd(int fd, bool own, ucb_error** perr)
{
    ucb_file* file = ucb_file_alloc();
    if (!file)
        return UCB_NULL;

    file->fd = fd;
    file->own = own;
    ucb_file_unix_detect_flags(file);
    ucb_file_set_io_caps(file);

    int status = ucb_file_unix_query(file);
    if (status != 0)
    {
        if (status < 0)
            ucb_throw_errno(perr, errno ? errno : EBADF, "ucb_file: invalid file descriptor");
        else
            ucb_throw(perr, UCB_ERROR_NOT_IMPLEMENTED, "ucb_file: unsupported file type");
        ucb_free(file);
        return UCB_NULL;
    }

    return file;
}

static int ucb_file_unix_openflags(unsigned flags)
{
    int oflags;
    if ((flags & (UCB_FILE_READ | UCB_FILE_WRITE)) == (UCB_FILE_READ | UCB_FILE_WRITE))
        oflags = O_RDWR;
    else if (flags & UCB_FILE_WRITE)
        oflags = O_WRONLY;
    else
        oflags = O_RDONLY;

    if (flags & UCB_FILE_APPEND)
        oflags |= O_APPEND;
    if (flags & UCB_FILE_TRUNCATE)
        oflags |= O_TRUNC;
    if (flags & UCB_FILE_CREATE)
        oflags |= O_CREAT;
    if (flags & UCB_FILE_EXCL)
        oflags |= O_EXCL;
    if (flags & UCB_FILE_NONBLOCK)
        oflags |= O_NONBLOCK;
#ifdef O_DIRECT
    if (flags & UCB_FILE_DIRECT)
        oflags |= O_DIRECT;
#endif
    return oflags;
}

static bool ucb_file_unix_apply_inherit(int fd, bool inherit, ucb_error** perr)
{
    int fdflags = fcntl(fd, F_GETFD);
    if (fdflags < 0)
        return !ucb_throw_errno(perr, errno, "ucb_file: fcntl(F_GETFD) failed");

    if (inherit)
        fdflags &= ~FD_CLOEXEC;
    else
        fdflags |= FD_CLOEXEC;

    if (fcntl(fd, F_SETFD, fdflags) < 0)
        return !ucb_throw_errno(perr, errno, "ucb_file: fcntl(F_SETFD) failed");

    return true;
}

ucb_file* ucb_file_plat_open(const char* path, unsigned flags, unsigned mode, ucb_error** perr)
{
    int oflags = ucb_file_unix_openflags(flags);
#ifdef O_CLOEXEC
    if (flags & UCB_FILE_CLOEXEC)
        oflags |= O_CLOEXEC;
#endif

    int fd = open(path, oflags, (mode_t)mode);
    if (fd < 0)
    {
        ucb_throw_errno(perr, errno, "ucb_file_open: failed to open file");
        return UCB_NULL;
    }

    if ((flags & UCB_FILE_CLOEXEC) || (flags & UCB_FILE_INHERIT))
    {
        if (!ucb_file_unix_apply_inherit(fd, (flags & UCB_FILE_INHERIT) != 0, perr))
        {
            close(fd);
            return UCB_NULL;
        }
    }

    ucb_file* file = ucb_file_unix_from_fd(fd, true, perr);
    if (!file)
    {
        close(fd);
        return UCB_NULL;
    }
    file->flags = flags;
    return file;
}

bool ucb_file_plat_close(ucb_file* file, ucb_error** perr)
{
    if (close(file->fd) != 0)
        return !ucb_throw_errno(perr, errno, "ucb_file_close: close failed");
    return true;
}

ucb_ssize ucb_file_plat_read(ucb_file* file, void* buf, size_t size, ucb_error** perr)
{
    for (;;)
    {
        ssize_t n = read(file->fd, buf, size);
        if (n >= 0)
            return (ucb_ssize)n;
        if (errno == EINTR)
            continue;
        ucb_throw_errno(perr, errno, "ucb_file_read: read failed");
        return -1;
    }
}

ucb_ssize ucb_file_plat_write(ucb_file* file, const void* buf, size_t size, ucb_error** perr)
{
    for (;;)
    {
        ssize_t n = write(file->fd, buf, size);
        if (n >= 0)
            return (ucb_ssize)n;
        if (errno == EINTR)
            continue;
        ucb_throw_errno(perr, errno, "ucb_file_write: write failed");
        return -1;
    }
}

bool ucb_file_plat_seek(ucb_file* file, int64_t offset, ucb_file_whence whence, ucb_error** perr)
{
    int w = (whence == UCB_FILE_SEEK_SET)   ? SEEK_SET
            : (whence == UCB_FILE_SEEK_CUR) ? SEEK_CUR
                                            : SEEK_END;
    if (lseek(file->fd, (off_t)offset, w) == (off_t)-1)
        return !ucb_throw_errno(perr, errno, "ucb_file_seek: lseek failed");
    return true;
}

bool ucb_file_plat_tell(const ucb_file* file, int64_t* out_pos, ucb_error** perr)
{
    off_t pos = lseek(file->fd, 0, SEEK_CUR);
    if (pos == (off_t)-1)
        return !ucb_throw_errno(perr, errno, "ucb_file_tell: lseek failed");
    *out_pos = (int64_t)pos;
    return true;
}

bool ucb_file_plat_sync(ucb_file* file, ucb_error** perr)
{
    if (fsync(file->fd) != 0)
        return !ucb_throw_errno(perr, errno, "ucb_file_sync: fsync failed");
    return true;
}

ucb_file* ucb_file_plat_dup(ucb_file* file, ucb_error** perr)
{
    // Prefer an atomic close-on-exec duplicate so there is no window where the
    // new descriptor is inheritable. The original inheritance state is applied
    // afterwards.
#ifdef F_DUPFD_CLOEXEC
    int fd = fcntl(file->fd, F_DUPFD_CLOEXEC, 0);
#else
    int fd = dup(file->fd);
#endif
    if (fd < 0)
    {
        ucb_throw_errno(perr, errno, "ucb_file_dup: dup failed");
        return UCB_NULL;
    }

    int fdflags = fcntl(file->fd, F_GETFD);
    if (fdflags >= 0)
        (void)fcntl(fd, F_SETFD, fdflags & FD_CLOEXEC);

    ucb_file* dup_file = ucb_file_unix_from_fd(fd, true, perr);
    if (!dup_file)
    {
        close(fd);
        return UCB_NULL;
    }
    dup_file->flags = file->flags;
    return dup_file;
}

bool ucb_file_plat_set_inherit(ucb_file* file, bool inherit, ucb_error** perr)
{
    if (!ucb_file_unix_apply_inherit(file->fd, inherit, perr))
        return false;

    file->flags &= ~(UCB_FILE_CLOEXEC | UCB_FILE_INHERIT);
    file->flags |= inherit ? UCB_FILE_INHERIT : UCB_FILE_CLOEXEC;
    return true;
}

ucb_file* ucb_file_plat_from_fd(int fd, bool own, ucb_error** perr)
{
    return ucb_file_unix_from_fd(fd, own, perr);
}

int ucb_file_plat_get_fd(const ucb_file* file)
{
    return file->fd;
}

bool ucb_file_plat_has_resource(const ucb_file* file)
{
    return file->fd >= 0;
}

bool ucb_file_plat_wait(ucb_file* file, bool writable, int timeout_ms, ucb_error** perr)
{
    struct pollfd pfd;
    pfd.fd = file->fd;
    pfd.events = (short)(writable ? POLLOUT : POLLIN);
    pfd.revents = 0;

    int r;
    do
    {
        r = poll(&pfd, 1, timeout_ms);
    } while (r < 0 && errno == EINTR);

    if (r < 0)
        return !ucb_throw_errno(perr, errno, "ucb_file_wait: poll failed");
    return r > 0;
}

bool ucb_pipe_plat_create(ucb_file** out_read,
                          ucb_file** out_write,
                          unsigned flags,
                          ucb_error** perr)
{
    bool inherit = (flags & UCB_FILE_INHERIT) != 0;
    bool cloexec = (flags & UCB_FILE_CLOEXEC) != 0;

    int fds[2];
#ifdef UCB_UNIX_HAVE_PIPE2
    int pflags = 0;
    if (flags & UCB_FILE_NONBLOCK)
        pflags |= O_NONBLOCK;
#ifdef O_CLOEXEC
    if (cloexec)
        pflags |= O_CLOEXEC;
#endif
    if (pipe2(fds, pflags) != 0)
    {
        ucb_throw_errno(perr, errno, "ucb_pipe_create: pipe failed");
        return false;
    }
#else
    if (pipe(fds) != 0)
    {
        ucb_throw_errno(perr, errno, "ucb_pipe_create: pipe failed");
        return false;
    }

    for (int i = 0; i < 2; ++i)
    {
        if (flags & UCB_FILE_NONBLOCK)
        {
            int fl = fcntl(fds[i], F_GETFL);
            if (fl >= 0)
                (void)fcntl(fds[i], F_SETFL, fl | O_NONBLOCK);
        }

        if (cloexec && !ucb_file_unix_apply_inherit(fds[i], false, perr))
        {
            close(fds[0]);
            close(fds[1]);
            return false;
        }
    }
#endif

    if (inherit)
    {
        for (int i = 0; i < 2; ++i)
        {
            if (!ucb_file_unix_apply_inherit(fds[i], true, perr))
            {
                close(fds[0]);
                close(fds[1]);
                return false;
            }
        }
    }

    ucb_file* read_end = ucb_file_unix_from_fd(fds[0], true, perr);
    if (!read_end)
    {
        close(fds[0]);
        close(fds[1]);
        return false;
    }

    ucb_file* write_end = ucb_file_unix_from_fd(fds[1], true, perr);
    if (!write_end)
    {
        ucb_file_free(read_end);
        close(fds[1]);
        return false;
    }

    if (flags & UCB_FILE_NONBLOCK)
    {
        read_end->flags |= UCB_FILE_NONBLOCK;
        write_end->flags |= UCB_FILE_NONBLOCK;
    }
    if (cloexec)
    {
        read_end->flags |= UCB_FILE_CLOEXEC;
        write_end->flags |= UCB_FILE_CLOEXEC;
    }

    *out_read = read_end;
    *out_write = write_end;
    return true;
}

static ucb_file s_ucb_std[3];

ucb_file* ucb_file_plat_std(unsigned std_id)
{
    if (std_id > 2)
        std_id = 0;

    ucb_file* file = &s_ucb_std[std_id];
    memset(file, 0, sizeof(*file));

    file->fd = (int)std_id;
    file->own = false;
    file->is_static = true;
    ucb_file_unix_detect_flags(file);
    ucb_file_set_io_caps(file);
    if (ucb_file_unix_query(file) != 0)
    {
        file->kind = UCB_FILE_KIND_UNKNOWN;
        file->caps = 0;
    }
    return file;
}
