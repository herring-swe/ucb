/**
 * @file task.h
 * This file is part of the UCB project
 * - SPDX-FileCopyrightText: © 2026 Åke Svedin <ake@svedin.org>
 * - SPDX-License-Identifier: MIT
 *
 * @brief Task for thread pool and other async operations
 */

#ifndef UCB_TASK_H
#define UCB_TASK_H

#include "export.h"

#include <stdbool.h>

/**
 * @brief Task user-defined function
 * Takes an optional argument pointer and returns a user-specific status code.
 */
typedef int (*ucb_task_func)(void* arg);

/**
 * @brief Task callback function
 * Called after a task has been run, with the same argument and the status code returned.
 */
typedef void (*ucb_task_callback)(void* arg, int status);

#define UCB_TASK_PRIO_LOWEST -10
#define UCB_TASK_PRIO_LOW -5
#define UCB_TASK_PRIO_NORMAL 0
#define UCB_TASK_PRIO_HIGH 5
#define UCB_TASK_PRIO_HIGHEST 10

/**
 * @struct ucb_task
 * @brief A general task
 *
 * To be used with @ref ucb_thread, @ref ucb_threadpool or similar
 * functions.
 */
typedef struct ucb_task
{
    ucb_task_func func;
    void* arg;
    ucb_task_callback callback;
    int priority;
} ucb_task;

/**
 * @brief Helper function for stack initialization of task
 *
 * Only required options as arguments
 * @param func the function to run
 * @return a task
 */
static inline ucb_task ucb_task_make(ucb_task_func func)
{
    ucb_task task = {0};
    task.func = func;
    return task;
}

/**
 * @brief Allocates a new task and initialize it with arguments
 * @param func optional function (UCB_NULL okay)
 * @param arg optional argument (UCB_NULL okay)
 * @return new task pointer
 */
UCB_API ucb_task* ucb_task_new(ucb_task_func func, void* arg);

/**
 * @brief Initialize task with arguments
 * For a task allocated on the stack
 * @param task the task
 * @param func optional function (UCB_NULL okay)
 * @param arg optional argument (UCB_NULL okay)
 */
UCB_API void ucb_task_init(ucb_task* task, ucb_task_func func, void* arg);

/**
 * @brief Release a task previously initiated with ucb_task_init
 * Currently this only zeroes the arguments
 * @param task the task
 */
UCB_API void ucb_task_release(ucb_task* task);

/**
 * @brief Free a task previously allocated with ucb_task_new
 * @param task the task
 */
UCB_API void ucb_task_free(ucb_task* task);

/**
 * Validate and report user error if faulty.
 * To be used as validator when it's used as an argument, as the object
 * is mainly to be constructed on the stack without setter functions.
 * @param task the task
 * @return true if valid
 */
UCB_API bool ucb_task_validate(const ucb_task* task);

/**
 * @brief Copy a task
 * @param dst task to set
 * @param src task to copy from
 */
UCB_API void ucb_task_copy(ucb_task* dst, const ucb_task* src);

/**
 * @brief Clone a task
 * @param src task to clone
 * @return a new task identical to src
 */
UCB_API ucb_task* ucb_task_clone(const ucb_task* src);

/**
 * @brief Compare two tasks based on priority
 * @param a first task
 * @param b second task
 * @return -1 if a < b, 0 if a == b, 1 if a > b
 */
UCB_API int ucb_task_cmp_prio(const ucb_task* a, const ucb_task* b);

/**
 * @brief Set the function and argument of a task
 * @param task the task
 * @param func the function
 * @param arg optional argument (UCB_NULL okay)
 */
UCB_API void ucb_task_set_func(ucb_task* task, ucb_task_func func, void* arg);

/**
 * @brief Set the callback of a task
 * @param task the task
 * @param callback the callback (UCB_NULL okay)
 */
UCB_API void ucb_task_set_callback(ucb_task* task, ucb_task_callback callback);

/**
 * @brief Set the priority of a task
 * Priority must be in range [UCB_TASK_PRIORITY_MIN, UCB_TASK_PRIORITY_MAX]
 * @param task the task
 * @param priority the priority
 */
UCB_API void ucb_task_set_priority(ucb_task* task, int priority);

/**
 * @brief Run in the current thread
 * Will run the task function and then call the callback function, if set.
 * func must be set before calling this function.
 * In the event of invalid arguments, the function will report the user error
 * and return INT_MAX.
 * @param task task to run
 * @return returned status of the task
 */
UCB_API int ucb_task_run(ucb_task* task);

#endif // UCB_TASK_H
