/**
 * This file is part of the UCB project
 * SPDX-FileCopyrightText: © 2025 Åke Svedin <ake@svedin.org>
 * SPDX-License-Identifier: MIT
 *
 * @brief Error handling implementation
 */

#include "ucb/error.h"

#include "ucb/cstring.h"
#include "ucb/defines.h"
#include "ucb/errcodes.h"
#include "ucb/memory.h"
#include "ucb/thread.h"
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

const ucb_error* ucb_error_msg(ucb_ecode code, const char* fmt, ...)
{
    const ucb_error* ret;
    va_list args;
    va_start(args, fmt);
    ret = ucb_error_msgv(code, fmt, args);
    va_end(args);
    return ret;
}

const ucb_error* ucb_error_msgv(ucb_ecode code, const char* fmt, va_list args)
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

void ucb_error_report(ucb_errlvl lvl, const ucb_error* err)
{
    if (s_ucb_errfunc)
        s_ucb_errfunc(lvl, err);
    else
        ucb_error_print(lvl, err);
}

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

const char* ucb_error_lvlstr(ucb_errlvl lvl)
{
    switch (lvl)
    {
    case UCB_ERRLVL_FATAL:
        return "FATAL ERROR";
    case UCB_ERRLVL_USER:
        return "ERROR";
    case UCB_ERRLVL_WARNING:
        return "WARNING";
    default:
        break;
    }
    return "UNKNOWN LEVEL";
}

const char* ucb_error_codestr(ucb_ecode error)
{
    // FIXME: Write all missing ones
    switch (error)
    {
    case UCB_OK:
        return "SUCCESS";
    case UCB_ERROR_UNKNOWN:
        return "ERROR_UNKNOWN";
    case UCB_ERROR_UNSUPPORTED:
        return "ERROR_UNSUPORTED";
    case UCB_ERROR_INVALID_ARG:
        return "ERROR_INVALID_ARG";
    case UCB_ERROR_NOT_FOUND:
        return "ERROR_NOT_FOUND";
    case UCB_ERROR_ACCESS_DENIED:
        return "ERROR_ACCESS_DENIED";
    case UCB_ERROR_IO:
        return "ERROR_IO";
    case UCB_ERROR_OUT_OF_MEMORY:
        return "ERROR_OUT_OF_MEMRY";
    case UCB_ERROR_ENCODING:
        return "ERROR_ENCODING";
    case UCB_ERROR_NOT_IMPLEMENTED:
        return "ERROR_NOT_IMPLEMENTED";
    case UCB_ERROR_RANGE:
        return "ERROR_RANGE";
    default:
        break;
    }
    assert(0);
    return "ERROR_UNKNOWN";
}
