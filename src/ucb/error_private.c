/**
 * This file is part of the UCB project
 * SPDX-FileCopyrightText: © 2025 Åke Svedin <ake@svedin.org>
 * SPDX-License-Identifier: MIT
 *
 * @brief Error handling implementation
 */

#include "ucb/error_private.h"

#include "ucb/error.h"
#include "ucb/errcodes.h"

#include <errno.h>

#if _WIN32
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <Windows.h>
#endif

#ifndef NOERROR
#define NOERROR 0
#endif

ucb_ecode ucb_err_wrap_errno(int err)
{
    switch (err)
    {
    case NOERROR:
        return UCB_OK;
    case EINVAL:
        return UCB_ERROR_INVALID_ARG;
    case ENOMEM:
        return UCB_ERROR_OUT_OF_MEMORY;
    case ERANGE:
        return UCB_ERROR_RANGE;
    default:
        return UCB_ERROR_UNKNOWN;
    }
}

ucb_ecode ucb_err_get_errno(void)
{
    return ucb_err_wrap_errno(errno);
}

#ifdef _WIN32

ucb_ecode ucb_err_wrap_win32(uint32_t err)
{
    switch (err)
    {
    case ERROR_SUCCESS:
        return UCB_OK;
    case ERROR_INSUFFICIENT_BUFFER:
    case ERROR_INVALID_FLAGS:
    case ERROR_INVALID_PARAMETER:
        return UCB_ERROR_INVALID_ARG;
    case ERROR_NOT_ENOUGH_MEMORY:
    case ERROR_OUTOFMEMORY:
        return UCB_ERROR_OUT_OF_MEMORY;
    case ERROR_NO_UNICODE_TRANSLATION:
        return UCB_ERROR_ENCODING;
    default:
        return UCB_ERROR_UNKNOWN;
    }
}

ucb_ecode ucb_err_get_win32(void)
{
    return ucb_err_wrap_win32(GetLastError());
}

#endif
