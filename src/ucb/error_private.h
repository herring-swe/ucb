/**
 * @file error_private.h
 *
 * This file is part of the UCB project
 * - SPDX-FileCopyrightText: © 2026 Åke Svedin <ake@svedin.org>
 * - SPDX-License-Identifier: MIT
 *
 * @brief Error handling internals
 *
 * These declarations are used by the library and its tests but are not part of
 * the public, installed API. They carry @ref UCB_API so tests can link them.
 */

#ifndef UCB_ERROR_PRIVATE_H
#define UCB_ERROR_PRIVATE_H

#include <ucb/error.h>
#include <ucb/export.h>

#include <stdarg.h>

/**
 * @brief Prepares an error for reporting with formatted message.
 *
 * Internal helper that sets a thread-local error object.
 * The message will be formatted on an internal buffer, of max
 * UCB_BUFSIZE_ERROR_MSG-1 characters.
 *
 * @warning A zero @p code or a UCB_NULL @p fmt is misuse and aborts. The
 * returned object is valid until the next call to @ref ucb_error_format or
 * @ref ucb_error_formatv on the same thread.
 *
 * @param code the error code, must be non-zero.
 * @param fmt the format string, must be non-NULL
 * @param ... the format arguments
 * @return a pointer to the thread-local error object.
 */
UCB_API const ucb_error* ucb_error_format(ucb_ecode code, const char* fmt, ...);
/**
 * @brief Prepares an error for reporting with formatted message using va_list.
 * @see ucb_error_format
 */
UCB_API const ucb_error* ucb_error_formatv(ucb_ecode code, const char* fmt, va_list args);

/**
 * @brief Report an error. Not to be called directly.
 *
 * @warning A UCB_NULL @p err is misuse and aborts. Reports made while already
 * inside an error report (for example from a custom handler) are suppressed to
 * avoid unbounded recursion.
 *
 * @see UCB_FATAL, UCB_REPORT, UCB_WARN
 */
UCB_API void ucb_error_report(ucb_errlvl lvl, const ucb_error* err);

/**
 * @brief Report a errno code as a system error
 *
 * The value is verified to be non-zero before reporting.
 * Value can either be errno or the return value from a function.
 */
#define UCB_REPORT_ERRNO(value, msg) ucb_report_errno((value), (msg), __func__)

#ifdef _WIN32
/**
 * @brief Report a Win32 error code
 *
 * The value is verified to be non-zero before reporting.
 * Value can either be GetLastError() or the return value from a function.
 */
#define UCB_REPORT_WIN32(value, msg) ucb_report_win32((value), (msg), __func__)
#endif

UCB_API bool ucb_report_errno(int status,
                              const char* UCB_RESTRICT msg,
                              const char* UCB_RESTRICT function);
UCB_API bool ucb_throw_errno(ucb_error** perr, int status, const char* msg);

#ifdef _WIN32

/**
 * @brief Format a message from Windows error code into UTF-8
 *
 * Must be free'd with ucb_free
 */
UCB_API char* ucb_err_msg_win32(uint32_t err);

UCB_API bool ucb_report_win32(uint32_t status,
                              const char* UCB_RESTRICT msg,
                              const char* UCB_RESTRICT function);
UCB_API bool ucb_throw_win32(ucb_error** perr, uint32_t status, const char* msg);

#endif

#endif // UCB_ERROR_PRIVATE_H
