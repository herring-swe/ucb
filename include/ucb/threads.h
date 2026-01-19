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
#include "task.h"
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

// Use same values as in task.h
#define UCB_THREAD_PRIO_MIN     UCB_TASK_PRIO_LOWEST
#define UCB_THREAD_PRIO_LOW     UCB_TASK_PRIO_LOW
#define UCB_THREAD_PRIO_DEFAULT UCB_TASK_PRIO_NORMAL
#define UCB_THREAD_PRIO_HIGH    UCB_TASK_PRIO_LOWEST
#define UCB_THREAD_PRIO_MAX     UCB_TASK_PRIO_HIGHEST

#define UCB_THREAD_NAME_MAX 15

/**
 * @struct ucb_thread
 * @brief Thread
 *
 * A general purpose thread that can be used to run a task.
 *
 * It can run as a joinable or detached thread.
 *
 * Joinable thread must be joined and/or free'd manually. These can be checked
 * using the thread pointer after the task is started.
 *
 * Detachable threads are asynchronous style and will be automatically freed when the task is
 * done. Therefore, the thread pointer is no longer valid after the task is started.
 */
typedef struct ucb_thread ucb_thread;

/**
 * Get the ID of the current thread
 *
 * @note On linux, we return the kernel thread ID and not a POSIX thread ID.
 * @return the tid
 */
UCB_API ucb_pid ucb_thread_id(void);

/**
 * Yield the current thread, to allow other threads to run.
 *
 * This is added for compatibility with existing logic. Normally it is better to use other
 * synchronization mechanisms, such as conditions or mutexes, which allows the scheduler to know
 * exactly what this thread is waiting for.
 *
 * @note This is a hint to the scheduler and may not be respected by all platforms.
 */
UCB_API void ucb_thread_yield(void);

UCB_API size_t ucb_thread_get_current_stack_size(void);

/**
 * @brief Allocates and initiates a new joinable thread
 * @return the new thread
 */
UCB_API ucb_thread* ucb_thread_new();

/**
 * @brief Allocates and initiates a new detached thread
 * @return UCB_API*
 */
UCB_API ucb_thread* ucb_thread_new_detached();
UCB_API void ucb_thread_free(ucb_thread* thread);

/**
 * @brief Set the thread name.
 *
 * The name will be truncated if longer than UCB_THREAD_NAME_MAX (excluding null).
 * Only valid before the thread is started.
 * @param thread the thread
 * @param name the name
 */
UCB_API void ucb_thread_set_name(ucb_thread* thread, const char* name);
UCB_API const char* ucb_thread_get_name(const ucb_thread* thread);

/**
 * @brief Set the minimum thread stack size.
 *
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
 * @brief Set the thread priority.
 *
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

/**
 * @brief Get the thread ID.
 *
 * This is the same type of ID as return from @ref ucb_thread_id.
 *
 * This call is only valid on a joinable thread after it has started and for as long as it's not
 * started again. Otherwise it will return UCB_PID_INVALID.
 * @param thread the thread
 * @return the thread ID or UCB_PID_INVALID
 */
UCB_API ucb_pid ucb_thread_get_id(ucb_thread* thread);

/**
 * @brief Start a thread with the given task
 *
 * The task will be executed in the thread context.
 * The thread priority is not updated from the task. This must be manually done before
 * this call, if this is expected.
 * This will clear any previous task status code and set the thread to running.
 *
 * @warning If the thread is detached, you must no longer use the ucb_thread pointer as it may free
 * itself at any time.
 * @param thread the thread
 * @param task the task to execute
 * @return true if the thread was started
 */
UCB_API bool ucb_thread_start(ucb_thread* thread, ucb_task task);

/**
 * @brief Wait for the thread to finish
 *
 * Only allowed if the thread is joinable.
 * @param thread the thread
 * @return the status code of the task
 */
UCB_API void ucb_thread_join(ucb_thread* thread);

/**
 * @brief Check if the thread is running
 * @param thread the thread
 * @return true if running
 */
UCB_API bool ucb_thread_is_running(ucb_thread* thread);

/**
 * @brief Get the return status code of the last task finished in the thread.
 *
 * Only valid for joinable threads
 * @param thread the thread
 * @param status pointer to store the status code
 * @return true if the status code was set
 */
UCB_API bool ucb_thread_get_task_status(const ucb_thread* thread, int* status);

#endif // UCB_THREADS_H
