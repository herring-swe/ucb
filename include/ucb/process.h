/**
 * @file process.h
 * This file is part of the UCB project
 * - SPDX-FileCopyrightText: © 2026 Åke Svedin <ake@svedin.org>
 * - SPDX-License-Identifier: MIT
 *
 * @brief Process launching, waiting and I/O redirection
 *
 * @ref ucb_process is a heap allocated, opaque handle representing a child
 * process launched by @ref ucb_process_spawn. It unifies the common POSIX
 * @c fork/@c exec and Windows @c CreateProcess operations while remaining
 * portable.
 *
 * Design contract:
 * - All textual inputs (@c argv, environment, working directory) are UTF-8
 *   encoded @c const char* values, consistent with @ref file.h and @ref fs.h.
 *   A @ref ucb_path is composable into such a string with @ref ucb_path_cstr and
 *   @c UCB_CSTR.
 * - A handle is **not thread-safe**. Use it from one thread at a time. Sharing a
 *   handle between threads requires external synchronization.
 * - An @ref ucb_process_opts and @ref ucb_process_capture_opts value borrows all
 *   of its pointers. The borrowed data only needs to stay valid for the duration
 *   of the spawn call; the child receives copies.
 * - Redirection handles (@c in, @c out, @c err) are borrowed and must be valid
 *   @ref ucb_file handles during the spawn call. They do not need to remain valid
 *   afterwards.
 */

#ifndef UCB_PROCESS_H
#define UCB_PROCESS_H

#include <ucb/defines.h>
#include <ucb/envmap.h>
#include <ucb/error.h>
#include <ucb/export.h>
#include <ucb/file.h>
#include <ucb/types.h>

#include <stdbool.h>
#include <stddef.h>

struct ucb_buffer;
typedef struct ucb_buffer ucb_buffer;

/**
 * @struct ucb_process
 * @brief Opaque handle to a launched child process
 *
 * Created with @ref ucb_process_spawn (or @ref ucb_process_capture) and released
 * with @ref ucb_process_free.
 */
typedef struct ucb_process ucb_process;

/**
 * @brief Flags controlling how a child process is launched
 *
 * Unknown bits are reserved and ignored. Platform specific flags are documented
 * as no-ops where they do not apply.
 */
typedef enum ucb_process_flags
{
    /**
     * @brief Start the child in a new session/process group
     *
     * POSIX: @c setsid() is called in the child. Windows: @c DETACHED_PROCESS and
     * @c CREATE_NEW_PROCESS_GROUP are used. Combined with
     * @ref ucb_process_capture this flag is rejected.
     */
    UCB_PROCESS_DETACH = 1u << 0,
    /**
     * @brief Windows: inherit all inheritable handles
     *
     * By default only the three standard handles are inherited using a precise
     * @c PROC_THREAD_ATTRIBUTE_HANDLE_LIST. When this flag is set all
     * inheritable handles are inherited instead. POSIX: no-op.
     */
    UCB_PROCESS_INHERIT_HANDLES = 1u << 1,
    /**
     * @brief Windows: do not create a console window
     *
     * Maps to @c CREATE_NO_WINDOW. Ignored on other platforms.
     */
    UCB_PROCESS_NO_WINDOW = 1u << 2,
} ucb_process_flags;

/**
 * @brief Options for @ref ucb_process_spawn
 *
 * Initialize with @ref ucb_process_opts_make and set the fields that apply. All
 * pointer fields are borrowed and only used during the spawn call.
 */
typedef struct ucb_process_opts
{
    /**
     * @brief Program and arguments, required
     *
     * A NULL-terminated array of UTF-8 strings where @c argv[0] is the program.
     * The array and every string must be non-NULL and remain valid for the
     * duration of the spawn call.
     */
    const char* const* argv;
    /**
     * @brief Environment, or UCB_NULL to inherit the parent environment
     *
     * When non-NULL the map is used as the exact replacement for the child's
     * environment. Merging with the parent environment is the caller's
     * responsibility, for example with @ref ucb_envmap_init_from_current.
     */
    const ucb_envmap* env;
    /** @brief Working directory, or UCB_NULL to inherit the parent directory */
    const char* cwd;
    /** @brief Redirected standard input, or UCB_NULL to inherit */
    ucb_file* in;
    /** @brief Redirected standard output, or UCB_NULL to inherit */
    ucb_file* out;
    /** @brief Redirected standard error, or UCB_NULL to inherit */
    ucb_file* err;
    /** @brief Bitwise OR of @ref ucb_process_flags */
    unsigned flags;
} ucb_process_opts;

/**
 * @brief Create a zero initialized @ref ucb_process_opts
 *
 * All pointers are UCB_NULL and @c flags is @c 0, which inherits the parent
 * environment, working directory and standard streams.
 *
 * @return a zero initialized options value
 */
static inline ucb_process_opts ucb_process_opts_make(void)
{
    ucb_process_opts opts = {0};
    return opts;
}

/**
 * @brief Options for @ref ucb_process_capture
 *
 * Initialize with @ref ucb_process_capture_opts_make. Standard input is closed
 * (redirected to the null device) and standard output/error are captured.
 */
typedef struct ucb_process_capture_opts
{
    /** @brief Program and arguments, required; same contract as @ref ucb_process_opts::argv */
    const char* const* argv;
    /** @brief Environment, or UCB_NULL to inherit */
    const ucb_envmap* env;
    /** @brief Working directory, or UCB_NULL to inherit */
    const char* cwd;
    /** @brief Bitwise OR of @ref ucb_process_flags; @ref UCB_PROCESS_DETACH is rejected */
    unsigned flags;
} ucb_process_capture_opts;

/**
 * @brief Create a zero initialized @ref ucb_process_capture_opts
 * @return a zero initialized options value
 */
static inline ucb_process_capture_opts ucb_process_capture_opts_make(void)
{
    ucb_process_capture_opts opts = {0};
    return opts;
}

/* -------------------------------------------------------------------------- */
/*                                  Lifetime                                  */
/* -------------------------------------------------------------------------- */

/**
 * @brief Launch a child process
 *
 * Builds the platform launch from @p opts. The call is synchronous with respect
 * to a failed @c exec: on POSIX a status pipe reports @c exec failures before
 * this function returns, so a returned handle refers to a successfully started
 * child (which may already have exited).
 *
 * @p opts, @p opts->argv and @p opts->argv[0] must be non-NULL. A @p cwd that is
 * set must not be empty. A redirection handle that is set must be a valid open
 * @ref ucb_file, otherwise @ref UCB_ERROR_INVALID_STATE is thrown.
 *
 * When @p opts->env is UCB_NULL the child inherits the parent environment. On
 * POSIX the @c exec search then follows the parent @c PATH; when a custom
 * environment is given, @c argv[0] is resolved against the @c PATH found in that
 * environment (falling back to the parent @c PATH) before launching.
 *
 * @param opts launch options, must be non-NULL
 * @param perr optional location to store the error on failure
 * @return a new owning handle, or UCB_NULL on failure
 */
UCB_API ucb_process* ucb_process_spawn(const ucb_process_opts* opts, ucb_error** perr);

/**
 * @brief Wait for a child process to exit
 *
 * @p timeout_ms follows the usual convention: negative waits indefinitely, @c 0
 * polls once, and a positive value waits up to that many milliseconds. POSIX
 * implements the timeout with a @c waitpid(WNOHANG) poll loop, so the timeout is
 * approximate.
 *
 * On timeout the function returns false with no error thrown, so callers
 * distinguish a timeout from a failure with @ref UCB_IS_THROWN on their local
 * error pointer. Waiting on an already reaped process returns true immediately
 * with the cached exit code.
 *
 * The exit code follows the platform convention: on POSIX it is
 * @c WEXITSTATUS(status), or @c 128 + signal when the child was signaled; on
 * Windows it is the raw process exit code.
 *
 * @param proc the process, must be non-NULL
 * @param timeout_ms timeout in milliseconds, negative for infinite
 * @param out_exit_code optional location for the exit code on success
 * @param perr optional location to store the error on failure
 * @return true if the process exited, false on timeout or error
 */
UCB_API bool ucb_process_wait(ucb_process* proc,
                              int timeout_ms,
                              int* out_exit_code,
                              ucb_error** perr);

/**
 * @brief Request a graceful termination of a child process
 *
 * POSIX sends @c SIGTERM; Windows calls @c TerminateProcess (both
 * @ref ucb_process_terminate and @ref ucb_process_kill map to
 * @c TerminateProcess there, so the distinction is documentation only).
 *
 * A process that was already reaped is a successful no-op. A POSIX child that
 * has already exited treats @c ESRCH as success. Termination does not wait; use
 * @ref ucb_process_wait afterwards to collect the exit code.
 *
 * @param proc the process, must be non-NULL
 * @param perr optional location to store the error on failure
 * @return true on success
 * @see ucb_process_kill
 */
UCB_API bool ucb_process_terminate(ucb_process* proc, ucb_error** perr);

/**
 * @brief Forcefully kill a child process
 *
 * POSIX sends @c SIGKILL; Windows calls @c TerminateProcess.
 *
 * @param proc the process, must be non-NULL
 * @param perr optional location to store the error on failure
 * @return true on success
 * @see ucb_process_terminate
 */
UCB_API bool ucb_process_kill(ucb_process* proc, ucb_error** perr);

/**
 * @brief Get the platform process identifier
 * @param proc the process, may be UCB_NULL
 * @return the pid, or @ref UCB_PID_INVALID if @p proc is UCB_NULL
 */
UCB_API ucb_pid ucb_process_get_pid(const ucb_process* proc);

/**
 * @brief Release a process handle
 *
 * Never waits indefinitely. On POSIX a non-blocking @c waitpid(WNOHANG) is
 * attempted to clear an already exited child; a still running child is left as
 * an orphan. On Windows the process handle is closed, which does not terminate
 * the child. @p proc may be UCB_NULL.
 *
 * @param proc the process to free, may be UCB_NULL
 */
UCB_API void ucb_process_free(ucb_process* proc);

/* -------------------------------------------------------------------------- */
/*                                  Capture                                   */
/* -------------------------------------------------------------------------- */

/**
 * @brief Run a child and capture its standard output and/or error
 *
 * Standard input is redirected to the null device so the child cannot block on
 * or read from the parent console. For each of @p out and @p err that is
 * non-NULL a pipe is created and the child writes into it; the captured bytes are
 * appended to the provided buffer. Both streams are drained concurrently so a
 * child that fills one pipe cannot deadlock.
 *
 * @p out and @p err may each be UCB_NULL to discard that stream; both may be
 * UCB_NULL to only collect the exit code. They must be distinct buffers when
 * both are provided, since the two streams are drained concurrently.
 * @ref UCB_PROCESS_DETACH is rejected
 * with @ref UCB_ERROR_INVALID_ARG because a detached child cannot be reliably
 * reaped.
 *
 * On failure the caller buffers may contain partial data. The child is always
 * reaped before returning.
 *
 * @param opts capture options, must be non-NULL
 * @param out buffer for standard output, or UCB_NULL to discard
 * @param err buffer for standard error, or UCB_NULL to discard
 * @param out_exit_code optional location for the exit code on success
 * @param perr optional location to store the error on failure
 * @return true on success
 */
UCB_API bool ucb_process_capture(const ucb_process_capture_opts* opts,
                                 ucb_buffer* out,
                                 ucb_buffer* err,
                                 int* out_exit_code,
                                 ucb_error** perr);

#endif // UCB_PROCESS_H
