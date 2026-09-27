/**
 * @file dir.h
 *
 * This file is part of the UCB project
 * - SPDX-FileCopyrightText: © 2026 Åke Svedin <ake@svedin.org>
 * - SPDX-License-Identifier: MIT
 *
 * @brief Directory iteration
 *
 * @ref ucb_dir is an opaque, heap allocated directory iterator that sits beside
 * @ref fs.h. It reuses the same path rules and entry kinds, and borrows each
 * entry name until the next call to @ref ucb_dir_next or a close.
 *
 * Directory iteration order is defined by the operating system and must never
 * be relied upon.
 *
 * @code{.c}
 * ucb_error* err = UCB_NULL;
 * ucb_dir* dir = ucb_dir_open("/tmp", 0, &err);
 * if (!dir)
 *     return handle_error(err);
 *
 * ucb_dir_entry entry;
 * while (ucb_dir_next(dir, &entry, &err))
 * {
 *     // entry.name is valid until the next call
 * }
 * ucb_dir_free(dir);
 * @endcode
 */

#ifndef UCB_DIR_H
#define UCB_DIR_H

#include <ucb/container/impl/vector_str.h>
#include <ucb/error.h>
#include <ucb/export.h>
#include <ucb/fs.h>

#include <stdbool.h>

/**
 * @struct ucb_dir
 * @brief Opaque directory iterator
 *
 * Created with @ref ucb_dir_open and released with @ref ucb_dir_free. Closing
 * and freeing are separate so a close failure can be observed; @ref ucb_dir_free
 * ignores a close failure.
 */
typedef struct ucb_dir ucb_dir;

/**
 * @brief Flags controlling directory iteration
 */
typedef enum ucb_dir_flags
{
    UCB_DIR_INCLUDE_DOT = 1u << 0, ///< Also report "." and ".."
} ucb_dir_flags;

/**
 * @struct ucb_dir_entry
 * @brief A single directory entry
 */
typedef struct ucb_dir_entry
{
    const char* name; ///< Borrowed name, valid until the next next/close call
    ucb_fs_kind kind; ///< Best-effort kind from the directory metadata
} ucb_dir_entry;

/**
 * @brief Open a directory for iteration
 *
 * The path is canonicalized exactly like @ref fs.h inputs. @c "." and ".." are
 * skipped unless @ref UCB_DIR_INCLUDE_DOT is set.
 *
 * @param path UTF-8 path, must be non-NULL and non-empty
 * @param flags 0 or @ref UCB_DIR_INCLUDE_DOT
 * @param perr optional location to store the error on failure
 * @return a new iterator, or UCB_NULL on failure
 */
UCB_API ucb_dir* ucb_dir_open(const char* path, unsigned flags, ucb_error** perr);

/**
 * @brief Advance to the next directory entry
 *
 * On success @p out is filled and its @c name points to a borrowed buffer that
 * is valid until the next call to this function or until the iterator is closed
 * or freed.
 *
 * Returns @c false both at the end of the directory and on error; an error is
 * signalled by an error thrown into @p perr. Use @ref UCB_IS_THROWN on a local
 * error pointer to distinguish the two.
 *
 * @param dir the iterator, must be non-NULL
 * @param out destination for the entry, must be non-NULL
 * @param perr optional location to store the error on failure
 * @return true if an entry was produced, false at end or on error
 */
UCB_API bool ucb_dir_next(ucb_dir* dir, ucb_dir_entry* out, ucb_error** perr);

/**
 * @brief Close the iterator's OS resource
 *
 * Idempotent. The @ref ucb_dir object remains allocated and can be reused only
 * after being freed.
 *
 * @param dir the iterator, may be UCB_NULL
 * @param perr optional location to store the error on failure
 * @return true on success
 */
UCB_API bool ucb_dir_close(ucb_dir* dir, ucb_error** perr);

/**
 * @brief Close (if needed) and free an iterator
 *
 * Errors from the implicit close are ignored. @p dir may be UCB_NULL.
 *
 * @param dir the iterator to free, may be UCB_NULL
 */
UCB_API void ucb_dir_free(ucb_dir* dir);

/**
 * @brief List the names in a directory
 *
 * Convenience wrapper around the iterator that collects every name into a new
 * vector of owned @ref ucb_str values. @c "." and ".." are not included. Free
 * the result with @ref ucb_vector_str_free_full.
 *
 * @param path UTF-8 path, must be non-NULL and non-empty
 * @param out destination for the vector, must be non-NULL
 * @param perr optional location to store the error on failure
 * @return true on success
 */
UCB_API bool ucb_dir_list(const char* path, ucb_vector_str** out, ucb_error** perr);

#endif // UCB_DIR_H
