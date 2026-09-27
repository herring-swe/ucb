/**
 * @file process_private.h
 *
 * This file is part of the UCB project
 * - SPDX-FileCopyrightText: © 2026 Åke Svedin <ake@svedin.org>
 * - SPDX-License-Identifier: MIT
 *
 * @brief Private process handle layout and platform backend interface
 */

#ifndef UCB_PROCESS_PRIVATE_H
#define UCB_PROCESS_PRIVATE_H

#include <ucb/process.h>

#include <stdbool.h>

/**
 * @brief Concrete, opaque @ref ucb_process layout
 *
 * On Windows the native process @c HANDLE is kept until the process has been
 * reaped, then closed. On POSIX the pid doubles as the @c pid_t used by
 * @c waitpid.
 */
#ifdef _WIN32
struct ucb_process
{
    ucb_pid pid;   ///< Windows process id
    bool reaped;   ///< True once the exit code has been collected
    int exit_code; ///< Raw process exit code
    void* handle;  ///< Win32 process HANDLE, or UCB_NULL
};
#else
struct ucb_process
{
    ucb_pid pid;   ///< POSIX pid_t of the child
    bool reaped;   ///< True once the child has been reaped
    int exit_code; ///< Exit code, or 128 + signal
};
#endif

/* -------------------------------------------------------------------------- */
/*                          Platform backend interface                        */
/* -------------------------------------------------------------------------- */

/**
 * @brief Launch a child process from validated options
 *
 * The shared entry point has already verified that @c opts, @c argv and
 * @c argv[0] are present and that redirection handles are valid, so the backend
 * can assume a consistent contract.
 */
ucb_process* ucb_process_plat_spawn(const ucb_process_opts* opts, ucb_error** perr);

/**
 * @brief Wait for the child, possibly with a timeout
 *
 * @return true on success whether the child exited or the wait timed out, false
 *         only on error. @p out_exited is set to true when the child exited and
 *         its code was stored in the handle.
 */
bool ucb_process_plat_wait(ucb_process* proc, int timeout_ms, bool* out_exited, ucb_error** perr);

/**
 * @brief Signal the child to terminate
 * @param force true to kill without a graceful request
 */
bool ucb_process_plat_terminate(ucb_process* proc, bool force, ucb_error** perr);

/**
 * @brief Release platform resources without freeing the struct
 *
 * On POSIX this clears an already exited child with a non-blocking
 * @c waitpid; on Windows it closes the process handle.
 */
void ucb_process_plat_free(ucb_process* proc);

#endif // UCB_PROCESS_PRIVATE_H
