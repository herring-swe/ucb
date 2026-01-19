/**
 * @file threadpool.c
 * This file is part of the UCB project
 * - SPDX-FileCopyrightText: © 2026 Åke Svedin <ake@svedin.org>
 * - SPDX-License-Identifier: MIT
 *
 * @brief Thread pool implementation
 */

#include "ucb/threadpool.h"

#include "ucb/cond_private.h"
#include "ucb/mutex_private.h"

#include "ucb/container/pqueue_private.h"

#include "ucb/error.h"
#include "ucb/memory.h"

struct ucb_threadpool
{
    ucb_thread** threads;
    ucb_pqueue tasks;
    ucb_mutex lock;
    ucb_cond task_notify;     // For worker threads (new tasks)
    ucb_cond finished_notify; // For wait_all (task completion)
    size_t num_threads;
    size_t num_running;
    bool running;
    bool stop;

    ucb_task_callback dflt_callback;
};

static int ucb_threadpool_worker(void* arg)
{
    ucb_threadpool* pool = (ucb_threadpool*)arg;
    while (1)
    {
        ucb_task* task = NULL;

        // Lock the pool mutex
        ucb_mutex_lock(&pool->lock);

        // Wait for a task or stop signal
        while (ucb_pqueue_empty(&pool->tasks) && !pool->stop)
        {
            ucb_cond_wait(&pool->task_notify, &pool->lock);
        }

        // Exit if the pool is stopping
        if (pool->stop)
        {
            ucb_mutex_unlock(&pool->lock);
            return 1;
        }

        // Get the highest-priority task
        task = (ucb_task*)ucb_pqueue_pop(&pool->tasks);
        pool->num_running++;
        ucb_mutex_unlock(&pool->lock);

        // Execute the task
        if (task)
        {
            ucb_task_run(task); // Ignore returned status
            ucb_task_free(task);
        }

        // Update running count
        ucb_mutex_lock(&pool->lock);
        pool->num_running--;
        ucb_cond_signal(&pool->finished_notify);
        ucb_mutex_unlock(&pool->lock);
    }
    return 0;
}

/**
 * Mutex must be locked before this call
 */
static void ucb_threadpool_start_impl(ucb_threadpool* pool)
{
    if (!pool->running)
    {
        ucb_task task = {
            .func = ucb_threadpool_worker,
            .arg  = pool,
        };

        pool->stop = false;
        pool->running = true;
        for (size_t i = 0; i < pool->num_threads; i++)
        {
            ucb_thread_start(pool->threads[i], task);
        }
    }
}

ucb_threadpool* ucb_threadpool_new(size_t num_threads)
{
    UCB_VERIFY_ARGS_RET(num_threads > 0, UCB_NULL);

    ucb_threadpool* pool = ucb_malloc_type(1, ucb_threadpool);
    if (pool)
    {
        pool->num_threads   = num_threads;
        pool->num_running   = 0;
        pool->running       = false;
        pool->stop          = false;
        pool->dflt_callback = UCB_NULL;

        ucb_pqueue_args pq_args = {
            .data_free  = ucb_task_free,
            .data_clone = UCB_NULL, // Clone manually
        };
        ucb_pqueue_init(&pool->tasks, pq_args);

        ucb_mutex_init(&pool->lock);
        ucb_cond_init(&pool->task_notify);
        ucb_cond_init(&pool->finished_notify);

        pool->threads = ucb_malloc_type(num_threads, ucb_thread*);
        for (size_t i = 0; i < num_threads; i++)
        {
            pool->threads[i] = ucb_thread_new();
        }
    }
    return pool;
}

void ucb_threadpool_free(ucb_threadpool* pool)
{
    UCB_VERIFY_ARGS(pool);
    ucb_threadpool_join(pool);

    ucb_pqueue_release(&pool->tasks);
    ucb_mutex_release(&pool->lock);
    ucb_cond_release(&pool->task_notify);
    ucb_cond_release(&pool->finished_notify);
    for (size_t i = 0; i < pool->num_threads; i++)
    {
        ucb_thread_free(pool->threads[i]);
    }
    ucb_free(pool->threads);
    ucb_free(pool);
}

void ucb_threadpool_set_thread_stack_size(ucb_threadpool* pool, size_t stack_size)
{
    UCB_VERIFY_ARGS(pool);
    UCB_VERIFY(!pool->running, UCB_ERROR_INVALID_STATE,
               "Cannot change stack size after threadpool has started");

    for (size_t i = 0; i < pool->num_threads; i++)
    {
        ucb_thread_set_stack_size(pool->threads[i], stack_size);
    }
}

void ucb_threadpool_set_thread_priority(ucb_threadpool* pool, int priority)
{
    UCB_VERIFY_ARGS(pool);
    UCB_VERIFY(!pool->running, UCB_ERROR_INVALID_STATE,
               "Cannot change priority after threadpool has started");

    for (size_t i = 0; i < pool->num_threads; i++)
    {
        ucb_thread_set_priority(pool->threads[i], priority);
    }
}

void ucb_threadpool_set_default_callback(ucb_threadpool* pool, ucb_task_callback callback)
{
    UCB_VERIFY_ARGS(pool);
    pool->dflt_callback = callback;
}

bool ucb_threadpool_add_task(ucb_threadpool* pool, const ucb_task* task)
{
    UCB_VERIFY_ARGS_RET(pool && task, false);
    UCB_VERIFY_RET(task->func, UCB_ERROR_INVALID_ARG, "Task function must be set", false);

    ucb_mutex_lock(&pool->lock);

    ucb_task* ctask = ucb_task_clone(task);
    if (!ctask)
    {
        ucb_mutex_unlock(&pool->lock);
        return false;
    }
    if (!ctask->callback && pool->dflt_callback)
        ctask->callback = pool->dflt_callback;
    bool success = ucb_pqueue_push(&pool->tasks, ctask, ctask->priority) != SIZE_MAX;
    if (success && pool->running)
    {
        ucb_cond_signal(&pool->task_notify);
    }
    ucb_mutex_unlock(&pool->lock);
    return success;
}

void ucb_threadpool_start(ucb_threadpool* pool)
{
    UCB_VERIFY_ARGS(pool);

    ucb_mutex_lock(&pool->lock);

    ucb_threadpool_start_impl(pool);

    ucb_mutex_unlock(&pool->lock);
}

size_t ucb_threadpool_num_running(ucb_threadpool* pool)
{
    UCB_VERIFY_ARGS_RET(pool, 0);
    ucb_mutex_lock(&pool->lock);
    size_t ret = pool->num_running;
    ucb_mutex_unlock(&pool->lock);
    return ret;
}

size_t ucb_threadpool_num_queued(ucb_threadpool* pool)
{
    UCB_VERIFY_ARGS_RET(pool, 0);
    ucb_mutex_lock(&pool->lock);
    size_t ret = ucb_pqueue_size(&pool->tasks);
    ucb_mutex_unlock(&pool->lock);
    return ret;
}

void ucb_threadpool_wait_all(ucb_threadpool* pool)
{
    UCB_VERIFY_ARGS(pool);

    ucb_mutex_lock(&pool->lock);
    if (!pool->running && !ucb_pqueue_empty(&pool->tasks))
    {
        ucb_threadpool_start_impl(pool);
    }

    while (pool->num_running > 0 || !ucb_pqueue_empty(&pool->tasks))
    {
        ucb_cond_wait(&pool->finished_notify, &pool->lock);
    }
    ucb_mutex_unlock(&pool->lock);
}

void ucb_threadpool_join(ucb_threadpool* pool)
{
    UCB_VERIFY_ARGS(pool);

    ucb_mutex_lock(&pool->lock);
    pool->stop = true;
    ucb_cond_broadcast(&pool->task_notify);

    while (pool->num_running > 0 || !ucb_pqueue_empty(&pool->tasks))
    {
        ucb_cond_wait(&pool->finished_notify, &pool->lock);
    }
    ucb_mutex_unlock(&pool->lock);

    // Wait for all threads to finish
    for (size_t i = 0; i < pool->num_threads; i++)
    {
        ucb_thread_join(pool->threads[i]);
    }
}
