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

#ifndef UCB_ERROR_H
#define UCB_ERROR_H

#include "defines.h"
#include "export.h"
#include "types.h"

// Struct for returning generic data. Introduce new types instead.
typedef struct
{
    ucb_error_t error;
    void* data;
} ucb_result_t;

typedef void (*ucb_errfunc_t)(ucb_error_t error);

UCB_API void ucb_set_last_error(ucb_error_t error);
UCB_API ucb_error_t ucb_get_last_error(void);

/**
 * @brief Report fatal error and exit process
 * This call will call the function set with ucb_set_fatal_errfunc,
 * if any.
 * This function is meant to be only used for unrecoverable errors, such as
 * UCB_ERROR_OUT_OF_MEMORY in memory allocation functions.
 *
 * @param error last error code
 */
UCB_API void ucb_set_fatal_error(ucb_error_t error);
UCB_API const char* ucb_get_error_string(ucb_error_t error);

/**
 * Set the error function to call when an unrecovery error
 * occurs, such as UCB_ERROR_OUT_OF_MEMORY.
 * It can be used to call abort() or exit().
 * Note, that the function may be called from another thread in
 * multi-threaded applications.
 *
 * @param func error function
 * @return previous error function, or NULL
 */
UCB_API ucb_errfunc_t ucb_set_fatal_errfunc(ucb_errfunc_t func);
UCB_API ucb_errfunc_t ucb_get_fatal_errfunc(void);

#endif // UCB_ERROR_H
