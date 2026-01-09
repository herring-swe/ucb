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
    .code      = UCB_OK,
    .msg       = UCB_NULL,
    .is_static = true,
};

static ucb_error* ucb_error_get(void)
{
    if (!s_err.is_static)
    {
        s_err.code = UCB_ERROR_INVALID_STATE;
        s_err.msg  = "Static ucb_error has been manipulated. Forcing abort.";
        ucb_error_report(UCB_ERRLVL_USER, &s_err);
        abort();
    }
    return &s_err;
}

static ucb_error* ucb_error_prepare_throw(const ucb_error** perr)
{
    ucb_error* err = UCB_NULL;
    if (perr)
    {
        if (*perr)
        {
            ucb_user_literal(
                UCB_ERROR_UNHANDLED_ERROR,
                "ucb_error_prepare_throw: Found unhandled error when preparing a new error. All "
                "errors returned from functions must be free'd with ucb_error_free().");
            ucb_error_clear(perr);
        }
        err   = ucb_calloc_type(1, ucb_error);
        *perr = err;
        return err;
    }
    return err;
}

/* -------------------------------------------------------------------------- */
/*                                    Error                                   */
/* -------------------------------------------------------------------------- */

ucb_error* ucb_error_copy(const ucb_error* err)
{
    ucb_error* ret = UCB_NULL;
    if (err)
    {
        ret            = ucb_malloc_type(1, ucb_error);
        ret->code      = err->code;
        ret->msg       = ucb_cstr_dup(err->msg);
        ret->is_static = false;
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
        const ucb_error* err2 = ucb_error_literal(
            UCB_ERROR_INVALID_ARG, "ucb_err_release called with invalid error object");
        ucb_error_report(UCB_ERRLVL_USER, err2);
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
    ucb_error* err = ucb_error_get();
    err->code      = code;
    err->msg       = s_buf;
    // Will truncate if too long, including null terminator.
    ucb_cstr_vsnprintf(s_buf, UCB_BUFSIZE_ERROR_MSG, fmt, args);
    return err;
}

const ucb_error* ucb_error_literal(ucb_ecode code, const char* msg)
{
    ucb_error* err = ucb_error_get();
    err->code      = code;
    err->msg       = msg;
    return err;
}

void ucb_error_print(ucb_errlvl lvl, const ucb_error* err)
{
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

void ucb_error_clear(const ucb_error** perr)
{
    if (perr && *perr)
    {
        ucb_error_free((ucb_error*)*perr);
        *perr = UCB_NULL;
    }
}

void ucb_throw(const ucb_error** perr, ucb_ecode code, const char* msg)
{
    ucb_error* err = ucb_error_prepare_throw(perr);
    if (!err)
        return;

    err->code = code;
    err->msg  = ucb_cstr_dup(msg);
}

void ucb_throw_format(const ucb_error** perr, ucb_ecode code, const char* fmt, ...)
{
    va_list args;
    va_start(args, fmt);
    ucb_throw_formatv(perr, code, fmt, args);
    va_end(args);
}

void ucb_throw_formatv(const ucb_error** perr, ucb_ecode code, const char* fmt, va_list args)
{
    ucb_error* err = ucb_error_prepare_throw(perr);
    if (!err)
        return;

    err->code = code;
    ucb_cstr_vasprintf((char**)&err->msg, fmt, args);
}

/* -------------------------------------------------------------------------- */
/*                              Reporting errors                              */
/* -------------------------------------------------------------------------- */

ucb_error_func ucb_error_set_func(ucb_error_func func)
{
    ucb_error_func prev = s_ucb_errfunc;
    s_ucb_errfunc       = func;
    return prev;
}

ucb_error_func ucb_error_get_func(void)
{
    return s_ucb_errfunc;
}

void ucb_error_report(ucb_errlvl lvl, const ucb_error* err)
{
    if (s_ucb_errfunc)
        s_ucb_errfunc(lvl, err);
    else
        ucb_error_print(lvl, err);
}
