/**
 * @file threads.h
 * 
 * This file is part of the UCB project
 * - SPDX-FileCopyrightText: © 2026 Åke Svedin <ake@svedin.org>
 * - SPDX-License-Identifier: MIT
 * 
 * @brief Cross-platform threading
 */

#ifndef UCB_THREADS_H
#define UCB_THREADS_H

#include "error.h"
#include "export.h"
#include "types.h"

#include <stdbool.h>
#include <stddef.h>

#if defined(__cplusplus)
#define UCB_THREAD_LOCAL thread_local
#elif defined(__STDC_VERSION__) && __STDC_VERSION__ >= 202311L
#define UCB_THREAD_LOCAL thread_local
#else
#define UCB_THREAD_LOCAL _Thread_local
#endif

#define UCB_THREAD_PRIO_MIN     -10
#define UCB_THREAD_PRIO_LOW     -5
#define UCB_THREAD_PRIO_DEFAULT 0
#define UCB_THREAD_PRIO_HIGH    5
#define UCB_THREAD_PRIO_MAX     10

/**
 * By default will create a joinable thread.
 */
#define UCB_THREAD_FLAG_DEFAULT  0x00
#define UCB_THREAD_FLAG_JOINABLE 0x01
#define UCB_THREAD_FLAG_DETACHED 0x02

#define UCB_THREAD_NAME_MAX 15

// struct ucb_thread;
typedef struct ucb_thread ucb_thread;

#ifdef __cplusplus
extern "C" {
#endif
typedef void (*ucb_thread_func_t)(void* arg);
typedef void (*ucb_thread_exit_func_t)(void* exit_arg, void* func_arg);
#ifdef __cplusplus
}
#endif

UCB_API ucb_pid ucb_thread_id(void);
UCB_API size_t ucb_thread_get_current_stack_size(void);

UCB_API ucb_thread* ucb_thread_new(int flags);
UCB_API void ucb_thread_free(ucb_thread* thread);

/**
 * Set the thread name.
 * The name will be truncated if longer than UCB_THREAD_NAME_MAX (excluding null).
 * Only valid before the thread is started.
 * @param thread the thread
 * @param name the name
 */
UCB_API void ucb_thread_set_name(ucb_thread* thread, const char* name);
UCB_API const char* ucb_thread_get_name(const ucb_thread* thread);

/**
 * Set the minimum thread stack size.
 * Only valid before the thread is started.
 * The default stack (0) will use the same stack size as the current thread.
 * Any other size may be rounded up to a minimum + any necessary alignment.
 * Recommended minimum and increments are 64 KiB for platform compatibility.
 * @param thread the thread
 * @param stack_size size in bytes
 */
UCB_API void ucb_thread_set_stack_size(ucb_thread* thread, size_t stack_size);
UCB_API size_t ucb_thread_get_stack_size(const ucb_thread* thread);

/**
 * Set the thread priority.
 * Only valid before the thread is started.
 * The priority will be considered only if different from UCB_THREAD_PRIO_DEFAULT and
 * mapped into the platform's priority range.
 * Note, that increasing the priority may not be allowed.
 * Any failure is logged only on debug builds as warnings.
 * @param thread the thread
 * @param priority priority between and including UCB_THREAD_PRIO_LOWEST and UCB_THREAD_PRIO_HIGHEST
 */
UCB_API void ucb_thread_set_priority(ucb_thread* thread, int priority);
UCB_API int ucb_thread_get_priority(const ucb_thread* thread);

UCB_API bool ucb_thread_is_joinable(const ucb_thread* thread);

UCB_API void ucb_thread_set_func(ucb_thread* thread, ucb_thread_func_t func, void* arg);
UCB_API void ucb_thread_set_exit_func(ucb_thread* thread, ucb_thread_exit_func_t func, void* arg);
UCB_API ucb_pid ucb_thread_get_id(ucb_thread* thread);

UCB_API bool ucb_thread_start(ucb_thread* thread);
UCB_API void ucb_thread_join(ucb_thread* thread);
UCB_API bool ucb_thread_is_running(ucb_thread* thread);

#endif // UCB_THREADS_H
