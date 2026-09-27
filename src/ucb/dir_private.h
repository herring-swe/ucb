/**
 * @file dir_private.h
 *
 * This file is part of the UCB project
 * - SPDX-FileCopyrightText: © 2026 Åke Svedin <ake@svedin.org>
 * - SPDX-License-Identifier: MIT
 *
 * @brief Private directory iterator layout and platform backend interface
 */

#ifndef UCB_DIR_PRIVATE_H
#define UCB_DIR_PRIVATE_H

#include <ucb/dir.h>
#include <ucb/error.h>

#include <stdbool.h>

/**
 * @brief Concrete, opaque @ref ucb_dir layout
 *
 * The platform state is kept behind @c state so this header stays free of
 * platform system headers. @c name is an owned UTF-8 buffer reused for the
 * current entry name on Windows; on POSIX the name points into the @c dirent.
 */
struct ucb_dir
{
    char* path;     ///< Owned canonical directory path
    unsigned flags; ///< Bitwise OR of ucb_dir_flags
    bool closed;    ///< True once the OS resource has been released
    void* state;    ///< Owned platform iterator state
    char* name;     ///< Owned entry-name buffer (Windows), or UCB_NULL
};

bool ucb_dir_plat_open(ucb_dir* dir, ucb_error** perr);
bool ucb_dir_plat_next(ucb_dir* dir, ucb_dir_entry* out, ucb_error** perr);
bool ucb_dir_plat_close(ucb_dir* dir, ucb_error** perr);

#endif // UCB_DIR_PRIVATE_H
