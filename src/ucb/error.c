/**
 * This file is part of the UCB project
 * SPDX-FileCopyrightText: © 2025 Åke Svedin <ake@svedin.org>
 * SPDX-License-Identifier: MIT
 *
 * @brief Error handling implementation
 */

#include "ucb/error.h"

#include "ucb/defines.h"
#include "ucb/errcodes.h"
#include "ucb/types.h"

#include <assert.h>
#include <stdlib.h>
#include <threads.h>

// Threadlocal error code
static thread_local ucb_error_t s_ucb_last_error = UCB_OK;
static ucb_errfunc_t s_ucb_fatal_errfunc         = UCB_NULL;

void ucb_set_fatal_error(ucb_error_t error)
{
    ucb_set_last_error(error);
    s_ucb_fatal_errfunc(error);
}

void ucb_set_last_error(ucb_error_t error)
{
    s_ucb_last_error = error;
}

ucb_error_t ucb_get_last_error(void)
{
    return s_ucb_last_error;
}

const char* ucb_get_error_string(ucb_error_t error)
{
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

ucb_errfunc_t ucb_set_fatal_errfunc(ucb_errfunc_t func)
{
    ucb_errfunc_t prev  = s_ucb_fatal_errfunc;
    s_ucb_fatal_errfunc = func;
    return prev;
}

ucb_errfunc_t ucb_get_fatal_errfunc(void)
{
    return s_ucb_fatal_errfunc;
}
