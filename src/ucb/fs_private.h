/**
 * @file fs_private.h
 *
 * This file is part of the UCB project
 * - SPDX-FileCopyrightText: © 2026 Åke Svedin <ake@svedin.org>
 * - SPDX-License-Identifier: MIT
 *
 * @brief Private filesystem helpers and platform backend interface
 */

#ifndef UCB_FS_PRIVATE_H
#define UCB_FS_PRIVATE_H

#include <ucb/error.h>
#include <ucb/fs.h>
#include <ucb/fs_ex.h>

#include <stdbool.h>

/* -------------------------------------------------------------------------- */
/*                              Shared helpers                                */
/* -------------------------------------------------------------------------- */

/**
 * @brief Lexically canonicalize a path.
 *
 * Collapses duplicate separators and strips a trailing separator unless the
 * value is a root. @c . and @c .. are left untouched. The result is handed
 * directly to the operating system.
 *
 * @param path UTF-8 path, may be UCB_NULL
 * @return an owned string to free with ucb_free, or UCB_NULL
 */
char* ucb_fs_canon(const char* path);

/**
 * @brief Get the lexically canonical parent of a canonical path.
 *
 * A root is its own parent, so the caller must compare the result with the input
 * to detect the top. Also used by @ref ucb_fs_mkdir_p.
 *
 * @param canon a canonical path
 * @return an owned string to free with ucb_free, or UCB_NULL
 */
char* ucb_fs_parent(const char* canon);

/**
 * @brief Join a canonical directory and a single entry name.
 *
 * Inserts the native separator unless @p dir is empty or already ends with one.
 *
 * @param dir canonical directory path
 * @param name entry name
 * @return an owned string to free with ucb_free, or UCB_NULL
 */
char* ucb_fs_join(const char* dir, const char* name);

/**
 * @brief Check whether a canonical path is a filesystem root.
 *
 * Recognizes a POSIX @c /, a Windows drive root (@c C:\\) and a UNC share root
 * (@c \\\\server\\share). Used by @ref ucb_fs_remove_all as a safety guard.
 *
 * @param canon a canonical path
 * @return true if @p canon is a root
 */
bool ucb_fs_is_root(const char* canon);

/* -------------------------------------------------------------------------- */
/*                          Platform backend interface                        */
/* -------------------------------------------------------------------------- */

bool ucb_fs_plat_kind(const char* path, unsigned flags, ucb_fs_kind* out, ucb_error** perr);
bool ucb_fs_plat_stat(const char* path, unsigned flags, ucb_fs_stat* out, ucb_error** perr);
bool ucb_fs_plat_stat_ns(const char* path, unsigned flags, ucb_fs_stat_ns* out, ucb_error** perr);
bool ucb_fs_plat_mkdir(const char* path, ucb_error** perr);
bool ucb_fs_plat_remove_file(const char* path, ucb_error** perr);
bool ucb_fs_plat_remove_dir(const char* path, ucb_error** perr);
bool ucb_fs_plat_rename(const char* src, const char* dst, unsigned flags, ucb_error** perr);
bool ucb_fs_plat_set_mode(const char* path, unsigned mode, ucb_error** perr);
bool ucb_fs_plat_set_mtime(const char* path, ucb_time mtime, ucb_error** perr);
bool ucb_fs_plat_read_link(const char* path, char** out, ucb_error** perr);
bool ucb_fs_plat_create_link(const char* target,
                             const char* linkpath,
                             bool target_is_dir,
                             ucb_error** perr);
ucb_str* ucb_fs_plat_cwd(void);
bool ucb_fs_plat_chdir(const char* path, ucb_error** perr);

#endif // UCB_FS_PRIVATE_H
