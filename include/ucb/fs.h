/**
 * @file fs.h
 *
 * This file is part of the UCB project
 * - SPDX-FileCopyrightText: © 2026 Åke Svedin <ake@svedin.org>
 * - SPDX-License-Identifier: MIT
 *
 * @brief Portable filesystem query and modification API
 *
 * The filesystem API is a string based, filesystem aware companion to @ref path.h
 * and @ref file.h. All path-like inputs are UTF-8 encoded @c const char* values,
 * consistent with @ref file.h, @ref env.h and @ref pipe.h. @ref ucb_path stays
 * lexical-only and is not required to use this module, although callers may pass
 * a @ref ucb_path through @ref UCB_CSTR when convenient.
 *
 * @section fs_path_semantics Path semantics
 *
 * Every input path is canonicalized **lexically** before it reaches the operating
 * system: repeated separators are collapsed and a trailing separator is stripped,
 * except for a filesystem root. A path that contains a @c .. component is
 * additionally normalized lexically, resolving @c .. without crossing the
 * anchor. This is required for safety: leaving @c .. unresolved would let a
 * guard observe one path while the operating system acts on another (for
 * example @c "/tmp/.." names the root). A @c . component with no @c .. is left
 * as written. As a consequence the byte string passed in is not echoed back
 * verbatim.
 *
 * A trailing separator therefore carries no meaning:
 * - @c "/my/path/" and @c "/my/path" name the same entry.
 * - A destination is always the **exact** result path. There is no @c "into" or
 *   @c "contents-only" interpretation: @c ucb_fs_copy_tree("a", "b/c") makes
 *   @c b/c a copy of @c a. To copy into an existing directory, compose the final
 *   name first, for example with @ref ucb_path_join_c.
 *
 * An empty path string is invalid for every operation and throws
 * @ref UCB_ERROR_INVALID_ARG.
 *
 * @section fs_symlink_policy Symbolic link policy
 *
 * - @ref ucb_fs_exists, @ref ucb_fs_is_file and @ref ucb_fs_is_dir follow a
 *   final symbolic link; @ref ucb_fs_is_symlink tests the entry itself.
 * - @ref ucb_fs_get_kind and @ref ucb_fs_get_stat follow a final link by default. Pass
 *   @ref UCB_FS_NOFOLLOW to classify the link itself.
 * - Intermediate links are always resolved by the operating system; the follow
 *   flag only affects the final path component.
 *
 * @section fs_threads Thread safety
 *
 * These functions are not thread-safe with respect to each other when they
 * mutate a shared directory tree. @ref ucb_fs_chdir is process-global and must
 * be treated as such.
 */

#ifndef UCB_FS_H
#define UCB_FS_H

#include <ucb/defines.h>
#include <ucb/error.h>
#include <ucb/export.h>
#include <ucb/string.h>
#include <ucb/time.h>
#include <ucb/types.h>

#include <stdbool.h>
#include <stdint.h>

/**
 * @brief The kind of filesystem entry a path names
 *
 * This enumeration is intentionally separate from @ref ucb_file_kind. A
 * @ref ucb_file_kind describes an open handle, which always resolves to the
 * target of a link, while @ref ucb_fs_kind can describe a link itself:
 * - @ref UCB_FS_KIND_FILE maps to @ref UCB_FILE_KIND_REGULAR.
 * - @ref UCB_FS_KIND_DIR and @ref UCB_FS_KIND_SYMLINK have no handle equivalent.
 * - @ref UCB_FS_KIND_OTHER covers what a path can name but a handle distinguishes:
 *   @ref UCB_FILE_KIND_PIPE, @ref UCB_FILE_KIND_CONSOLE, @ref UCB_FILE_KIND_DEVICE
 *   or an unrecognized entry.
 */
typedef enum ucb_fs_kind
{
    UCB_FS_KIND_UNKNOWN = 0, ///< No metadata or an unrecognized entry
    UCB_FS_KIND_FILE,        ///< Regular file
    UCB_FS_KIND_DIR,         ///< Directory
    UCB_FS_KIND_SYMLINK,     ///< Symbolic link (Win32 reparse point)
    UCB_FS_KIND_OTHER,       ///< FIFO, socket, device or anything unmapped
} ucb_fs_kind;

/**
 * @brief Flags shared by filesystem query and modification functions
 *
 * Not every function honors every flag; see the documentation of each function.
 */
typedef enum ucb_fs_flags
{
    UCB_FS_NOFOLLOW = 1u << 0,  ///< Query: classify the final link itself
    UCB_FS_OVERWRITE = 1u << 1, ///< Copy/move: allow an existing destination
    UCB_FS_FOLLOW = 1u << 2,    ///< Copy: resolve links instead of recreating them
} ucb_fs_flags;

/**
 * @struct ucb_fs_stat
 * @brief Metadata for a filesystem entry
 *
 * Filled by @ref ucb_fs_get_stat. Timestamps are whole seconds; use
 * @ref ucb_fs_get_stat_ns in @ref fs_ex.h when sub-second precision is needed.
 */
typedef struct ucb_fs_stat
{
    ucb_fs_kind kind; ///< Entry kind (affected by @ref UCB_FS_NOFOLLOW)
    uint64_t size;    ///< Size in bytes for a regular file, 0 otherwise
    ucb_time mtime;   ///< Last modification time in seconds
    ucb_time atime;   ///< Last access time in seconds
    ucb_time ctime;   ///< Last status change time in seconds
    unsigned mode;    ///< POSIX permission bits, 0 on Windows
    bool is_readonly; ///< True if the entry is marked read-only
} ucb_fs_stat;

/* -------------------------------------------------------------------------- */
/*                             Simple predicates                              */
/* -------------------------------------------------------------------------- */

/**
 * @brief Check whether a path names an existing entry
 *
 * A final symbolic link is followed. The path is verified with
 * @ref UCB_VERIFY_ARGS, so a @c UCB_NULL path is misuse and aborts. Beyond
 * that these four predicates are trivial: any failure, including a permission
 * error, resolves to @c false, so they cannot distinguish "missing" from
 * "unreadable". Use @ref ucb_fs_get_kind or @ref ucb_fs_get_stat when that
 * distinction matters.
 *
 * @param path UTF-8 path, must not be UCB_NULL (an empty path returns false)
 * @return true if the entry exists
 */
UCB_API bool ucb_fs_exists(const char* path);

/**
 * @brief Check whether a path names a regular file
 *
 * Follows a final symbolic link. See @ref ucb_fs_exists for the validation and
 * trivial-error contract.
 *
 * @param path UTF-8 path, must not be UCB_NULL (an empty path returns false)
 * @return true if the entry is a regular file
 */
UCB_API bool ucb_fs_is_file(const char* path);

/**
 * @brief Check whether a path names a directory
 *
 * Follows a final symbolic link. See @ref ucb_fs_exists for the validation and
 * trivial-error contract.
 *
 * @param path UTF-8 path, must not be UCB_NULL (an empty path returns false)
 * @return true if the entry is a directory
 */
UCB_API bool ucb_fs_is_dir(const char* path);

/**
 * @brief Check whether a path names a symbolic link
 *
 * Tests the entry itself rather than its target, so a broken link returns true
 * and @ref ucb_fs_exists returns false for the same path. On Windows a symbolic
 * link and a junction (both reparse points) are reported as a link. See
 * @ref ucb_fs_exists for the validation and trivial-error contract.
 *
 * @param path UTF-8 path, must not be UCB_NULL (an empty path returns false)
 * @return true if the entry is a symbolic link
 */
UCB_API bool ucb_fs_is_symlink(const char* path);

/* -------------------------------------------------------------------------- */
/*                              Error-aware query                             */
/* -------------------------------------------------------------------------- */

/**
 * @brief Classify a path
 *
 * Unlike the simple predicates this reports errors: an empty path or a missing
 * entry throws into @p perr. Pass @ref UCB_FS_NOFOLLOW to classify a final
 * symbolic link itself.
 *
 * @param path UTF-8 path, must be non-NULL and non-empty
 * @param flags 0 or @ref UCB_FS_NOFOLLOW
 * @param out destination for the kind, must be non-NULL
 * @param perr optional location to store the error on failure
 * @return true on success
 */
UCB_API bool ucb_fs_get_kind(const char* path, unsigned flags, ucb_fs_kind* out, ucb_error** perr);

/**
 * @brief Query metadata for a path
 *
 * Follows a final symbolic link unless @ref UCB_FS_NOFOLLOW is set. A missing
 * entry throws @ref UCB_ERRSYS_ENOENT.
 *
 * @param path UTF-8 path, must be non-NULL and non-empty
 * @param flags 0 or @ref UCB_FS_NOFOLLOW
 * @param out destination for the metadata, must be non-NULL
 * @param perr optional location to store the error on failure
 * @return true on success
 */
UCB_API bool ucb_fs_get_stat(const char* path, unsigned flags, ucb_fs_stat* out, ucb_error** perr);

/**
 * @brief Check whether a kind denotes a regular file
 * @param kind the kind to test
 * @return true if @p kind is @ref UCB_FS_KIND_FILE
 */
static inline bool ucb_fs_kind_is_file(ucb_fs_kind kind)
{
    return kind == UCB_FS_KIND_FILE;
}

/**
 * @brief Check whether a kind denotes a directory
 * @param kind the kind to test
 * @return true if @p kind is @ref UCB_FS_KIND_DIR
 */
static inline bool ucb_fs_kind_is_dir(ucb_fs_kind kind)
{
    return kind == UCB_FS_KIND_DIR;
}

/* -------------------------------------------------------------------------- */
/*                                Modification                                */
/* -------------------------------------------------------------------------- */

/**
 * @brief Create a single directory
 *
 * No parent directories are created. An existing entry throws
 * @ref UCB_ERRSYS_EEXIST (even when it is a directory); use
 * @ref ucb_fs_mkdir_p to tolerate an existing directory.
 *
 * @param path UTF-8 path, must be non-NULL and non-empty
 * @param perr optional location to store the error on failure
 * @return true on success
 */
UCB_API bool ucb_fs_mkdir(const char* path, ucb_error** perr);

/**
 * @brief Create a directory and any missing parents
 *
 * Idempotent: an existing directory is a success. An existing non-directory
 * component throws @ref UCB_ERRSYS_ENOTDIR.
 *
 * @param path UTF-8 path, must be non-NULL and non-empty
 * @param perr optional location to store the error on failure
 * @return true on success
 */
UCB_API bool ucb_fs_mkdir_p(const char* path, ucb_error** perr);

/**
 * @brief Remove a file, symbolic link or empty directory
 *
 * A final symbolic link is removed without touching its target. A missing path
 * throws @ref UCB_ERRSYS_ENOENT and a non-empty directory throws
 * @ref UCB_ERRSYS_ENOTEMPTY. Use @ref ucb_fs_remove_all for a recursive,
 * idempotent removal.
 *
 * @param path UTF-8 path, must be non-NULL and non-empty
 * @param perr optional location to store the error on failure
 * @return true on success
 */
UCB_API bool ucb_fs_remove(const char* path, ucb_error** perr);

/**
 * @brief Remove a path recursively
 *
 * Idempotent: a missing path is a successful no-op. A filesystem root is
 * refused with @ref UCB_ERROR_INVALID_ARG, so a stray or root path cannot erase
 * a volume. Because a @c .. component is resolved during canonicalization, a
 * path such as @c "/tmp/.." is recognized as a root and refused too. Symbolic
 * links are removed without being followed.
 *
 * @param path UTF-8 path, must be non-NULL and non-empty
 * @param perr optional location to store the error on failure
 * @return true on success
 */
UCB_API bool ucb_fs_remove_all(const char* path, ucb_error** perr);

/**
 * @brief Copy a single file
 *
 * The destination is created when missing. An existing destination fails with
 * @ref UCB_ERRSYS_EEXIST unless @ref UCB_FS_OVERWRITE is set; a destination
 * that is a directory always throws @ref UCB_ERRSYS_EISDIR, even with
 * @ref UCB_FS_OVERWRITE.
 *
 * With @ref UCB_FS_OVERWRITE the new contents are staged in a temporary sibling
 * and renamed over the destination, so a failed copy leaves the original intact
 * and copying a path onto itself is safe.
 *
 * By default a symbolic link @p src is recreated as a link at @p dst instead of
 * being traversed. With @ref UCB_FS_FOLLOW the link target is copied. On
 * success the POSIX mode (where available) and the modification time of the
 * source are applied to the destination.
 *
 * @param src UTF-8 source path, must be non-NULL and non-empty
 * @param dst UTF-8 destination path, must be non-NULL and non-empty
 * @param flags 0, @ref UCB_FS_OVERWRITE and/or @ref UCB_FS_FOLLOW
 * @param perr optional location to store the error on failure
 * @return true on success
 */
UCB_API bool ucb_fs_copy_file(const char* src, const char* dst, unsigned flags, ucb_error** perr);

/**
 * @brief Copy a directory tree
 *
 * @p src must resolve to a directory. The destination is created if missing,
 * including missing parents. An existing destination that is not a directory
 * throws @ref UCB_ERRSYS_ENOTDIR; an existing directory fails with
 * @ref UCB_ERRSYS_EEXIST unless @ref UCB_FS_OVERWRITE is set, in which case the
 * contents are merged into it. A destination equal to or inside @p src is
 * refused with @ref UCB_ERROR_INVALID_ARG so a copy cannot recurse into itself.
 * Symbolic links are recreated by default and followed only with
 * @ref UCB_FS_FOLLOW.
 *
 * @param src UTF-8 source path, must be non-NULL and non-empty
 * @param dst UTF-8 destination path, must be non-NULL and non-empty
 * @param flags 0, @ref UCB_FS_OVERWRITE and/or @ref UCB_FS_FOLLOW
 * @param perr optional location to store the error on failure
 * @return true on success
 */
UCB_API bool ucb_fs_copy_tree(const char* src, const char* dst, unsigned flags, ucb_error** perr);

/**
 * @brief Move or rename a path
 *
 * Attempts an atomic rename first. When the source and destination are on
 * different devices the rename fails and the move falls back to a copy followed
 * by a delete; only the same-device rename is atomic. If the copy succeeds but
 * the delete fails, the destination is retained, the move reports failure and
 * throws, and the operation is documented as non-atomic.
 *
 * A destination that is a directory always throws @ref UCB_ERRSYS_EEXIST; the
 * caller must remove it first. Moving a directory onto an existing file throws
 * @ref UCB_ERRSYS_ENOTDIR.
 *
 * @param src UTF-8 source path, must be non-NULL and non-empty
 * @param dst UTF-8 destination path, must be non-NULL and non-empty
 * @param flags 0 or @ref UCB_FS_OVERWRITE for an existing file destination
 * @param perr optional location to store the error on failure
 * @return true on success
 */
UCB_API bool ucb_fs_move(const char* src, const char* dst, unsigned flags, ucb_error** perr);

/* -------------------------------------------------------------------------- */
/*                             Working directory                              */
/* -------------------------------------------------------------------------- */

/**
 * @brief Get the current working directory
 *
 * @param perr optional location to store the error on failure
 * @return a new owned string, or UCB_NULL on failure
 */
UCB_API ucb_str* ucb_fs_cwd(ucb_error** perr);

/**
 * @brief Change the current working directory
 *
 * This is process-global and not thread-safe.
 *
 * @param path UTF-8 path, must be non-NULL and non-empty
 * @param perr optional location to store the error on failure
 * @return true on success
 */
UCB_API bool ucb_fs_chdir(const char* path, ucb_error** perr);

#endif // UCB_FS_H
