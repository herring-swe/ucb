/**
 * @file pipe.h
 * This file is part of the UCB project
 * - SPDX-FileCopyrightText: © 2026 Åke Svedin <ake@svedin.org>
 * - SPDX-License-Identifier: MIT
 *
 * @brief Anonymous pipe creation
 *
 * Pipes are represented by the unified @ref ucb_file handle, one for each end.
 * There is no separate pipe close call: release each end with
 * @ref ucb_file_close to close just the OS resource, or @ref ucb_file_free to
 * close and free it. Both ends must be released.
 *
 * @see file.h
 *
 * @code{.c}
 * ucb_error* err = UCB_NULL;
 * ucb_file* rd = UCB_NULL;
 * ucb_file* wr = UCB_NULL;
 *
 * if (!ucb_pipe_create(&rd, &wr, UCB_FILE_CLOEXEC, &err))
 *     return handle_error(err);
 *
 * const char msg[] = "hello";
 * if (!ucb_file_write_full(wr, msg, sizeof(msg) - 1, &err))
 *     goto fail;
 *
 * // Closing the write end signals end of input on the read end.
 * ucb_file_free(wr);
 * wr = UCB_NULL;
 *
 * char buf[64];
 * ucb_ssize n = ucb_file_read(rd, buf, sizeof(buf), &err);
 * if (n < 0)
 *     goto fail;
 *
 * // ... use buf[0..n) ...
 *
 * ucb_file_free(rd);
 * return true;
 *
 * fail:
 *     ucb_error_clear(&err);
 *     ucb_file_free(wr);
 *     ucb_file_free(rd);
 *     return false;
 * @endcode
 */

#ifndef UCB_PIPE_H
#define UCB_PIPE_H

#include <ucb/error.h>
#include <ucb/export.h>
#include <ucb/file.h>

#include <stdbool.h>

/**
 * @brief Create an anonymous pipe
 *
 * On success @p out_read and @p out_write receive two independent owning
 * handles for the read and write ends. The pipe is a byte stream: reads and
 * writes may transfer fewer bytes than requested.
 *
 * The following @p flags are honored and applied to both ends:
 * - @ref UCB_FILE_NONBLOCK, where the platform supports it
 * - @ref UCB_FILE_CLOEXEC to hide both ends from child processes
 * - @ref UCB_FILE_INHERIT to make both ends inheritable by child processes
 *   (default on Windows is non-inheritable)
 * - @ref UCB_FILE_BINARY (accepted as a no-op)
 *
 * On Windows anonymous pipes are always blocking and cannot report writability
 * through @ref ucb_file_wait_writable; that call throws
 * @ref UCB_ERROR_NOT_IMPLEMENTED. @ref ucb_file_wait_readable works.
 *
 * On failure both output parameters are left as UCB_NULL.
 *
 * @param out_read destination for the read end, must be non-NULL
 * @param out_write destination for the write end, must be non-NULL
 * @param flags bitwise OR of relevant @ref ucb_file_open_flags
 * @param perr optional location to store the error on failure
 * @return true on success
 */
UCB_API bool ucb_pipe_create(ucb_file** out_read,
                             ucb_file** out_write,
                             unsigned flags,
                             ucb_error** perr);

#endif // UCB_PIPE_H
