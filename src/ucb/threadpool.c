/**
 * @file threadpool.c
 * This file is part of the UCB project
 * - SPDX-FileCopyrightText: © 2026 Åke Svedin <ake@svedin.org>
 * - SPDX-License-Identifier: MIT
 *
 * @brief Thread pool implementation
 */

#include "ucb/threadpool.h"

#include "ucb/cond.h"
#include "ucb/container/pqueue_private.h"
#include "ucb/error.h"
#include "ucb/memory.h"
#include "ucb/mutex.h"

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
 * Start the worker threads. The pool mutex must be locked before this call.
 *
 * On full success @p pool->running is true and @p pool->stop is false.
 * On partial failure the started workers are signalled to stop and the number
 * of started workers is returned; the caller must reap them with
 * ucb_threadpool_reap_partial.
 *
 * @return the number of workers started
 */
static size_t ucb_threadpool_start_impl(ucb_threadpool* pool)
{
    if (pool->running)
        return pool->num_threads;

    ucb_task task = {
        .func = ucb_threadpool_worker,
        .arg = pool,
    };

    pool->stop = false;
    pool->running = true;
    size_t started = 0;
    for (size_t i = 0; i < pool->num_threads; i++)
    {
        if (!ucb_thread_start(pool->threads[i], task))
            break;
        started++;
    }
    if (started != pool->num_threads)
    {
        pool->stop = true;
        ucb_cond_broadcast(&pool->task_notify);
    }
    return started;
}

/**
 * Reap workers after a partial start. The pool mutex must be locked before
 * this call. The lock is released while joining and the pool is left not
 * running on return, with the lock released.
 */
static void ucb_threadpool_reap_partial(ucb_threadpool* pool, size_t started)
{
    ucb_mutex_unlock(&pool->lock);
    for (size_t i = 0; i < started; i++)
    {
        ucb_thread_join(pool->threads[i]);
    }
    ucb_mutex_lock(&pool->lock);
    pool->running = false;
    pool->stop = false;
    ucb_mutex_unlock(&pool->lock);
}

/**
 * Release all pool resources. Only valid after all members have been
 * successfully initiated.
 */
static void ucb_threadpool_destroy(ucb_threadpool* pool)
{
    if (pool->threads)
    {
        for (size_t i = 0; i < pool->num_threads; i++)
        {
            ucb_thread_free(pool->threads[i]);
        }
        ucb_free(pool->threads);
    }
    ucb_pqueue_release(&pool->tasks);
    ucb_cond_release(&pool->finished_notify);
    ucb_cond_release(&pool->task_notify);
    ucb_mutex_release(&pool->lock);
    ucb_free(pool);
}

ucb_threadpool* ucb_threadpool_new(size_t num_threads)
{
    UCB_VERIFY_ARGS(num_threads > 0);

    ucb_threadpool* pool = ucb_calloc_type(1, ucb_threadpool);
    if (!pool)
        return UCB_NULL;

    pool->num_threads = num_threads;
    pool->num_running = 0;
    pool->running = false;
    pool->stop = false;
    pool->dflt_callback = UCB_NULL;

    ucb_pqueue_args pq_args = {
        .data_free = (ucb_free_func)ucb_task_free,
        .data_clone = UCB_NULL, // Clone manually
    };
    if (!ucb_pqueue_init(&pool->tasks, pq_args))
    {
        ucb_free(pool);
        return UCB_NULL;
    }
    if (!ucb_mutex_init(&pool->lock))
    {
        ucb_pqueue_release(&pool->tasks);
        ucb_free(pool);
        return UCB_NULL;
    }
    if (!ucb_cond_init(&pool->task_notify))
    {
        ucb_mutex_release(&pool->lock);
        ucb_pqueue_release(&pool->tasks);
        ucb_free(pool);
        return UCB_NULL;
    }
    if (!ucb_cond_init(&pool->finished_notify))
    {
        ucb_cond_release(&pool->task_notify);
        ucb_mutex_release(&pool->lock);
        ucb_pqueue_release(&pool->tasks);
        ucb_free(pool);
        return UCB_NULL;
    }

    pool->threads = ucb_calloc_type(num_threads, ucb_thread*);
    if (!pool->threads)
    {
        ucb_threadpool_destroy(pool);
        return UCB_NULL;
    }
    for (size_t i = 0; i < num_threads; i++)
    {
        pool->threads[i] = ucb_thread_new();
        if (!pool->threads[i])
        {
            ucb_threadpool_destroy(pool);
            return UCB_NULL;
        }
    }
    return pool;
}

void ucb_threadpool_free(ucb_threadpool* pool)
{
    if (!pool)
        return;
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
    UCB_VERIFY(!pool->running,
               UCB_ERROR_INVALID_STATE,
               "Cannot change stack size after threadpool has started");

    for (size_t i = 0; i < pool->num_threads; i++)
    {
        ucb_thread_set_stack_size(pool->threads[i], stack_size);
    }
}

void ucb_threadpool_set_thread_priority(ucb_threadpool* pool, int priority)
{
    UCB_VERIFY_ARGS(pool);
    UCB_VERIFY(!pool->running,
               UCB_ERROR_INVALID_STATE,
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
    UCB_VERIFY_ARGS(pool && task);
    UCB_VERIFY(task->func, UCB_ERROR_INVALID_ARG, "Task function must be set");

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

bool ucb_threadpool_start(ucb_threadpool* pool)
{
    UCB_VERIFY_ARGS(pool);

    ucb_mutex_lock(&pool->lock);

    size_t started = ucb_threadpool_start_impl(pool);
    bool ok = (started == pool->num_threads);
    if (!ok)
    {
        ucb_threadpool_reap_partial(pool, started);
    }
    else
    {
        ucb_mutex_unlock(&pool->lock);
    }
    return ok;
}

size_t ucb_threadpool_num_running(ucb_threadpool* pool)
{
    UCB_VERIFY_ARGS(pool);
    ucb_mutex_lock(&pool->lock);
    size_t ret = pool->num_running;
    ucb_mutex_unlock(&pool->lock);
    return ret;
}

size_t ucb_threadpool_num_queued(ucb_threadpool* pool)
{
    UCB_VERIFY_ARGS(pool);
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
        size_t started = ucb_threadpool_start_impl(pool);
        if (started != pool->num_threads)
        {
            // Could not start all workers; leave the tasks queued.
            ucb_threadpool_reap_partial(pool, started);
            return;
        }
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
    if (!pool->running)
    {
        // No workers to wait for; discard any tasks that will never run.
        ucb_pqueue_clear(&pool->tasks);
        ucb_mutex_unlock(&pool->lock);
        return;
    }

    pool->stop = true;
    ucb_cond_broadcast(&pool->task_notify);

    // Wait for tasks already running to finish. Queued tasks are not picked up
    // after the stop signal and are discarded below.
    while (pool->num_running > 0)
    {
        ucb_cond_wait(&pool->finished_notify, &pool->lock);
    }
    ucb_mutex_unlock(&pool->lock);

    // Wait for all threads to finish
    for (size_t i = 0; i < pool->num_threads; i++)
    {
        ucb_thread_join(pool->threads[i]);
    }

    ucb_mutex_lock(&pool->lock);
    pool->running = false;
    pool->stop = false;
    ucb_pqueue_clear(&pool->tasks);
    ucb_mutex_unlock(&pool->lock);
}
