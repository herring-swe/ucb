/**
 * @file error_win32.c
 *
 * This file is part of the UCB project
 * - SPDX-FileCopyrightText: © 2026 Åke Svedin <ake@svedin.org>
 * - SPDX-License-Identifier: MIT
 *
 * @brief Error handling windows implementation
 */

#ifndef _WIN32
#error "This file is only for Windows"
#endif

#include "ucb/error.h"

#include "ucb/debug.h"
#include "ucb/errcodes.h"
#include "ucb/memory.h"
#include "ucb/string.h"

#if _WIN32
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <Windows.h>
#endif

ucb_ecode ucb_err_wrap_win32(uint32_t err)
{
    /**
     * No need to wrap all codes since we use
     * FormatMessage to print a detailed error.
     * Wrap the most common ones and try to generalize.
     *
     * In many cases, such as threads and file API, we should
     * catch the underlying error and present as an UCB error.
     */

    switch (err)
    {
    case ERROR_SUCCESS:
        return UCB_OK;

    case ERROR_INVALID_HANDLE:
        return UCB_ERRSYS_WIN_INVALID_HANDLE;

    case ERROR_CURRENT_DIRECTORY:
        return UCB_ERRSYS_WIN_CURRENT_DIRECTORY;

    case ERROR_DIRECTORY:
        return UCB_ERRSYS_WIN_ERROR_DIRECTORY;

    // The rest maps into POSIX errors
    case ERROR_INVALID_ADDRESS:
        return UCB_ERRSYS_EFAULT;

    case ERROR_NOT_SAME_DEVICE:
        return UCB_ERRSYS_EXDEV;

    case ERROR_BAD_UNIT:
        return UCB_ERRSYS_ENODEV;

    case ERROR_INVALID_DATA:
    case ERROR_INVALID_FUNCTION:
    case ERROR_INVALID_ACCESS:
    case ERROR_INVALID_DRIVE:
    case ERROR_INVALID_NAME:
    case ERROR_INVALID_USER_BUFFER:
    case ERROR_INVALID_PARAMETER:
    case ERROR_INVALID_FLAGS:
    case ERROR_INVALID_LOCK_RANGE:
    case ERROR_INVALID_TOKEN:
    case ERROR_BAD_ARGUMENTS:
    case ERROR_BAD_ENVIRONMENT:
    case ERROR_BAD_PATHNAME:
    case ERROR_INSUFFICIENT_BUFFER:
    case ERROR_NOT_A_REPARSE_POINT:
        return UCB_ERRSYS_EINVAL;

    case ERROR_NOT_SUPPORTED:
        return UCB_ERRSYS_ENOTSUP;

    case ERROR_CANCELLED:
        return UCB_ERRSYS_ECANCELED;

    case ERROR_OUTOFMEMORY:
    case ERROR_NOT_ENOUGH_MEMORY:
        return UCB_ERRSYS_ENOMEM;

    case ERROR_ACCESS_DENIED:
    case ERROR_LOCK_VIOLATION:
    case ERROR_NETWORK_ACCESS_DENIED:
        return UCB_ERRSYS_EACCES;

    case ERROR_CONNECTION_REFUSED:
        return UCB_ERRSYS_ECONNREFUSED;

    case ERROR_HOST_UNREACHABLE:
        return UCB_ERRSYS_EHOSTUNREACH;

    case ERROR_CONNECTION_ABORTED:
        return UCB_ERRSYS_ECONNABORTED;

    case ERROR_TIMEOUT:
        UCB_ERRSYS_ETIMEDOUT;

    case ERROR_WRITE_PROTECT:
        return UCB_ERRSYS_EROFS;

    case ERROR_FILE_NOT_FOUND:
    case ERROR_PATH_NOT_FOUND:
    case ERROR_BAD_NETPATH:
    case ERROR_BAD_NET_NAME:
        return UCB_ERRSYS_ENOENT;

    case ERROR_TOO_MANY_OPEN_FILES:
        return UCB_ERRSYS_EMFILE;

    case ERROR_ALREADY_EXISTS:
    case ERROR_FILE_EXISTS:
        return UCB_ERRSYS_EEXIST;

    case ERROR_FILE_TOO_LARGE:
        return UCB_ERRSYS_EFBIG;

    case ERROR_NOT_READY:
    case ERROR_BUSY:
    case ERROR_PATH_BUSY:
        return UCB_ERRSYS_EBUSY;

    case ERROR_SHARING_VIOLATION: // Transient error
        return UCB_ERRSYS_EAGAIN;

    case ERROR_OPERATION_IN_PROGRESS:
        return UCB_ERRSYS_EALREADY;

    case ERROR_NO_UNICODE_TRANSLATION:
        return UCB_ERRSYS_EILSEQ;

    case ERROR_DISK_FULL:
    case ERROR_HANDLE_DISK_FULL:
        return UCB_ERRSYS_ENOSPC;

    case ERROR_OPERATION_ABORTED:
        return UCB_ERRSYS_EINTR;

    case ERROR_SEEK:
        return UCB_ERRSYS_ESPIPE;

    case ERROR_WRITE_FAULT:
    case ERROR_READ_FAULT:
        return UCB_ERRSYS_EIO;

    case ERROR_BROKEN_PIPE:
        return UCB_ERRSYS_EPIPE;

    // Name too generic, may be used for other errors...
    case ERROR_BUFFER_OVERFLOW:
    case ERROR_FILENAME_EXCED_RANGE:
        return UCB_ERRSYS_ENAMETOOLONG;

    case ERROR_NO_DATA:
    case ERROR_MORE_DATA:
        return UCB_ERRSYS_ENODATA;

    case ERROR_DIR_NOT_EMPTY:
        return UCB_ERRSYS_ENOTEMPTY;

    // And the unhandled mapped into generic windows error
    default:
        UCB_DPRINT("UCB: Unhandled windows error code: %d\n", err);
        return UCB_ERRSYS_WIN_GENERIC;
    }
}

ucb_ecode ucb_err_get_win32(void)
{
    return ucb_err_wrap_win32(GetLastError());
}

char* ucb_err_msg_win32(uint32_t err)
{
    wchar_t* wstr = UCB_NULL;

    if (FormatMessage(FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM |
                          FORMAT_MESSAGE_IGNORE_INSERTS,
                      NULL, err, MAKELANGID(LANG_ENGLISH, SUBLANG_ENGLISH_US), (LPTSTR)&wstr, 0,
                      NULL) == 0)
    {
        return UCB_NULL;
    }
    // FIXME: Make low-level
    ucb_str* str = ucb_str_from_wchar(wstr, wcslen(wstr), UCB_NULL);
    LocalFree(wstr);

    char* data = UCB_NULL;
    if (str)
    {
        data = str->data;
        ucb_free(str);
    }
    return data;
}

bool ucb_report_win32(uint32_t status, const char* UCB_RESTRICT msg, const char* UCB_RESTRICT function)
{
    if (status == 0)
        return false;
    if (msg)
    {
        ucb_error_report(UCB_ERRLVL_SYSTEM,
                         ucb_error_format(ucb_err_wrap_win32(status), "%s: %s", function, msg));
    }
    else
    {
        char* errmsg = ucb_err_msg_win32(status);
        ucb_error_report(UCB_ERRLVL_SYSTEM,
                         ucb_error_format(ucb_err_wrap_win32(status), "%s: Unexpected error - %s",
                                          function, errmsg ? errmsg : "Unknown error"));
        if (errmsg)
            ucb_free(errmsg);
    }
    return true;
}

bool ucb_throw_win32(ucb_error** perr, uint32_t status, const char* msg)
{
    if (status == 0)
        return false;
    if (msg)
        ucb_throw(perr, ucb_err_wrap_win32(status), msg);
    else
    {
        char* errmsg = ucb_err_msg_win32(status);
        ucb_throw_format(perr, ucb_err_wrap_win32(status), "%s", errmsg ? errmsg : "Unknown error");
        if (errmsg)
            ucb_free(errmsg);
    }
    return true;
}
