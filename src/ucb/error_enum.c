/**
 * @file error_enum.c
 * 
 * This file is part of the UCB project
 * - SPDX-FileCopyrightText: © 2026 Åke Svedin <ake@svedin.org>
 * - SPDX-License-Identifier: MIT
 * 
 * @brief Error handling enum functions
 */

#include "ucb/debug.h"
#include "ucb/error.h"

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
    UCB_WARN(UCB_ERROR_INVALID_ARG, "No match for error level %d", lvl);
    return "UNKNOWN LEVEL";
}

const char* ucb_error_codestr(ucb_ecode code)
{
    // FIXME: Write all missing ones
    switch (code)
    {
    case UCB_OK:
        return "SUCCESS";
    case UCB_ERROR_OUT_OF_MEMORY:
        return "ERROR_OUT_OF_MEMRY";
    case UCB_ERROR_INVALID_ARG:
        return "ERROR_INVALID_ARG";
    case UCB_ERROR_UNHANDLED_ERROR:
        return "ERROR_UNHANDLED_ERROR";
    case UCB_ERROR_OUT_OF_BOUNDS:
        return "ERROR_OUT_OF_BOUNDS";
    case UCB_ERROR_INVALID_UTF8:
        return "ERROR_INVALID_UTF8";
    case UCB_ERROR_INVALID_CODEPOINT:
        return "ERROR_INVALID_CODEPOINT";
    case UCB_ERROR_INTERNAL:
        return "ERROR_INTERNAL";
    case UCB_ERROR_INVALID_STATE:
        return "ERROR_INVALID_STATE";
    case UCB_ERROR_NOT_IMPLEMENTED:
        return "ERROR_NOT_IMPLEMENTED";
    case UCB_ERROR_BUFFER:
        return "ERROR_BUFFER";
    case UCB_ERROR_LOCKED:
        return "ERROR_LOCKED";
    case UCB_ERROR_MAX_SIZE:
        return "ERROR_MAX_SIZE";
    case UCB_ERROR_INVALID_ALLOC:
        return "ERROR_INVALID_ALLOC";
    case UCB_ERROR_THREAD_BUSY:
        return "ERROR_THREAD_BUSY";
    case UCB_ERROR_MUTEX_LOCKED:
        return "ERROR_MUTEX_LOCKED";
    default:
        break;
    }
    UCB_WARN(UCB_ERROR_INVALID_ARG, "No match for error");
    return "ERROR_UNKNOWN";
}
