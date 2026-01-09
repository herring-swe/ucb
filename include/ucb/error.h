/**
 * @file error.h
 * This file is part of the UCB project
 * - SPDX-FileCopyrightText: © 2026 Åke Svedin <ake@svedin.org>
 * - SPDX-License-Identifier: MIT
 *
 * @brief Error handling
 *
 * @todo Write a more fluid documentation.
 *
 * UCB handles errors in the following way:
 * - Fatal errors, user errors and warnings:
 *   - Are **reported** to a single error handling function.
 *   - By default these errors are written to stderr, but the program is **not** terminated.
 *   - The user can override the default error handler, with @ref ucb_error_set_func.
 *   - Examples or each type:
 *     - Fatal error: Out of memory. Functions try to return early, but no guarantee.
 *     - User error: Invalid arguments or states in input. Functions return early.
 *     - Warning: Recoverable errors where fallbacks may be used to continue.
 * - Complex errors:
 *   - For more complex errors, where input may lead to invalid results, arithmetic failures or
 * similar, the function will **throw** an @ref ucb_error.
 *   - These functions takes a @ref ucb_error double pointer as argument and must return a type such
 * as bool or NULL pointer which can indicate that an error has occured.
 *   - The @ref ucb_error contain an @ref ucb_ecode and a message of the error.
 *   - Errors may be ignored by not providing an @ref ucb_error pointer, but this is not
 * recommended.
 *   - Any errors thrown must be handled by the caller or propagated. The error is cleared by
 * calling @ref ucb_error_clear.
 * - Otherwise for simple functions, like getters, any input NULL pointers may be ignored if the
 * return type allows it.
 * - All functions must document if they behave differently than above rules.
 */

#ifndef UCB_ERROR_H
#define UCB_ERROR_H

#include "defines.h"
#include "errcodes.h"
#include "export.h"
#include "types.h"

#include <stdarg.h>
#include <stdbool.h>

typedef enum ucb_errlvl
{
    UCB_ERRLVL_FATAL,
    UCB_ERRLVL_USER,
    UCB_ERRLVL_WARNING,
} ucb_errlvl;

typedef struct ucb_error
{
    const char* msg;
    ucb_ecode code;
    bool is_static;
} ucb_error;

typedef void (*ucb_error_func)(ucb_errlvl lvl, const ucb_error* error);

/**
 * @brief Get a string representation of the error level.
 *
 * @param lvl the level
 * @return a string literal
 */
UCB_API const char* ucb_error_lvlstr(ucb_errlvl lvl);

/**
 * @brief Get a string representation of the error code.
 *
 * @param error the error
 * @return A string literal
 */
UCB_API const char* ucb_error_codestr(ucb_ecode code);

/* -------------------------------------------------------------------------- */
/*                                    Error                                   */
/* -------------------------------------------------------------------------- */

/**
 * @brief Copy an error so it can be stored
 * @param err Error to copy
 * @return Pointer to new error, which must be free'd with ucb_error_free
 */
UCB_API ucb_error* ucb_error_copy(const ucb_error* err);
/**
 * @brief Free an error
 * Can only be called on errors returned by ucb_error_copy or manual
 * allocation with ucb memory functions. Then is_static must be false.
 * Do not use with errors from reports or returned from functions.
 * This method will report a user error if either err is UCB_NULL or err->is_static is true.
 * @param err Error to free
 */
UCB_API void ucb_error_free(ucb_error* err);

/**
 * @brief Prepares an error for reporting with formatted message.
 * This will set a thread-local error object.
 * The message will be formatted on an internal buffer, of max
 * UCB_BUFSIZE_ERROR_MSG-1 characters.
 *
 * See general documentation about the lifetime of the error object.
 *
 * @param code the error code, must be non-zero.
 * @param fmt the format string
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
 * @brief Prepares an error for reporting with a literal message.
 * The literal message itself is set on the error object, so it must be valid for the
 * lifetime of the error object. The message will not be free'd.
 * @see ucb_error_format
 */
UCB_API const ucb_error* ucb_error_literal(ucb_ecode code, const char* msg);

/**
 * Print error to stderr
 * @param lvl the level
 * @param error the error
 */
UCB_API void ucb_error_print(ucb_errlvl lvl, const ucb_error* error);

/* -------------------------------------------------------------------------- */
/*                                Thrown errors                               */
/* -------------------------------------------------------------------------- */

/**
 * Clear an error previously thrown error from a function
 * The error will be free'd and the pointer set to UCB_NULL.
 * @param perr pointer to error to clear
 */
static inline void ucb_error_clear(const ucb_error** perr)
{
    if (perr && *perr)
    {
        ucb_error_free((ucb_error*)*perr);
        *perr = UCB_NULL;
    }
}

static inline bool ucb_error_check(const ucb_error* err)
{
    return (!err || err->code == 0);
}

UCB_API void ucb_throw(const ucb_error** perr, ucb_ecode code, const char* msg);
UCB_API void ucb_throw_format(const ucb_error** perr, ucb_ecode code, const char* fmt, ...);
UCB_API void ucb_throw_formatv(const ucb_error** perr, ucb_ecode code, const char* fmt,
                               va_list args);

/* -------------------------------------------------------------------------- */
/*                              Reporting errors                              */
/* -------------------------------------------------------------------------- */

#define UCB_VERIFY(expr, code, msg)                         \
    do                                                      \
    {                                                       \
        if (!(expr))                                        \
        {                                                   \
            ucb_user_format(code, "%s: %s", __func__, msg); \
            return;                                         \
        }                                                   \
    } while (0)

#define UCB_VERIFY_RET(expr, code, msg, ret)                \
    do                                                      \
    {                                                       \
        if (!(expr))                                        \
        {                                                   \
            ucb_user_format(code, "%s: %s", __func__, msg); \
            return ret;                                     \
        }                                                   \
    } while (0)

#define UCB_VERIFY_ARGS(expr) UCB_VERIFY(expr, UCB_ERROR_INVALID_ARG, "Invalid arguments")
#define UCB_VERIFY_ARGS_RET(expr, ret) \
    UCB_VERIFY_RET(expr, UCB_ERROR_INVALID_ARG, "Invalid arguments", ret)

#define UCB_VERIFY_ERRNO(status, msg) ucb_report_on_status(status, UCB_ERRLVL_USER, msg, __func__)

#ifdef _WIN32
#define UCB_VERIFY_WIN32(status, msg) ucb_report_on_win32(status, UCB_ERRLVL_USER, msg, __func__)
#endif

/**
 * Set the error function that will be called for any level.
 * The user can the override the default behavior.
 * If no function is set, the error will be printed using ucb_error_print.
 *
 * The function may be called from a different thread.
 *
 * @param func error function
 * @return previous error function, or UCB_NULL
 */
UCB_API ucb_error_func ucb_error_set_func(ucb_error_func func);
UCB_API ucb_error_func ucb_error_get_func(void);

/**
 * @brief Report error.
 * Depending on level, this will call the corresponding error function, if set.
 * By default the error will be printed with ucb_error_print and return.
 *
 * @param lvl the error level
 * @param error the error to report
 */
UCB_API void ucb_error_report(ucb_errlvl lvl, const ucb_error* error);

/* Convenience functions that will call ucb_error_report */

#define ucb_fatal_format(code, fmt, ...) \
    ucb_error_report(UCB_ERRLVL_FATAL, ucb_error_format(code, fmt, __VA_ARGS__))
#define ucb_fatal_literal(code, msg) \
    ucb_error_report(UCB_ERRLVL_FATAL, ucb_error_literal(code, msg))
#define ucb_user_format(code, fmt, ...) \
    ucb_error_report(UCB_ERRLVL_USER, ucb_error_format(code, fmt, __VA_ARGS__))
#define ucb_user_literal(code, msg) \
    ucb_error_report(UCB_ERRLVL_USER, ucb_error_literal(code, msg)) // break
#define ucb_warn_format(code, fmt, ...) \
    ucb_error_report(UCB_ERRLVL_WARNING, ucb_error_format(code, fmt, __VA_ARGS__))
#define ucb_warn_literal(code, msg) \
    ucb_error_report(UCB_ERRLVL_WARNING, ucb_error_literal(code, msg))

/* -------------------------------------------------------------------------- */
/*                           Functions to wrap errno                          */
/* -------------------------------------------------------------------------- */

ucb_ecode ucb_err_wrap_errno(int err);
ucb_ecode ucb_err_get_errno(void);

UCB_API bool ucb_report_on_status(int status, ucb_errlvl lvl, const char* UCB_RESTRICT msg,
                                  const char* UCB_RESTRICT function);

UCB_API bool ucb_throw_on_status(const ucb_error** perr, int status, const char* msg);

#ifdef _WIN32

/* -------------------------------------------------------------------------- */
/*                      Functions to wrap Windows errors                      */
/* -------------------------------------------------------------------------- */

UCB_API ucb_ecode ucb_err_wrap_win32(uint32_t err);
UCB_API ucb_ecode ucb_err_get_win32(void);
/**
 * Format a message from Windows error code into UTF-8
 * Must be free'd with ucb_free
 */
UCB_API char* ucb_err_msg_win32(uint32_t err);

UCB_API bool ucb_report_on_win32(uint32_t status, ucb_errlvl lvl, const char* msg,
                                 const char* function);
UCB_API bool ucb_throw_on_win32(const ucb_error** perr, uint32_t status, const char* msg);

#endif

#endif // UCB_ERROR_H
