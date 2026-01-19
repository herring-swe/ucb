/**
 * @file threadpool.h
 * This file is part of the UCB project
 * - SPDX-FileCopyrightText: © 2026 Åke Svedin <ake@svedin.org>
 * - SPDX-License-Identifier: MIT
 *
 * @brief Cross-platform thread pool
 */

#ifndef UCB_THREADPOOL_H
#define UCB_THREADPOOL_H

#include "export.h"
#include "threads.h"

#include <stdbool.h>

/**
 * @struct ucb_threadpool
 * @brief Thread pool
 *
 * Allocates a number of reusable threads that can be used to run tasks.
 * The tasks are added to a queue and the threads will pick them up when they are free.
 *
 * It is backed by a priority queue, to allow certain tasks to run as soon as possible.
 * @see ucb_task->priority
 *
 * The underlying threads can be configured only before the threadpool is started.
 * See @ref ucb_threadpool_set_thread_stack_size and @ref ucb_threadpool_set_thread_priority.
 *
 * The threadpool will not start until @ref ucb_threadpool_start is called. After which
 * it will process the queue. It will run until @ref ucb_threadpool_join or until it's
 * free'd.
 */
typedef struct ucb_threadpool ucb_threadpool;

/**
 * @brief Allocates and initiates a new threadpool
 * @param num_threads number of threads to use initially
 * @return new threadpool pointer
 */
UCB_API ucb_threadpool* ucb_threadpool_new(size_t num_threads);

/**
 * @brief Free's the threadpool
 *
 * Will stop the pool if it's running
 * @param pool the pool
 */
UCB_API void ucb_threadpool_free(ucb_threadpool* pool);

/**
 * @brief Set the stack size for the threads in the pool
 *
 * Must be called before the threadpool is started
 * @see ucb_thread_set_stack_size
 * @param pool the pool
 * @param stack_size the stack size
 */
UCB_API void ucb_threadpool_set_thread_stack_size(ucb_threadpool* pool, size_t stack_size);

/**
 * @brief Set the priority for the threads in the pool
 *
 * Must be called before the threadpool is started
 * @see ucb_thread_set_priority
 * @param pool the pool
 * @param priority the priority
 */
UCB_API void ucb_threadpool_set_thread_priority(ucb_threadpool* pool, int priority);

/**
 * @brief Set the default callback function for tasks in the threadpool
 *
 * This applies only to new tasks that do not have a callback function set already.
 * @param pool the pool
 * @param callback the callback
 */
UCB_API void ucb_threadpool_set_default_callback(ucb_threadpool* pool, ucb_task_callback callback);

/**
 * @brief Add a task to the queue
 *
 * It will be added to the queue according to it's priority
 * The task object will be copied to the threadpool.
 * @param pool the threadpool
 * @param task the task to add
 */
UCB_API bool ucb_threadpool_add_task(ucb_threadpool* pool, const ucb_task* task);

/**
 * @brief Start the threadpool
 *
 * This will start the threadpool for processing. Tasks can be queued
 * before or after it has started.
 *
 * If the threadpool is already running, this function does nothing.
 * @param pool the pool
 */
UCB_API void ucb_threadpool_start(ucb_threadpool* pool);

/**
 * @brief Get the number of running tasks
 * @param pool the pool
 * @return number of tasks running
 */
UCB_API size_t ucb_threadpool_num_running(ucb_threadpool* pool);

/**
 * @brief Get the number of queued tasks
 * @param pool the pool
 * @return number of queued tasks
 */
UCB_API size_t ucb_threadpool_num_queued(ucb_threadpool* pool);

/**
 * @brief Wait for all tasks to finish
 *
 * This will start a non-running threadpool if it has queued tasks.
 *
 * After this call, the threadpool is still running and new tasks can be added.
 * @param pool the pool
 */
UCB_API void ucb_threadpool_wait_all(ucb_threadpool* pool);

/**
 * @brief Signal a stop and wait for all threads to finish
 *
 * This will not start a non-running threadpool if it has queued tasks.
 * Instead it will set a stop flag, so any running threads will not pick up new tasks.
 *
 * Then it will wait for all threads to finish.
 *
 * After this, the pool has to be restarted or free'd.
 * @param pool the pool
 */
UCB_API void ucb_threadpool_join(ucb_threadpool* pool);

#endif // UCB_THREADPOOL_H
