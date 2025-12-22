/**
 * This file is part of the UCB project
 * SPDX-FileCopyrightText: © 2025 Åke Svedin <ake@svedin.org>
 * SPDX-License-Identifier: MIT
 *
 * @brief Error handling definition
 *
 * Quote: "If the only possible errors are programmer errors, don't return an error code, use
 * asserts inside the function." - A.Shelly, stackoverflow.com.
 *
 * This, our philosophy is:
 *  - Prefer returning ucb_error_t
 *  - For simple functions, the basic return type is used:
 *    - For pointers, return UCB_NULL on error.
 *    - For arithmetic types, document which value may denote an error.
 *    - For bool, return maybe /s
 *
 * - Always check arguments (UCB_ERROR_INVALID_ARG)
 * - Always catch errors from system and internally
 * - Always set last error on error
 * - No need to clear errors
 * - If error handling becomes difficult, refactor to use ucb_error_t
 */

#ifndef UCB_ERROR_CODES_H
#define UCB_ERROR_CODES_H

enum ucb_error
{
    UCB_OK = 0,                  //
    UCB_ERROR_FALSE,             // For boolean type methods
    UCB_ERROR_INVALID_UTF8,      // Invalid UTF-8 sequence
    UCB_ERROR_INVALID_CODEPOINT, //
    UCB_ERROR_INTERNAL,          // Internal error, should not happen
    UCB_ERROR_NOT_IMPLEMENTED,
    UCB_ERROR_BUFFER,
    UCB_ERROR_LOCKED,
    UCB_ERROR_MAX_SIZE,
    /**
     * @brief FATAL ERROR: Attempt to free memory that was not allocated by ucb.
     * May also catch double free's. This is only enabled in debug builds
     */
    UCB_ERROR_INVALID_ALLOC,
    UCB_ERROR_THREAD_BUSY,      // Thread already running

    // System errors
    UCB_ERROR_UNKNOWN,       //
    UCB_ERROR_UNSUPPORTED,   //
    UCB_ERROR_INVALID_ARG,   // Invalid argument (EINVAL)
    UCB_ERROR_NOT_FOUND,     //
    UCB_ERROR_ACCESS_DENIED, //
    UCB_ERROR_IO,            // Generic IO error
    UCB_ERROR_OUT_OF_MEMORY, // Out of memory (ENOMEM). May be a fatal error, but not necessary.
    UCB_ERROR_RANGE,         // Numerical result out of range (ERANGE)
    UCB_ERROR_ENCODING,      // Invalid utf-8 or similar
};

#endif // UCB_ERROR_CODES_H
