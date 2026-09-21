/**
 * @file error.c
 *
 * This file is part of the UCB project
 * - SPDX-FileCopyrightText: © 2026 Åke Svedin <ake@svedin.org>
 * - SPDX-License-Identifier: MIT
 *
 * @brief Error handling implementation
 */

#include "ucb/error.h"

#include "mutex_private.h"
#include "once_private.h"

#include "ucb/cstring.h"
#include "ucb/defines.h"
#include "ucb/errcodes.h"
#include "ucb/memory.h"
#include "ucb/threads.h"
#include "ucb/types.h"

#include <assert.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>

static ucb_error_func s_ucb_errfunc = UCB_NULL;

// TODO Make compile configurable
#define UCB_BUFSIZE_ERROR_MSG 512
static UCB_THREAD_LOCAL char s_buf[UCB_BUFSIZE_ERROR_MSG];
static UCB_THREAD_LOCAL struct ucb_error s_err = {
    .code = UCB_OK,
    .msg = UCB_NULL,
    .is_static = true,
};

// Guards output to stderr for the default handler. The mutex is recursive so a
// nested report from the same thread cannot deadlock. Initialized once.
static ucb_mutex s_report_mutex;
static ucb_once s_report_once = UCB_ONCE_INIT;

// Prevents a custom error handler that reports an error from recursing forever.
static UCB_THREAD_LOCAL int s_report_depth = 0;

static void s_report_lock_init(void)
{
    ucb_mutex_init_recursive(&s_report_mutex);
}

static ucb_error* ucb_error_get(void)
{
    if (!s_err.is_static)
    {
        s_err.code = UCB_ERROR_INVALID_STATE;
        s_err.msg = "Static ucb_error has been manipulated. Forcing abort.";
        ucb_error_report(UCB_ERRLVL_USER, &s_err);
        abort();
    }
    return &s_err;
}

static ucb_error* ucb_error_prepare_throw(ucb_error** perr)
{
    ucb_error* err = UCB_NULL;
    if (perr)
    {
        if (*perr)
        {
            UCB_WARN(
                "Found unhandled error when preparing a new error. All errors returned from "
                "functions must be free'd with ucb_error_free().");
            ucb_error_clear(perr);
        }
        err = ucb_calloc_type(1, ucb_error);
        if (!err)
        {
            UCB_FATAL(UCB_ERROR_OUT_OF_MEMORY, "Failed to allocate error object");
            return UCB_NULL;
        }
        *perr = err;
        return err;
    }
    return err;
}

ucb_error_func ucb_error_set_func(ucb_error_func func)
{
    ucb_error_func prev = s_ucb_errfunc;
    s_ucb_errfunc = func;
    return prev;
}

ucb_error_func ucb_error_get_func(void)
{
    return s_ucb_errfunc;
}

/* -------------------------------------------------------------------------- */
/*                                    Error                                   */
/* -------------------------------------------------------------------------- */

ucb_error* ucb_error_copy(const ucb_error* err)
{
    ucb_error* ret = UCB_NULL;
    if (err)
    {
        ret = ucb_calloc_type(1, ucb_error);
        if (!ret)
        {
            UCB_FATAL(UCB_ERROR_OUT_OF_MEMORY, "Failed to allocate error copy");
            return UCB_NULL;
        }
        ret->code = err->code;
        ret->msg = ucb_cstr_dup(err->msg);
        if (err->msg && !ret->msg)
        {
            UCB_FATAL(UCB_ERROR_OUT_OF_MEMORY, "Failed to copy error message");
            ucb_free(ret);
            return UCB_NULL;
        }
        // ret->is_static = false; // Const value set by calloc.
    }
    return ret;
}

void ucb_error_free(ucb_error* err)
{
    if (err && !err->is_static)
    {
        ucb_free((void*)err->msg);
        ucb_free(err);
    }
    else
    {
        UCB_REPORT(UCB_ERROR_INVALID_ARG, "Invalid error object");
    }
}

const ucb_error* ucb_error_format(ucb_ecode code, const char* fmt, ...)
{
    const ucb_error* ret;
    va_list args;
    va_start(args, fmt);
    ret = ucb_error_formatv(code, fmt, args);
    va_end(args);
    return ret;
}

const ucb_error* ucb_error_formatv(ucb_ecode code, const char* fmt, va_list args)
{
    UCB_VERIFY_ARGS(code != UCB_OK && fmt != UCB_NULL);

    ucb_error* err = ucb_error_get();
    err->code = code;
    err->msg = s_buf;
    // Will truncate if too long, including null terminator.
    ucb_cstr_vsnprintf(s_buf, UCB_BUFSIZE_ERROR_MSG, fmt, args);
    return err;
}

void ucb_error_print(ucb_errlvl lvl, const ucb_error* err)
{
    UCB_VERIFY_ARGS(err);

    fprintf(stderr, "\n\n** UCB %s **\n", ucb_error_lvlstr(lvl));
    fprintf(stderr, "%s\n", ucb_error_codestr(err->code));
    if (err->msg && err->msg[0])
        fprintf(stderr, "%s\n\n", err->msg);
    else
        fprintf(stderr, "\n");
}

/* -------------------------------------------------------------------------- */
/*                                Thrown errors                               */
/* -------------------------------------------------------------------------- */

void ucb_error_clear(ucb_error** perr)
{
    if (perr && *perr)
    {
        ucb_error_free(*perr);
        *perr = UCB_NULL;
    }
}

void ucb_throw(ucb_error** perr, ucb_ecode code, const char* msg)
{
    UCB_VERIFY_ARGS(code != UCB_OK && msg != UCB_NULL);

    ucb_error* err = ucb_error_prepare_throw(perr);
    if (!err)
        return;

    err->code = code;
    err->msg = ucb_cstr_dup(msg);
    if (!err->msg)
    {
        UCB_FATAL(UCB_ERROR_OUT_OF_MEMORY, "Failed to copy error message");
        ucb_error_clear(perr);
    }
}

void ucb_throw_format(ucb_error** perr, ucb_ecode code, const char* fmt, ...)
{
    va_list args;
    va_start(args, fmt);
    ucb_throw_formatv(perr, code, fmt, args);
    va_end(args);
}

void ucb_throw_formatv(ucb_error** perr, ucb_ecode code, const char* fmt, va_list args)
{
    UCB_VERIFY_ARGS(code != UCB_OK && fmt != UCB_NULL);

    ucb_error* err = ucb_error_prepare_throw(perr);
    if (!err)
        return;

    err->code = code;
    if (ucb_cstr_vasprintf((char**)&err->msg, fmt, args) < 0)
    {
        UCB_FATAL(UCB_ERROR_OUT_OF_MEMORY, "Failed to format error message");
        ucb_error_clear(perr);
    }
}

/* -------------------------------------------------------------------------- */
/*                              Reporting errors                              */
/* -------------------------------------------------------------------------- */

void ucb_error_report(ucb_errlvl lvl, const ucb_error* err)
{
    UCB_VERIFY_ARGS(err);

    if (s_report_depth > 0)
    {
        // A handler reported an error while handling one. Avoid recursion and
        // any lock, and just note it.
        fprintf(stderr,
                "\n\n** UCB %s **\nReentrant error report suppressed\n\n",
                ucb_error_lvlstr(lvl));
        return;
    }

    s_report_depth++;
    if (s_ucb_errfunc)
    {
        s_ucb_errfunc(lvl, err);
    }
    else
    {
        ucb_once_run(&s_report_once, s_report_lock_init);
        ucb_mutex_lock(&s_report_mutex);
        ucb_error_print(lvl, err);
        if (lvl != UCB_ERRLVL_WARNING && lvl != UCB_ERRLVL_USER)
        {
            s_report_depth--;
            abort();
        }
        ucb_mutex_unlock(&s_report_mutex);
    }
    s_report_depth--;
}

void ucb_report_fatal(ucb_ecode code, const char* fmt, ...)
{
    va_list args;
    va_start(args, fmt);
    ucb_error_report(UCB_ERRLVL_FATAL, ucb_error_formatv(code, fmt, args));
    va_end(args);
}

void ucb_report_user(ucb_ecode code, const char* fmt, ...)
{
    va_list args;
    va_start(args, fmt);
    ucb_error_report(UCB_ERRLVL_USER, ucb_error_formatv(code, fmt, args));
    va_end(args);
    abort();
}

void ucb_report_warning(const char* fmt, ...)
{
    va_list args;
    va_start(args, fmt);
    ucb_error_report(UCB_ERRLVL_WARNING, ucb_error_formatv(UCB_ERROR_WARNING, fmt, args));
    va_end(args);
}
