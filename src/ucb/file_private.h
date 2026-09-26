/**
 * @file file_private.h
 *
 * This file is part of the UCB project
 * - SPDX-FileCopyrightText: © 2026 Åke Svedin <ake@svedin.org>
 * - SPDX-License-Identifier: MIT
 *
 * @brief Private file handle layout and platform backend interface
 */

#ifndef UCB_FILE_PRIVATE_H
#define UCB_FILE_PRIVATE_H

#include <ucb/file.h>

#include <stdbool.h>
#include <stdint.h>

/**
 * @brief Concrete, opaque @ref ucb_file layout
 *
 * The platform backing differs: POSIX stores a file descriptor, Windows stores
 * a @c HANDLE plus, when the handle originated from a C runtime descriptor, the
 * original descriptor for @ref ucb_file_get_fd.
 */
struct ucb_file
{
    ucb_file_kind kind; ///< Detected resource kind
    unsigned caps;      ///< Bitwise OR of ucb_file_caps
    unsigned flags;     ///< Open flags as normalized by the backend
    bool own;           ///< True if close/free releases the OS resource
    bool is_static;     ///< True for the process-wide standard stream singletons
#ifdef _WIN32
    void* handle; ///< Windows HANDLE, or UCB_NULL
    int crt_fd;   ///< CRT descriptor if known, otherwise -1
#else
    int fd; ///< POSIX file descriptor, or -1
#endif
};

/* -------------------------------------------------------------------------- */
/*                          Platform backend interface                        */
/* -------------------------------------------------------------------------- */

ucb_file* ucb_file_plat_open(const char* path, unsigned flags, unsigned mode, ucb_error** perr);
bool ucb_file_plat_close(ucb_file* file, ucb_error** perr);
ucb_ssize ucb_file_plat_read(ucb_file* file, void* buf, size_t size, ucb_error** perr);
ucb_ssize ucb_file_plat_write(ucb_file* file, const void* buf, size_t size, ucb_error** perr);
bool ucb_file_plat_seek(ucb_file* file, int64_t offset, ucb_file_whence whence, ucb_error** perr);
bool ucb_file_plat_tell(const ucb_file* file, int64_t* out_pos, ucb_error** perr);
bool ucb_file_plat_sync(ucb_file* file, ucb_error** perr);
ucb_file* ucb_file_plat_dup(ucb_file* file, ucb_error** perr);
bool ucb_file_plat_set_inherit(ucb_file* file, bool inherit, ucb_error** perr);
ucb_file* ucb_file_plat_from_fd(int fd, bool own, ucb_error** perr);
int ucb_file_plat_get_fd(const ucb_file* file);
bool ucb_file_plat_wait(ucb_file* file, bool writable, int timeout_ms, ucb_error** perr);
bool ucb_pipe_plat_create(ucb_file** out_read,
                          ucb_file** out_write,
                          unsigned flags,
                          ucb_error** perr);

/**
 * @brief Check whether the platform resource is still present
 *
 * Unlike @ref ucb_file_is_valid this ignores the detected kind, so an owned but
 * unclassified handle can still be closed and freed.
 */
bool ucb_file_plat_has_resource(const ucb_file* file);

/**
 * @brief Build a non-owning standard stream singleton
 * @param std_id 0 for stdin, 1 for stdout, 2 for stderr
 * @return a static singleton, never UCB_NULL
 */
ucb_file* ucb_file_plat_std(unsigned std_id);

#ifdef _WIN32
ucb_file* ucb_file_plat_from_handle(void* handle, bool own, ucb_error** perr);
void* ucb_file_plat_get_handle(const ucb_file* file);
#endif

/* -------------------------------------------------------------------------- */
/*                              Shared helpers                                */
/* -------------------------------------------------------------------------- */

/**
 * @brief Allocate a zeroed handle with an invalid backend
 *
 * The caller must fill the platform fields and call @ref ucb_file_set_io_caps.
 */
ucb_file* ucb_file_alloc(void);

/**
 * @brief Set the read/write capabilities from the stored open flags
 */
void ucb_file_set_io_caps(ucb_file* file);

/**
 * @brief Derive the seek and poll capabilities from the detected kind
 *
 * Shared by both backends so the kind to capability mapping cannot drift.
 * Must be called after @ref ucb_file::kind has been set.
 */
void ucb_file_apply_kind_caps(ucb_file* file);

#endif // UCB_FILE_PRIVATE_H
