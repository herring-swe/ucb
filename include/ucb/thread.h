/**
 * This file is part of the UCB project
 * SPDX-FileCopyrightText: © 2025 Åke Svedin <ake@svedin.org>
 * SPDX-License-Identifier: MIT
 *
 * @brief Baisc threading support
 */

#ifndef UCB_THREAD_H
#define UCB_THREAD_H

#include "error.h"
#include "export.h"
#include "types.h"

#include <stdbool.h>

// struct ucb_thread;
typedef struct ucb_thread ucb_thread_t;

typedef void (*ucb_thread_func_t)(void* arg);
typedef void (*ucb_thread_exit_func_t)(void* exit_arg, void* func_arg);

UCB_API ucb_pid_t ucb_thread_id();

/* ----------------- UCB_API ucb_thread_t* ucb_thread_new(); ---------------- */
UCB_API ucb_error_t ucb_thread_init(ucb_thread_t* thread);
UCB_API void ucb_thread_release(ucb_thread_t* thread);
UCB_API void ucb_thread_free(ucb_thread_t* thread);

UCB_API ucb_error_t ucb_thread_set_func(ucb_thread_t* thread, ucb_thread_func_t func, void* arg);
UCB_API ucb_error_t ucb_thread_set_exit_func(ucb_thread_t* thread, ucb_thread_exit_func_t func,
                                             void* arg);
UCB_API ucb_pid_t ucb_thread_get_id(ucb_thread_t* thread);

UCB_API ucb_error_t ucb_thread_start(ucb_thread_t* thread, ucb_thread_func_t func, void* arg);
UCB_API ucb_error_t ucb_thread_join(ucb_thread_t* thread);
UCB_API ucb_error_t ucb_thread_detach(ucb_thread_t* thread);
UCB_API bool ucb_thread_is_running(ucb_thread_t* thread);

#endif // UCB_THREAD_H
