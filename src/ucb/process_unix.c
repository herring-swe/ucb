/**
 * @file process_unix.c
 *
 * This file is part of the UCB project
 * - SPDX-FileCopyrightText: © 2026 Åke Svedin <ake@svedin.org>
 * - SPDX-License-Identifier: MIT
 *
 * @brief POSIX implementation of the process launching backend
 *
 * The child runs only async-signal-safe calls between @c fork and @c exec. A
 * close-on-exec status pipe reports @c exec and early setup failures back to the
 * parent synchronously.
 */

#include "envmap_private.h"
#include "process_private.h"

#include <ucb/cstring.h>
#include <ucb/errcodes.h>
#include <ucb/error.h>
#include <ucb/memory.h>

#include <errno.h>
#include <fcntl.h>
#include <signal.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <time.h>
#include <unistd.h>

/* Poll interval for the timeout aware wait. The timeout is approximate. */
#define UCB_PROCESS_POLL_NS 1000000L

/* Parent environment, used when a custom environment is not supplied. */
extern char** environ;

static int64_t process_unix_now_ms(void)
{
    struct timespec ts;
#if defined(CLOCK_MONOTONIC)
    clock_gettime(CLOCK_MONOTONIC, &ts);
#else
    clock_gettime(CLOCK_REALTIME, &ts);
#endif
    return (int64_t)ts.tv_sec * 1000 + (int64_t)(ts.tv_nsec / 1000000);
}

static bool process_unix_is_executable_file(const char* path)
{
    struct stat st;
    if (stat(path, &st) != 0)
        return false;
    if (!S_ISREG(st.st_mode))
        return false;
    return access(path, X_OK) == 0;
}

/**
 * @brief Resolve a program to an absolute or relative executable path
 *
 * Used only when a custom environment replaces the parent one and @c execve is
 * required. A program containing a slash is used as is; otherwise the @c PATH of
 * the custom environment is searched, falling back to the parent @c PATH, then a
 * conservative default. An empty @c PATH component means the current directory.
 */
static char* process_unix_resolve(const char* program, const ucb_envmap* env, ucb_error** perr)
{
    if (strchr(program, '/'))
    {
        if (process_unix_is_executable_file(program))
        {
            char* dup = ucb_cstr_dup(program);
            if (!dup)
                ucb_throw(perr, UCB_ERROR_OUT_OF_MEMORY, "ucb_process_spawn: allocation failed");
            return dup;
        }

        ucb_throw_format(perr,
                         UCB_ERRSYS_ENOENT,
                         "ucb_process_spawn: executable not found: %s",
                         program);
        return UCB_NULL;
    }

    const char* path = env ? ucb_envmap_get(env, "PATH") : UCB_NULL;
    if (!path)
        path = getenv("PATH");
    if (!path)
        path = "/usr/local/bin:/usr/bin:/bin";

    const char* cursor = path;
    for (;;)
    {
        const char* colon = strchr(cursor, ':');
        size_t dir_len = colon ? (size_t)(colon - cursor) : strlen(cursor);
        size_t prog_len = strlen(program);
        size_t dir_used = dir_len ? dir_len : 1;
        size_t total = dir_used + 1 + prog_len + 1;

        char* candidate = (char*)ucb_malloc(total);
        if (!candidate)
        {
            ucb_throw(perr, UCB_ERROR_OUT_OF_MEMORY, "ucb_process_spawn: allocation failed");
            return UCB_NULL;
        }

        if (dir_len)
            memcpy(candidate, cursor, dir_len);
        else
            candidate[0] = '.';

        candidate[dir_used] = '/';
        memcpy(candidate + dir_used + 1, program, prog_len);
        candidate[dir_used + 1 + prog_len] = '\0';

        if (process_unix_is_executable_file(candidate))
            return candidate;

        ucb_free(candidate);

        if (!colon)
            break;
        cursor = colon + 1;
    }

    ucb_throw_format(perr,
                     UCB_ERRSYS_ENOENT,
                     "ucb_process_spawn: executable not found in PATH: %s",
                     program);
    return UCB_NULL;
}

static void process_unix_close_pipe(int pipe_fds[2])
{
    if (pipe_fds[0] >= 0)
        close(pipe_fds[0]);
    if (pipe_fds[1] >= 0)
        close(pipe_fds[1]);
    pipe_fds[0] = -1;
    pipe_fds[1] = -1;
}

static void process_unix_reap_quiet(pid_t pid)
{
    int status = 0;
    while (waitpid(pid, &status, 0) < 0 && errno == EINTR)
    {
    }
}

/* -------------------------------------------------------------------------- */
/*                                  Backend                                   */
/* -------------------------------------------------------------------------- */

ucb_process* ucb_process_plat_spawn(const ucb_process_opts* opts, ucb_error** perr)
{
    int status_pipe[2] = {-1, -1};
    char* resolved = UCB_NULL;
    char** envp = UCB_NULL;
    int fd_in = -1;
    int fd_out = -1;
    int fd_err = -1;
    pid_t pid;

    if (pipe(status_pipe) != 0)
    {
        ucb_throw_errno(perr, errno, "ucb_process_spawn: pipe failed");
        return UCB_NULL;
    }

    for (int i = 0; i < 2; ++i)
    {
        int flags = fcntl(status_pipe[i], F_GETFD);
        if (flags < 0 || fcntl(status_pipe[i], F_SETFD, flags | FD_CLOEXEC) < 0)
        {
            int e = errno;
            process_unix_close_pipe(status_pipe);
            ucb_throw_errno(perr, e, "ucb_process_spawn: fcntl failed");
            return UCB_NULL;
        }
    }

    /* Resolve the program in the parent so the child only ever calls execve,
       which is async-signal-safe; execvp is not. */
    resolved = process_unix_resolve(opts->argv[0], opts->env, perr);
    if (!resolved)
    {
        process_unix_close_pipe(status_pipe);
        return UCB_NULL;
    }

    if (opts->env)
    {
        envp = ucb_envmap_to_envp(opts->env, perr);
        if (!envp)
        {
            process_unix_close_pipe(status_pipe);
            ucb_free(resolved);
            return UCB_NULL;
        }
    }

    fd_in = opts->in ? ucb_file_get_fd(opts->in) : -1;
    fd_out = opts->out ? ucb_file_get_fd(opts->out) : -1;
    fd_err = opts->err ? ucb_file_get_fd(opts->err) : -1;
    if ((opts->in && fd_in < 0) || (opts->out && fd_out < 0) || (opts->err && fd_err < 0))
    {
        process_unix_close_pipe(status_pipe);
        ucb_free(resolved);
        ucb_envmap_envp_free(envp);
        ucb_throw(perr,
                  UCB_ERROR_INVALID_STATE,
                  "ucb_process_spawn: redirect handle has no descriptor");
        return UCB_NULL;
    }

    pid = fork();
    if (pid < 0)
    {
        int e = errno;
        process_unix_close_pipe(status_pipe);
        ucb_free(resolved);
        ucb_envmap_envp_free(envp);
        ucb_throw_errno(perr, e, "ucb_process_spawn: fork failed");
        return UCB_NULL;
    }

    if (pid == 0)
    {
        /* Child: only async-signal-safe calls below this point. */
        int failure = 0;

        if (opts->flags & UCB_PROCESS_DETACH)
            (void)setsid();

        if (fd_in >= 0 && fd_in != STDIN_FILENO && dup2(fd_in, STDIN_FILENO) < 0)
            failure = errno;
        if (!failure && fd_out >= 0 && fd_out != STDOUT_FILENO && dup2(fd_out, STDOUT_FILENO) < 0)
            failure = errno;
        if (!failure && fd_err >= 0 && fd_err != STDERR_FILENO && dup2(fd_err, STDERR_FILENO) < 0)
            failure = errno;
        if (!failure && opts->cwd && chdir(opts->cwd) != 0)
            failure = errno;

        /* dup2 clears FD_CLOEXEC on the duplicate but leaves the source open, so
           close any redirect source that would otherwise leak past exec. */
        if (fd_in > STDERR_FILENO)
            close(fd_in);
        if (fd_out > STDERR_FILENO && fd_out != fd_in)
            close(fd_out);
        if (fd_err > STDERR_FILENO && fd_err != fd_in && fd_err != fd_out)
            close(fd_err);

        if (!failure)
        {
            execve(resolved, (char* const*)opts->argv, envp ? envp : environ);
            failure = errno;
        }

        (void)!write(status_pipe[1], &failure, sizeof(failure));
        _exit(127);
    }

    /* Parent. */
    close(status_pipe[1]);
    status_pipe[1] = -1;

    {
        int exec_error = 0;
        ssize_t got;
        do
        {
            got = read(status_pipe[0], &exec_error, sizeof(exec_error));
        } while (got < 0 && errno == EINTR);

        int read_error = got < 0 ? errno : 0;
        close(status_pipe[0]);
        status_pipe[0] = -1;

        ucb_free(resolved);
        ucb_envmap_envp_free(envp);

        if (got == (ssize_t)sizeof(exec_error))
        {
            process_unix_reap_quiet(pid);
            ucb_throw_errno(perr, exec_error, "ucb_process_spawn: exec failed");
            return UCB_NULL;
        }

        if (got < 0)
        {
            (void)kill(pid, SIGKILL);
            process_unix_reap_quiet(pid);
            ucb_throw_errno(perr, read_error, "ucb_process_spawn: status pipe read failed");
            return UCB_NULL;
        }
    }

    {
        ucb_process* proc = ucb_calloc_type(1, ucb_process);
        if (!proc)
        {
            (void)kill(pid, SIGKILL);
            process_unix_reap_quiet(pid);
            ucb_throw(perr, UCB_ERROR_OUT_OF_MEMORY, "ucb_process_spawn: allocation failed");
            return UCB_NULL;
        }

        proc->pid = (ucb_pid)pid;
        proc->reaped = false;
        return proc;
    }
}

bool ucb_process_plat_wait(ucb_process* proc, int timeout_ms, bool* out_exited, ucb_error** perr)
{
    *out_exited = false;

    int status = 0;

    if (timeout_ms < 0)
    {
        pid_t r;
        do
        {
            r = waitpid((pid_t)proc->pid, &status, 0);
        } while (r < 0 && errno == EINTR);

        if (r < 0)
            return !ucb_throw_errno(perr, errno, "ucb_process_wait: waitpid failed");
    }
    else
    {
        int64_t deadline = timeout_ms > 0 ? process_unix_now_ms() + timeout_ms : 0;
        for (;;)
        {
            pid_t r = waitpid((pid_t)proc->pid, &status, WNOHANG);
            if (r == (pid_t)proc->pid)
                break;

            if (r < 0)
            {
                if (errno == EINTR)
                    continue;
                return !ucb_throw_errno(perr, errno, "ucb_process_wait: waitpid failed");
            }

            /* Still running. */
            if (timeout_ms == 0 || process_unix_now_ms() >= deadline)
                return true;

            struct timespec req = {0, UCB_PROCESS_POLL_NS};
            struct timespec rem;
            while (nanosleep(&req, &rem) != 0 && errno == EINTR)
                req = rem;
        }
    }

    proc->reaped = true;
    if (WIFEXITED(status))
        proc->exit_code = WEXITSTATUS(status);
    else if (WIFSIGNALED(status))
        proc->exit_code = 128 + WTERMSIG(status);
    else
        proc->exit_code = -1;

    *out_exited = true;
    return true;
}

bool ucb_process_plat_terminate(ucb_process* proc, bool force, ucb_error** perr)
{
    if (kill((pid_t)proc->pid, force ? SIGKILL : SIGTERM) != 0)
    {
        if (errno == ESRCH)
            return true;
        return !ucb_throw_errno(perr, errno, "ucb_process_terminate: kill failed");
    }
    return true;
}

void ucb_process_plat_free(ucb_process* proc)
{
    if (!proc || proc->reaped)
        return;

    /* Best effort: clear an already exited child, leave a running one alone. */
    int status = 0;
    pid_t r;
    do
    {
        r = waitpid((pid_t)proc->pid, &status, WNOHANG);
    } while (r < 0 && errno == EINTR);
}
