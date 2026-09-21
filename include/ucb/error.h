/**
 * @file error.h
 * This file is part of the UCB project
 * - SPDX-FileCopyrightText: © 2026 Åke Svedin <ake@svedin.org>
 * - SPDX-License-Identifier: MIT
 *
 * @brief Error handling
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
 *
 * Edge cases in this module follow the same severity model:
 * - Misuse of the API (NULL where not allowed, a zero error code, a NULL format
 *   string, or freeing an error that is not owned) is reported as a user error,
 *   which always aborts.
 * - Internal allocation failures while building or copying an error are reported
 *   as fatal errors. These abort with the default handler but can be overridden.
 *   An error is never silently dropped.
 * - Unknown error levels and codes are not misuse: @ref ucb_error_lvlstr and
 *   @ref ucb_error_codestr return static fallbacks instead of aborting.
 * - @ref ucb_error_report suppresses reentrant reports made from an error
 *   handler, to avoid unbounded recursion.
 */

#ifndef UCB_ERROR_H
#define UCB_ERROR_H

#include <ucb/defines.h>
#include <ucb/errcodes.h>
#include <ucb/export.h>
#include <ucb/types.h>

#include <stdarg.h>
#include <stdbool.h>

typedef enum ucb_errlvl
{
    /**
     * @brief User error
     *
     * Invalid arguments, invalid state of given objects and other
     * aspects of bad usage.
     * Will always lead to abort.
     */
    UCB_ERRLVL_USER,
    /**
     * @brief Fatal error
     *
     * Out of memory or UCB internal error.
     * Aborting by default but overridable.
     */
    UCB_ERRLVL_FATAL,
    /**
     * @brief System error
     *
     * Unexpected system errors (e.g. thread creation failed).
     * Aborting by default but overrideable.
     */
    UCB_ERRLVL_SYSTEM,
    /**
     * @brief Warning
     *
     * Any non-severe error or detail that needs to be logged
     * but not aborted, unless the user choose to.
     */
    UCB_ERRLVL_WARNING,
} ucb_errlvl;

/**
 * @struct ucb_error
 * @brief Error information for thrown errors
 */
typedef struct ucb_error
{
    const char* msg;      ///< Error message.
    ucb_ecode code;       ///< Error code.
    const bool is_static; ///< Used internally.
} ucb_error;

typedef void (*ucb_error_func)(ucb_errlvl lvl, const ucb_error* error);

/**
 * @brief Set the global error function
 *
 * The function will be called for any error level. @see ucb_error_func.
 * If no function is set, the error will be printed using @ref ucb_error_print.
 *
 * The function may be called from a different thread.
 *
 * @warning The handler is stored in a global without synchronization. Set it
 * before any other thread may report an error.
 *
 * @param func error function
 * @return previous error function, or UCB_NULL
 */
UCB_API ucb_error_func ucb_error_set_func(ucb_error_func func);
UCB_API ucb_error_func ucb_error_get_func(void);

/**
 * @brief Get a string representation of the error level.
 * @param lvl the level
 * @return a string literal, or "UNKNOWN" if the level is not recognized
 */
UCB_API const char* ucb_error_lvlstr(ucb_errlvl lvl);

/**
 * @brief Get a string representation of the error code.
 * @param code the error
 * @return A string literal, or "UNKNOWN_ERROR" if the code is not recognized
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
 *
 * Can only be called on errors returned by @ref ucb_error_copy() or manual
 * allocation with ucb memory functions. Then is_static must be false.
 * Do not use with errors from reports or returned from functions.
 * This method will report a user error if either err is UCB_NULL or err->is_static is true.
 * @param err Error to free
 */
UCB_API void ucb_error_free(ucb_error* err);

/**
 * @brief Prepares an error for reporting with formatted message.
 *
 * This will set a thread-local error object.
 * The message will be formatted on an internal buffer, of max
 * UCB_BUFSIZE_ERROR_MSG-1 characters.
 *
 * See general documentation about the lifetime of the error object.
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
 * @brief Print error to stderr
 *
 * @warning A UCB_NULL @p error is misuse and aborts.
 *
 * @param lvl the level
 * @param error the error, must be non-NULL
 */
UCB_API void ucb_error_print(ucb_errlvl lvl, const ucb_error* error);

/* -------------------------------------------------------------------------- */
/*                                Thrown errors                               */
/* -------------------------------------------------------------------------- */

/**
 * @brief Check if an error is set and refer to a valid error object.
 * @param err the ucb_error object to check
 */
#define UCB_IS_THROWN(err) ((err) && (err)->code != 0)

/**
 * @brief Clear an error previously thrown error from a function
 *
 * This is ment to be used in intermediate functions that handles an error
 * before returning to the caller.
 * For the top-level function, use @ref ucb_error_free() instead.
 *
 * The error will be free'd and the pointer set to UCB_NULL.
 * @param perr pointer to error to clear
 */
UCB_API void ucb_error_clear(ucb_error** perr);

/**
 * @brief Throw an error with a static message
 *
 * If @p perr is non-NULL, a new error object is allocated and stored in
 * @p *perr. An existing error in @p *perr is reported as a warning and cleared
 * first.
 *
 * @warning @p code must be non-zero and @p msg must be non-NULL, otherwise it is
 * misuse and aborts. If the error object or message cannot be allocated, a fatal
 * out-of-memory error is reported and @p *perr may be left as UCB_NULL.
 *
 * @param perr optional pointer to store the error
 * @param code the error code, must be non-zero
 * @param msg the error message, must be non-NULL
 */
UCB_API void ucb_throw(ucb_error** perr, ucb_ecode code, const char* msg);

/**
 * @brief Throw an error with a formatted message
 * @see ucb_throw
 */
UCB_API void ucb_throw_format(ucb_error** perr, ucb_ecode code, const char* fmt, ...);

/**
 * @brief Throw an error with a formatted message using va_list
 * @see ucb_throw
 */
UCB_API void ucb_throw_formatv(ucb_error** perr, ucb_ecode code, const char* fmt, va_list args);

/* -------------------------------------------------------------------------- */
/*                              Reporting errors                              */
/* -------------------------------------------------------------------------- */

/**
 * @brief Report a user error
 *
 * Always aborts(), since this is a breach on the contract between the user and the library
 * and needs to be caught and fixed early.
 */
#define UCB_REPORT(code, fmt, ...) ucb_report_user((code), "%s: " fmt, __func__, ##__VA_ARGS__)
#define UCB_REPORT_MSG(code, msg) ucb_report_user((code), "%s: %s", __func__, (msg))

/**
 * @brief Report a fatal error
 *
 * Only used for severe errors like out-of-memory or internal errors.
 * Only exits the program if the user has set a custom error handler to do so.
 */
#define UCB_FATAL(code, fmt, ...) ucb_report_fatal((code), "%s: " fmt, __func__, ##__VA_ARGS__)

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

/**
 * @brief Report a warning
 */
#define UCB_WARN(fmt, ...) ucb_report_warning("%s: " fmt, __func__, ##__VA_ARGS__)

/**
 * @brief Report a thrown error
 *
 * Reports the code and message of @p err as a user error, which aborts.
 * If @p err is UCB_NULL or has no code, invalid arguments are reported instead.
 * A missing message is replaced with a fallback.
 */
#define UCB_REPORT_ERROR(err)                                                      \
    do                                                                             \
    {                                                                              \
        if (UCB_IS_THROWN(err))                                                    \
            UCB_REPORT_MSG((err)->code, (err)->msg ? (err)->msg : "(no message)"); \
        else                                                                       \
            UCB_REPORT(UCB_ERROR_INVALID_ARG, "No error to report");               \
    } while (0)

/**
 * @brief Verify an expression and report a user error if it fails
 */
#define UCB_VERIFY(expr, code, fmt, ...)                           \
    do                                                             \
    {                                                              \
        if (!(expr))                                               \
            UCB_REPORT(code, "%s: " fmt, __func__, ##__VA_ARGS__); \
    } while (0)

#define UCB_VERIFY_MSG(expr, code, msg) \
    do                                  \
    {                                   \
        if (!(expr))                    \
            UCB_REPORT_MSG(code, msg);  \
    } while (0)

#define UCB_VERIFY_ERROR(expr, err) UCB_VERIFY_MSG(expr, err->code, err->msg)

/**
 * @brief Verify an expression and report invalid arguments if it fails
 */
#define UCB_VERIFY_ARGS(expr) UCB_VERIFY(expr, UCB_ERROR_INVALID_ARG, "Invalid arguments")

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
 * @brief Report a fatal error
 *
 * Prefer to use the macro @ref UCB_FATAL
 */
UCB_API void ucb_report_fatal(ucb_ecode code, const char* fmt, ...);

/**
 * @brief Report a user error
 *
 * Prefer to use the macro @ref UCB_REPORT
 */
UCB_API_NORETURN void ucb_report_user(ucb_ecode code, const char* fmt, ...);

/**
 * @brief Report a warning
 *
 * Prefer to use the macro @ref UCB_WARN
 */
UCB_API void ucb_report_warning(const char* fmt, ...);

/* -------------------------------------------------------------------------- */
/*                           Functions to wrap errno                          */
/* -------------------------------------------------------------------------- */

ucb_ecode ucb_err_wrap_errno(int err);
ucb_ecode ucb_err_get_errno(void);

UCB_API bool ucb_report_errno(int status,
                              const char* UCB_RESTRICT msg,
                              const char* UCB_RESTRICT function);
UCB_API bool ucb_throw_errno(ucb_error** perr, int status, const char* msg);

#ifdef _WIN32

/* -------------------------------------------------------------------------- */
/*                      Functions to wrap Windows errors                      */
/* -------------------------------------------------------------------------- */

UCB_API ucb_ecode ucb_err_wrap_win32(uint32_t err);
UCB_API ucb_ecode ucb_err_get_win32(void);
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

#endif // UCB_ERROR_H
