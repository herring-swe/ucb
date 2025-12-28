/**
 * This file is part of the UCB project
 * SPDX-FileCopyrightText: © 2025 Åke Svedin <ake@svedin.org>
 * SPDX-License-Identifier: MIT
 *
 * @brief
 */

#include "ucb/thread.h"

#include "ucb/defines.h"
#include "ucb/errcodes.h"
#include "ucb/memory.h"

#if defined(_WIN32)
#define WIN32_LEAN_AND_MEAN
#include <Windows.h>

#include <process.h>
#else
#include <pthread.h>
#include <unistd.h>
#include <sys/types.h>
#endif

/**
 * TODO:
 * - Add support for stack size and priority
 * - Add support for thread names
 * - Add support for thread local storage
 * - Add support for thread pools
 */

struct ucb_thread
{
#if defined(_WIN32)
    HANDLE handle;
#else
    pthread_t handle;
#endif
    ucb_thread_func_t func;
    void* arg;
    ucb_thread_exit_func_t exit_func;
    void* exit_arg;
    bool running;
    // TODO: Support stack size and priority
};

ucb_pid_t ucb_thread_id(void)
{
#ifdef _WIN32
    return GetCurrentThreadId();
#else
    return gettid();
#endif
}

#if defined(_WIN32)
static unsigned __stdcall win_thread_wrapper(void* arg)
{
    ucb_thread_t* th = (ucb_thread_t*)arg;
    th->func(th->arg);
    th->running = false;
    if (th->exit_func)
    {
        th->exit_func(th->exit_arg, th->arg);
    }
    return 0;
}
#else
static void* posix_thread_wrapper(void* arg)
{
    ucb_thread_t* th = (ucb_thread_t*)arg;
    th->func(th->arg);
    th->running = false;
    if (th->exit_func)
    {
        th->exit_func(th->exit_arg, th->arg);
    }
    return NULL;
}
#endif

ucb_thread_t* ucb_thread_new()
{
    ucb_thread_t* th = ucb_malloc_type(1, ucb_thread_t);
    if (!th)
        return UCB_NULL;

    th->handle  = 0;
    th->func    = UCB_NULL;
    th->arg     = UCB_NULL;
    th->running = false;
    return th;
}

ucb_ecode ucb_thread_init(ucb_thread_t* th)
{
    if (!th)
        return UCB_ERROR_INVALID_ARG;
    th->handle  = 0;
    th->func    = UCB_NULL;
    th->arg     = UCB_NULL;
    th->running = false;
    return UCB_OK;
}

void ucb_thread_release(ucb_thread_t* th)
{
    if (th->running)
    {
        ucb_thread_join(th);
    }
}

void ucb_thread_free(ucb_thread_t* th)
{
    if (!th)
        return;
    ucb_thread_release(th);
    ucb_free(th);
}

ucb_ecode ucb_thread_start(ucb_thread_t* th, ucb_thread_func_t func, void* arg)
{
    // TODO: Nedds error checking
    if (th->running)
    {
        return UCB_ERROR_THREAD_BUSY;
    }
    th->func    = func;
    th->arg     = arg;
    th->running = true;
#if defined(_WIN32)
    unsigned threadaddr;
    th->handle = (HANDLE)_beginthreadex(NULL, 0, win_thread_wrapper, th, 0, &threadaddr);
    if (!th->handle) {
        th->running = false;
        return -1;
    }
#else
    int ret = pthread_create(&th->handle, NULL, posix_thread_wrapper, th);
    if (ret != 0) {
        th->running = false;
        return ret;
    }
#endif
    return 0;
}

ucb_ecode ucb_thread_join(ucb_thread_t* th)
{
    // TODO: Nedds error checking
    if (!th->running)
    {
        return UCB_OK;
    }
#if defined(_WIN32)
    WaitForSingleObject(th->handle, INFINITE);
    CloseHandle(th->handle);
#else
    int ret = pthread_join(th->handle, NULL);
    if (ret != 0)
    {
        return ret;
    }
#endif
    th->running = false;
    return UCB_OK;
}

bool ucb_thread_is_running(ucb_thread_t* th)
{
    return th->running;
}
