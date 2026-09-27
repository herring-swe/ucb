/**
 * @file process.c
 *
 * This file is part of the UCB project
 * - SPDX-FileCopyrightText: © 2026 Åke Svedin <ake@svedin.org>
 * - SPDX-License-Identifier: MIT
 *
 * @brief Process launching, shared platform independent logic
 */

#include "process_private.h"

#include <ucb/buffer.h>
#include <ucb/errcodes.h>
#include <ucb/memory.h>
#include <ucb/pipe.h>
#include <ucb/task.h>
#include <ucb/threads.h>

#include <string.h>

/* -------------------------------------------------------------------------- */
/*                              Shared validation                             */
/* -------------------------------------------------------------------------- */

static bool process_validate_opts(const ucb_process_opts* opts, ucb_error** perr)
{
    UCB_VERIFY_ARGS(opts);
    UCB_VERIFY_ARGS(opts->argv);
    UCB_VERIFY_ARGS(opts->argv[0]);

    if (opts->cwd && opts->cwd[0] == '\0')
    {
        ucb_throw(perr, UCB_ERROR_INVALID_ARG, "ucb_process: cwd must not be empty");
        return false;
    }

    if (opts->in && !ucb_file_is_valid(opts->in))
    {
        ucb_throw(perr, UCB_ERROR_INVALID_STATE, "ucb_process: stdin handle is not open");
        return false;
    }
    if (opts->out && !ucb_file_is_valid(opts->out))
    {
        ucb_throw(perr, UCB_ERROR_INVALID_STATE, "ucb_process: stdout handle is not open");
        return false;
    }
    if (opts->err && !ucb_file_is_valid(opts->err))
    {
        ucb_throw(perr, UCB_ERROR_INVALID_STATE, "ucb_process: stderr handle is not open");
        return false;
    }

    return true;
}

/* -------------------------------------------------------------------------- */
/*                                  Lifetime                                  */
/* -------------------------------------------------------------------------- */

ucb_process* ucb_process_spawn(const ucb_process_opts* opts, ucb_error** perr)
{
    if (!process_validate_opts(opts, perr))
        return UCB_NULL;

    return ucb_process_plat_spawn(opts, perr);
}

bool ucb_process_wait(ucb_process* proc, int timeout_ms, int* out_exit_code, ucb_error** perr)
{
    UCB_VERIFY_ARGS(proc);

    if (proc->reaped)
    {
        if (out_exit_code)
            *out_exit_code = proc->exit_code;
        return true;
    }

    bool exited = false;
    if (!ucb_process_plat_wait(proc, timeout_ms, &exited, perr))
        return false;

    if (!exited)
        return false;

    if (out_exit_code)
        *out_exit_code = proc->exit_code;
    return true;
}

static bool process_terminate_common(ucb_process* proc, bool force, ucb_error** perr)
{
    UCB_VERIFY_ARGS(proc);

    if (proc->reaped)
        return true;

    return ucb_process_plat_terminate(proc, force, perr);
}

bool ucb_process_terminate(ucb_process* proc, ucb_error** perr)
{
    return process_terminate_common(proc, false, perr);
}

bool ucb_process_kill(ucb_process* proc, ucb_error** perr)
{
    return process_terminate_common(proc, true, perr);
}

ucb_pid ucb_process_get_pid(const ucb_process* proc)
{
    return proc ? proc->pid : UCB_PID_INVALID;
}

void ucb_process_free(ucb_process* proc)
{
    if (!proc)
        return;

    ucb_process_plat_free(proc);
    ucb_free(proc);
}

/* -------------------------------------------------------------------------- */
/*                                  Capture                                   */
/* -------------------------------------------------------------------------- */

typedef struct process_capture_drain
{
    ucb_file* file;
    ucb_buffer* buf;
    ucb_error* err;
    bool ok;
} process_capture_drain;

static int process_capture_drain_func(void* arg)
{
    process_capture_drain* drain = (process_capture_drain*)arg;
    drain->ok = ucb_file_read_buffer(drain->file, drain->buf, &drain->err);
    return drain->ok ? 0 : 1;
}

/* Move an error into *perr, keeping an earlier error if one is already set. */
static void process_take_error(ucb_error** perr, ucb_error** src)
{
    if (!*src)
        return;

    if (perr && !UCB_IS_THROWN(*perr))
    {
        *perr = *src;
        *src = UCB_NULL;
    }
    else
    {
        ucb_error_clear(src);
    }
}

bool ucb_process_capture(const ucb_process_capture_opts* opts,
                         ucb_buffer* out,
                         ucb_buffer* err,
                         int* out_exit_code,
                         ucb_error** perr)
{
    UCB_VERIFY_ARGS(opts);
    UCB_VERIFY_ARGS(opts->argv);
    UCB_VERIFY_ARGS(opts->argv[0]);

    if (opts->flags & UCB_PROCESS_DETACH)
    {
        ucb_throw(perr,
                  UCB_ERROR_INVALID_ARG,
                  "ucb_process_capture: UCB_PROCESS_DETACH is not supported");
        return false;
    }

    if (out && out == err)
    {
        ucb_throw(perr,
                  UCB_ERROR_INVALID_ARG,
                  "ucb_process_capture: out and err must be distinct buffers");
        return false;
    }

    bool ok = false;
    bool io_ok = true;
    ucb_error* io_err = UCB_NULL;
    ucb_error* wait_err = UCB_NULL;
    ucb_file* devnull_r = UCB_NULL;
    ucb_file* devnull_w = UCB_NULL;
    ucb_file* out_rd = UCB_NULL;
    ucb_file* out_wr = UCB_NULL;
    ucb_file* err_rd = UCB_NULL;
    ucb_file* err_wr = UCB_NULL;
    ucb_process* proc = UCB_NULL;
    ucb_thread* drainer = UCB_NULL;
    process_capture_drain edrain = {0};

#ifdef _WIN32
    const char* null_path = "NUL";
#else
    const char* null_path = "/dev/null";
#endif

    devnull_r = ucb_file_open(null_path, UCB_FILE_READ | UCB_FILE_CLOEXEC, perr);
    if (!devnull_r)
        goto done;

    devnull_w = ucb_file_open(null_path, UCB_FILE_WRITE | UCB_FILE_CLOEXEC, perr);
    if (!devnull_w)
        goto done;

    if (out)
    {
        if (!ucb_pipe_create(&out_rd, &out_wr, UCB_FILE_CLOEXEC, perr))
            goto done;
#ifdef _WIN32
        if (!ucb_file_set_inherit(out_wr, true, perr))
            goto done;
#endif
    }

    if (err)
    {
        if (!ucb_pipe_create(&err_rd, &err_wr, UCB_FILE_CLOEXEC, perr))
            goto done;
#ifdef _WIN32
        if (!ucb_file_set_inherit(err_wr, true, perr))
            goto done;
#endif
    }

    {
        ucb_process_opts popts = ucb_process_opts_make();
        popts.argv = opts->argv;
        popts.env = opts->env;
        popts.cwd = opts->cwd;
        popts.flags = opts->flags;
        popts.in = devnull_r;
        popts.out = out ? out_wr : devnull_w;
        popts.err = err ? err_wr : devnull_w;

        proc = ucb_process_spawn(&popts, perr);
        if (!proc)
            goto done;
    }

    /* The parent must not keep the write ends open, or the child never sees EOF. */
    ucb_file_free(out_wr);
    out_wr = UCB_NULL;
    ucb_file_free(err_wr);
    err_wr = UCB_NULL;

    if (out && err)
    {
        edrain.file = err_rd;
        edrain.buf = err;
        drainer = ucb_thread_new();
        if (!drainer)
        {
            ucb_throw(perr,
                      UCB_ERROR_OUT_OF_MEMORY,
                      "ucb_process_capture: thread allocation failed");
        }
        else
        {
            ucb_task task = ucb_task_make(process_capture_drain_func);
            task.arg = &edrain;
            if (!ucb_thread_start(drainer, task))
            {
                ucb_thread_free(drainer);
                drainer = UCB_NULL;
                ucb_throw(perr,
                          UCB_ERROR_INTERNAL,
                          "ucb_process_capture: failed to start drain thread");
            }
        }

        if (drainer)
        {
            io_ok = ucb_file_read_buffer(out_rd, out, &io_err);
            ucb_thread_join(drainer);
            ucb_thread_free(drainer);
            drainer = UCB_NULL;

            if (!edrain.ok)
            {
                io_ok = false;
                if (!io_err)
                {
                    io_err = edrain.err;
                    edrain.err = UCB_NULL;
                }
            }
        }
        else
        {
            /* Without a concurrent drainer a full pipe could block the child
               forever; kill it so both read ends reach EOF, then drain them. */
            io_ok = false;
            ucb_error* kill_err = UCB_NULL;
            (void)ucb_process_kill(proc, &kill_err);
            ucb_error_clear(&kill_err);
            (void)ucb_file_read_buffer(out_rd, out, UCB_NULL);
            (void)ucb_file_read_buffer(err_rd, err, UCB_NULL);
        }

        ucb_error_clear(&edrain.err);
    }
    else if (out)
    {
        io_ok = ucb_file_read_buffer(out_rd, out, &io_err);
    }
    else if (err)
    {
        io_ok = ucb_file_read_buffer(err_rd, err, &io_err);
    }

    {
        int exit_code = 0;
        bool waited = ucb_process_wait(proc, -1, &exit_code, &wait_err);
        if (waited && out_exit_code)
            *out_exit_code = exit_code;
        ok = io_ok && waited;
    }

done:
    if (drainer)
    {
        ucb_thread_join(drainer);
        ucb_thread_free(drainer);
    }

    ucb_process_free(proc);
    ucb_file_free(out_wr);
    ucb_file_free(err_wr);
    ucb_file_free(out_rd);
    ucb_file_free(err_rd);
    ucb_file_free(devnull_r);
    ucb_file_free(devnull_w);

    process_take_error(perr, &io_err);
    process_take_error(perr, &wait_err);

    return ok;
}
