/**
 * This file is part of the UCB project
 * SPDX-FileCopyrightText: © 2025 Åke Svedin <ake@svedin.org>
 * SPDX-License-Identifier: MIT
 *
 * @brief
 */

#ifndef UCB_ERROR_PRIVATE_H
#define UCB_ERROR_PRIVATE_H

#include "ucb/cstring.h"
#include "ucb/error.h"

#include <stdint.h>

#if _WIN32
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <Windows.h>
#endif

ucb_ecode ucb_err_wrap_errno(int err);
ucb_ecode ucb_err_get_errno(void);

#define UCB_ERR_SET(err, res_, msg_)       \
    do                                     \
    {                                      \
        if (err)                           \
        {                                  \
            err->code = res_;               \
            err->msg = ucb_cstr_dup(msg_); \
        }                                  \
    } while (0)
#define UCB_ERR_SET_ERRNO(err, msg) \
    ucb_err_set_errno(err, msg) UCB_ERR_SET(err, ucb_err_get_errno(), msg)

#ifdef _WIN32
ucb_ecode ucb_err_wrap_win32(uint32_t err);
ucb_ecode ucb_err_get_win32(void);

#define UCB_ERR_SET_WIN32(err, msg) UCB_ERR_SET(err, ucb_err_get_win32(), msg)

#endif

#endif // UCB_ERROR_PRIVATE_H
