/**
 * This file is part of the UCB project
 * SPDX-FileCopyrightText: © 2025 Åke Svedin <ake@svedin.org>
 * SPDX-License-Identifier: MIT
 *
 * @brief Error handling
 *
 * This, our philosophy is:
 *  - Prefer returning ucb_ecode
 *  - For simple functions, the basic return type is used:
 *    - For pointers, return UCB_NULL on error.
 *    - For arithmetic types, document which value may denote an error.
 *    - For bool, return maybe /s
 *
 * - Always check arguments (UCB_ERROR_INVALID_ARG)
 * - Always catch errors from system and internally
 * - Always set last error on error
 * - No need to clear errors
 * - If error handling becomes difficult, refactor to use ucb_ecode
 */

#ifndef UCB_ERROR_H
#define UCB_ERROR_H

#include "defines.h"
#include "export.h"
#include "types.h"

#include <stdarg.h>
#include <stdbool.h>

// Boilerplate macro for functions that return ucb_ecode
#define UCB_TRY(call)             \
    do                            \
    {                             \
        ucb_ecode _err = (call);  \
        if (_err != UCB_ERROR_OK) \
            return _err;          \
    } while (0)

/**
 * Boilerplate macro for functions that return a result struct
 */
#define UCB_TRY_RESULT(call, result_type)        \
    do                                           \
    {                                            \
        ucb_ecode _err = (call);                 \
        if (_err != UCB_ERROR_OK)                \
        {                                        \
            return (result_type){.err = errval}; \
        }                                        \
    } while (0)

typedef enum ucb_errlvl
{
    UCB_ERRLVL_FATAL,
    UCB_ERRLVL_USER,
    UCB_ERRLVL_WARNING,
} ucb_errlvl;

typedef struct ucb_error
{
    ucb_ecode code;
    const char* msg;
    bool is_static;
} ucb_error;

typedef void (*ucb_error_func)(ucb_errlvl lvl, const ucb_error* error);

/**
 * @brief Copy an error so it can be stored
 * @param err Error to copy
 * @return Pointer to new error, which must be free'd with ucb_error_free
 */
UCB_API ucb_error* ucb_error_copy(const ucb_error* err);
/**
 * @brief Free an error
 * Can only be called on errors returned by ucb_error_copy or manual
 * allocation with ucb mmeory functions. Then is_static must be false.
 * Do not use with errors from reports or returned from functions.
 * This method will report a user error if either err is UCB_NULL or err->is_static is true.
 * @param err Error to free, ignored if UCB_NULL or err->is_static is true
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
 * @return a pointer to the thread-local error object, which is valid until the next call to
 * ucb_error_msg or ucb_error_msgv, or until the thread exits. Do not free this pointer. If the
 * error is to be stored, use ucb_error_copy. If the error is
 */
UCB_API const ucb_error* ucb_error_msg(ucb_ecode code, const char* fmt, ...);
/**
 * @brief Prepares an error for reporting with formatted message using va_list.
 * @see ucb_error_msg
 */
UCB_API const ucb_error* ucb_error_msgv(ucb_ecode code, const char* fmt, va_list args);
/**
 * @brief Prepares an error for reporting with a literal message.
 * The literal message itself is set on the error object, so it must be valid for the
 * lifetime of the error object. The message will not be free'd.
 * @see ucb_error_msg
 */
UCB_API const ucb_error* ucb_error_literal(ucb_ecode code, const char* msg);

UCB_API void ucb_error_print(ucb_errlvl lvl, const ucb_error* error);

/**
 * @brief Report error.
 * Depending on level, this will call the corresponding error function, if set.
 * By default the error will be printed with ucb_error_print and return.
 *
 * @param lvl the error level
 * @param error the error to report
 */
UCB_API void ucb_error_report(ucb_errlvl lvl, const ucb_error* error);
static inline void ucb_fatal(const ucb_error* error)
{
    ucb_error_report(UCB_ERRLVL_FATAL, error);
}
static inline void ucb_user(const ucb_error* error)
{
    ucb_error_report(UCB_ERRLVL_USER, error);
}
static inline void ucb_warn(const ucb_error* error)
{
    ucb_error_report(UCB_ERRLVL_WARNING, error);
}

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
UCB_API const char* ucb_error_codestr(ucb_ecode error);

#endif // UCB_ERROR_H
