/**
 * @file fs_ex.h
 *
 * This file is part of the UCB project
 * - SPDX-FileCopyrightText: © 2026 Åke Svedin <ake@svedin.org>
 * - SPDX-License-Identifier: MIT
 *
 * @brief Extended, opt-in filesystem helpers
 *
 * This header follows the `_ex.h` convention: an optional companion to a main
 * header that holds additions which do not fit cleanly into a section of that
 * header, or extra functions that most users do not need. It is never included
 * by the main header; include it explicitly when needed.
 *
 * @see fs.h
 */

#ifndef UCB_FS_EX_H
#define UCB_FS_EX_H

#include <ucb/fs.h>

#include <stdbool.h>
#include <stdint.h>

/**
 * @struct ucb_fs_stat_ns
 * @brief Nanosecond timestamps for a filesystem entry
 *
 * Filled by @ref ucb_fs_get_stat_ns. Each field is a whole number of nanoseconds
 * since the Unix epoch, matching @c st_mtim.tv_nsec style platforms, extended to
 * a full 64-bit count so it is unambiguous on Windows.
 */
typedef struct ucb_fs_stat_ns
{
    int64_t mtime_ns; ///< Last modification time in nanoseconds
    int64_t atime_ns; ///< Last access time in nanoseconds
    int64_t ctime_ns; ///< Last status change time in nanoseconds
} ucb_fs_stat_ns;

/**
 * @brief Query nanosecond timestamps for a path
 *
 * Follows a final symbolic link unless @ref UCB_FS_NOFOLLOW is set. Only the
 * timestamps are returned; use @ref ucb_fs_get_stat for the remaining metadata.
 * A missing entry throws @ref UCB_ERRSYS_ENOENT.
 *
 * @param path UTF-8 path, must be non-NULL and non-empty
 * @param flags 0 or @ref UCB_FS_NOFOLLOW
 * @param out destination for the timestamps, must be non-NULL
 * @param perr optional location to store the error on failure
 * @return true on success
 */
UCB_API bool ucb_fs_get_stat_ns(const char* path,
                                unsigned flags,
                                ucb_fs_stat_ns* out,
                                ucb_error** perr);

#endif // UCB_FS_EX_H
