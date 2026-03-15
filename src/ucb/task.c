/**
 * @file task.c
 * This file is part of the UCB project
 * - SPDX-FileCopyrightText: © 2026 Åke Svedin <ake@svedin.org>
 * - SPDX-License-Identifier: MIT
 *
 * @brief Task implementation
 */

#include "ucb/task.h"

#include "ucb/debug.h"
#include "ucb/error.h"
#include "ucb/memory.h"

#include <limits.h>

ucb_task* ucb_task_new(ucb_task_func func, void* arg)
{
    ucb_task* task = ucb_malloc_type(1, ucb_task);
    if (task)
    {
        ucb_task_init(task, func, arg);
    }
    return task;
}

void ucb_task_init(ucb_task* task, ucb_task_func func, void* arg)
{
    UCB_VERIFY_ARGS(task);
    task->func = func;
    task->arg = arg;
    task->callback = UCB_NULL;
    task->priority = UCB_TASK_PRIO_NORMAL;
}

void ucb_task_release(ucb_task* task)
{
    UCB_VERIFY_ARGS(task);
    task->func = UCB_NULL;
    task->arg = UCB_NULL;
    task->callback = UCB_NULL;
    task->priority = UCB_TASK_PRIO_NORMAL;
}

void ucb_task_free(ucb_task* task)
{
#ifndef NDEBUG
    ucb_task_release(task);
#else
    UCB_VERIFY_ARGS(task);
#endif
    ucb_free(task);
}

bool ucb_task_validate(const ucb_task* task)
{
    UCB_VERIFY(task->priority >= UCB_TASK_PRIO_LOWEST && task->priority <= UCB_TASK_PRIO_HIGHEST,
               UCB_ERROR_INVALID_ARG, "Invalid task priority");
    return true;
}

void ucb_task_copy(ucb_task* dst, const ucb_task* src)
{
    UCB_VERIFY_ARGS(dst && src);
    dst->func = src->func;
    dst->arg = src->arg;
    dst->callback = src->callback;
    dst->priority = src->priority;
}

ucb_task* ucb_task_clone(const ucb_task* src)
{
    ucb_task* task = UCB_NULL;
    if (src)
    {
        task = ucb_malloc_type(1, ucb_task);
        if (task)
            ucb_task_copy(task, src);
    }
    return task;
}

int ucb_task_cmp_prio(const ucb_task* a, const ucb_task* b)
{
    // Use assert to force abort in debug build.
    UCB_ASSERT(a && b, UCB_ERROR_INVALID_ARG, "NULL arguments in comparison function");
    return a->priority - b->priority;
}

void ucb_task_set_func(ucb_task* task, ucb_task_func func, void* arg)
{
    UCB_VERIFY_ARGS(task && func);
    task->func = func;
    task->arg = arg;
}

void ucb_task_set_callback(ucb_task* task, ucb_task_callback callback)
{
    UCB_VERIFY_ARGS(task);
    task->callback = callback;
}

void ucb_task_set_priority(ucb_task* task, int priority)
{
    UCB_VERIFY_ARGS(task);
    UCB_VERIFY(priority >= UCB_TASK_PRIO_LOWEST && priority <= UCB_TASK_PRIO_HIGHEST,
               UCB_ERROR_INVALID_ARG, "Invalid priority");
    task->priority = priority;
}

int ucb_task_run(ucb_task* task)
{
    UCB_VERIFY_ARGS(task && task->func);
    int status = task->func(task->arg);
    if (task->callback)
    {
        task->callback(task->arg, status);
    }
    return status;
}
