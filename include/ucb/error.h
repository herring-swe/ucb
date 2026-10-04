/**
 * @file error.h
 * This file is part of the UCB project
 * - SPDX-FileCopyrightText: © 2026 Åke Svedin <ake@svedin.org>
 * - SPDX-License-Identifier: MIT
 *
 * @brief Error handling
 *
 * This header defines the public error contract: the severity levels, the
 * @ref ucb_error value and its lifecycle, the report macros and the throw
 * functions used to create errors.
 *
 * UCB handles errors in the following way:
 * - Fatal errors, user errors and warnings:
 *   - Are **reported** to a single error handling function.
 *   - By default these errors are written to stderr, but the program is **not** terminated.
 *   - The user can override the default error handler, with @ref ucb_error_set_func.
 *   - Examples or each type:
 *     - Fatal error: Out of memory. Aborts unless user overrides.
 *     - User error: Invalid arguments or states in input. Always aborts.
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
 * - Internal allocation failures while building an error are reported
 *   as fatal errors. These abort with the default handler but can be overridden.
 *   An error is never silently dropped.
 * - Unknown error levels and codes are not misuse: @ref ucb_error_lvlstr and
 *   @ref ucb_error_codestr return static fallbacks instead of aborting.
 * - Reentrant reports made from an error handler are suppressed to avoid
 *   unbounded recursion.
 *
 * The reporting and throwing machinery behind these guarantees lives in the
 * library's private headers and is not installed. @ref ucb_error_print is the
 * printer a custom handler can reuse for its default output; the internal
 * dispatcher that selects between the handler and the default output is not
 * public.
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
    const char* msg; ///< Error message.
    ucb_ecode code;  ///< Error code.
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
 * @brief Print an error using the default output
 *
 * This is the printer a custom error handler can reuse. It writes to stderr in
 * a fixed severity format.
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
 * Aborts if no error handler is set or a custom error handler does it.
 */
#define UCB_FATAL(code, fmt, ...) ucb_report_fatal((code), "%s: " fmt, __func__, ##__VA_ARGS__)

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

/**
 * @brief Verify an expression and report invalid arguments if it fails
 */
#define UCB_VERIFY_ARGS(expr) UCB_VERIFY(expr, UCB_ERROR_INVALID_ARG, "Invalid arguments")

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

UCB_API ucb_ecode ucb_err_wrap_errno(int err);

#ifdef _WIN32

/* -------------------------------------------------------------------------- */
/*                      Functions to wrap Windows errors                      */
/* -------------------------------------------------------------------------- */

UCB_API ucb_ecode ucb_err_wrap_win32(uint32_t err);

#endif

#endif // UCB_ERROR_H
