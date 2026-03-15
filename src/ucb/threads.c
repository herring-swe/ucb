/**
 * @file threads.c
 *
 * This file is part of the UCB project
 * - SPDX-FileCopyrightText: © 2026 Åke Svedin <ake@svedin.org>
 * - SPDX-License-Identifier: MIT
 *
 * @brief Cross-platform threading implementation
 */

#include "ucb/threads.h"

#include "ucb/cstring.h"
#include "ucb/debug.h"
#include "ucb/defines.h"
#include "ucb/error.h"
#include "ucb/memory.h"
#include "ucb/sys_private.h"

#if defined(_WIN32)

#define WIN32_LEAN_AND_MEAN
#include <Windows.h>

#include <process.h>

#else // POSIX

#include <limits.h>
#include <pthread.h>
#include <sched.h>
#include <unistd.h>
#if !__GLIBC_PREREQ(2, 30)
#define IMPL_GETTID
#include <sys/syscall.h>
#endif

// Use only for threads with priority
#define PTHREAD_SCHED SCHED_OTHER
#define NICE_LOWEST 19
#define NICE_HIGHEST -20

#endif

/**
 * By default will create a joinable thread.
 */
#define UCB_THREAD_FLAG_JOINABLE 0x01
#define UCB_THREAD_FLAG_DETACHED 0x02

struct ucb_thread
{
#if defined(_WIN32)
    HANDLE handle;
#else
    pthread_t handle;
#endif
    ucb_task task;
    char* name;
    size_t stack_size;
    int flags;
    int priority;
    ucb_pid id;
    bool running;
    int status[2]; // First is existence (int bool), second is the status
};

size_t ucb_thread_get_current_stack_size(void)
{
#ifdef _WIN32
    ULONG_PTR low, high;
    GetCurrentThreadStackLimits(&low, &high);
    return (size_t)(high - low);
#else
    int code;
    UCB_UNUSED(code);
    pthread_attr_t attr;
    code = pthread_getattr_np(pthread_self(), &attr);
    UCB_ASSERT_STATUS(code, "Failed to get thread attributes");
    size_t stack_size;
    code = pthread_attr_getstacksize(&attr, &stack_size);
    UCB_ASSERT_STATUS(code, "Failed to get thread stack size");
    code = pthread_attr_destroy(&attr);
    UCB_ASSERT_STATUS(code, "Failed to destroy thread attributes");
    return stack_size;
#endif
}

static inline int map_prio(int val, int lowest, int highest)
{
    double fraction =
        (val - UCB_THREAD_PRIO_MIN) / (double)(UCB_THREAD_PRIO_MAX - UCB_THREAD_PRIO_MIN);
    return (int)(.5 + fraction * (highest - lowest) + lowest);
}

#ifdef _WIN32 // OS-specific thread priority handling

/**
 * Must be called after creating the thread
 */
static void set_thread_prio_win32(ucb_thread* th)
{
    if (th->priority == UCB_THREAD_PRIO_DEFAULT)
        return;

    int prio = map_prio(th->priority, THREAD_PRIORITY_LOWEST, THREAD_PRIORITY_HIGHEST);
    if (!SetThreadPriority(th->handle, prio))
    {
        UCB_WARN("Failed to set priority for thread");
    }
}

#else // POSIX

/**
 * Must be called before starting thread
 */
static void set_thread_prio_posix(ucb_thread* th, pthread_attr_t* attr)
{
    if (th->priority == UCB_THREAD_PRIO_DEFAULT)
        return;

    int min_prio = sched_get_priority_min(PTHREAD_SCHED);
    int max_prio = sched_get_priority_max(PTHREAD_SCHED);
    if (min_prio == -1 || max_prio == -1)
    {
        UCB_DPRINT("sched_get_priority_min/max failed, using 0\n");
        min_prio = 0;
        max_prio = 0;
    }

    int prio = map_prio(th->priority, min_prio, max_prio);
    struct sched_param param = {.sched_priority = prio};
    pthread_attr_setschedpolicy(attr, SCHED_OTHER);
    bool success = pthread_attr_setschedparam(attr, &param) == 0;

    if (!success)
    {
        UCB_WARN("Failed to set priority for thread");
    }
}

#endif // OS-specific thread priority handling

static void set_thread_name(ucb_thread* th)
{
#ifdef _WIN32
    wchar_t wname[64];
    int code = MultiByteToWideChar(CP_UTF8, 0, th->name, -1, wname, 64);
    UCB_UNUSED(code);
    UCB_ASSERT_WIN32(code == 0 ? GetLastError() : ERROR_SUCCESS,
                     "Could not convert thread name to UTF-16");
    HRESULT hr = SetThreadDescription(th->handle, wname);
    UCB_UNUSED(hr);
    UCB_ASSERT(SUCCEEDED(hr), UCB_ERRSYS_UNKNOWN, "Failed to set thread name");
#else
    int code = pthread_setname_np(th->handle, th->name);
    UCB_UNUSED(code);
    UCB_ASSERT_ERRNO(code, "Failed to set thread name");
#endif
}

static void ucb_thread_free_impl(ucb_thread* th)
{
    if (th->name)
        ucb_free(th->name);
    ucb_free(th);
}

#if defined(_WIN32)
static unsigned __stdcall win_thread_wrapper(void* arg)
{
    ucb_thread* th = (ucb_thread*)arg;
    th->id = ucb_thread_id();
    if (th->name)
        set_thread_name(th);
    if (th->priority != UCB_THREAD_PRIO_DEFAULT)
        set_thread_prio_win32(th);

    th->status[1] = ucb_task_run(&th->task);
    th->status[0] = 1;
    th->running = false;

    if (th->flags & UCB_THREAD_FLAG_DETACHED)
        ucb_thread_free_impl(th);
    return 0;
}
#else
static void* posix_thread_wrapper(void* arg)
{
    ucb_thread* th = (ucb_thread*)arg;
    th->id = ucb_thread_id();
    if (th->name)
        set_thread_name(th);

    th->status[1] = ucb_task_run(&th->task);
    th->status[0] = 1;
    th->running = false;

    if (th->flags & UCB_THREAD_FLAG_DETACHED)
        ucb_thread_free_impl(th);
    return NULL;
}
#endif

static inline ucb_thread* ucb_thread_new_flags(int flags)
{
    ucb_thread* th = ucb_calloc_type(1, ucb_thread);
    if (th)
    {
        th->flags = flags;
        th->running = false;
        th->status[0] = 0;
        th->id = UCB_PID_INVALID;
#ifdef _WIN32
        th->handle = INVALID_HANDLE_VALUE;
#else
        th->handle = 0;
#endif
    }
    return th;
}

ucb_pid ucb_thread_id(void)
{
#ifdef _WIN32
    return GetCurrentThreadId();
#elif defined(IMPL_GETTID)
    return syscall(SYS_gettid);
#else
    return gettid();
#endif
}

void ucb_thread_yield(void)
{
#ifdef _WIN32
    SwitchToThread();
#else
    sched_yield();
#endif
}

ucb_thread* ucb_thread_new()
{
    return ucb_thread_new_flags(UCB_THREAD_FLAG_JOINABLE);
}

ucb_thread* ucb_thread_new_detached()
{
    return ucb_thread_new_flags(UCB_THREAD_FLAG_DETACHED);
}

void ucb_thread_free(ucb_thread* th)
{
    UCB_VERIFY_ARGS(th);
    UCB_VERIFY(!th->running || (th->flags & UCB_THREAD_FLAG_JOINABLE), UCB_ERROR_INVALID_ARG,
               "Thread is running and not joinable");

    if (th->flags & UCB_THREAD_FLAG_JOINABLE)
        ucb_thread_join(th);

    ucb_thread_free_impl(th);
}

void ucb_thread_set_name(ucb_thread* thread, const char* name)
{
    UCB_VERIFY_ARGS(thread && !thread->running);

    if (thread->name)
        ucb_free(thread->name);
    thread->name = ucb_cstr_ndup(name, UCB_THREAD_NAME_MAX);

    if (thread->running)
        set_thread_name(thread);
}

const char* ucb_thread_get_name(const ucb_thread* thread)
{
    return thread ? thread->name : UCB_NULL;
}

void ucb_thread_set_stack_size(ucb_thread* thread, size_t stack_size)
{
    UCB_VERIFY_ARGS(thread && !thread->running);

#ifndef _WIN32
    if (stack_size > 0 && stack_size < (size_t)PTHREAD_STACK_MIN)
        stack_size = PTHREAD_STACK_MIN;
#endif
    if (stack_size > 0)
    {
        size_t align = ucb_sys_get_thread_alignment();
        stack_size = (stack_size + align - 1) & ~(align - 1);
    }
    thread->stack_size = stack_size;
}

size_t ucb_thread_get_stack_size(const ucb_thread* thread)
{
    return thread ? thread->stack_size : 0;
}

void ucb_thread_set_priority(ucb_thread* thread, int priority)
{
    UCB_VERIFY_ARGS(thread && !thread->running && priority >= UCB_THREAD_PRIO_MIN &&
                    priority <= UCB_THREAD_PRIO_MAX);

    thread->priority = priority;
}

int ucb_thread_get_priority(const ucb_thread* thread)
{
    return thread ? thread->priority : UCB_THREAD_PRIO_DEFAULT;
}

bool ucb_thread_is_joinable(const ucb_thread* th)
{
    return th && (th->flags & UCB_THREAD_FLAG_JOINABLE);
}

ucb_pid ucb_thread_get_id(ucb_thread* th)
{
    return th ? th->id : UCB_PID_INVALID;
}

bool ucb_thread_start(ucb_thread* th, ucb_task task)
{
    // TODO: Needs error checking
    UCB_VERIFY_ARGS(th && task.func);
    UCB_VERIFY(!th->running, UCB_ERROR_THREAD_BUSY, "Thread is already running");
    if (!ucb_task_validate(&task))
        return false;

    if (th->stack_size == 0)
    {
        th->stack_size = ucb_thread_get_current_stack_size();
    }
    // UCB_DPRINT_INT("ucb_thread_start: stack_size = %zu\n", th->stack_size, "thread size");

    th->task = task;
    th->running = true;
    th->status[0] = 0;
    th->id = UCB_PID_INVALID;
#if defined(_WIN32)
    unsigned threadaddr;
    int stack_size = (int)th->stack_size;
    th->handle = (HANDLE)_beginthreadex(NULL, stack_size, win_thread_wrapper, th, 0, &threadaddr);
    if (!th->handle)
    {
        th->running = false;
        return false;
    }
    if (th->flags & UCB_THREAD_FLAG_DETACHED)
    {
        CloseHandle(th->handle);
        th->handle = INVALID_HANDLE_VALUE;
    }
#else
    pthread_attr_t attr;
    pthread_attr_init(&attr);
    pthread_attr_setstacksize(&attr, th->stack_size);
    if (th->priority != UCB_THREAD_PRIO_DEFAULT)
        set_thread_prio_posix(th, &attr);
    if (th->flags & UCB_THREAD_FLAG_DETACHED)
        pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_DETACHED);
    else
        pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_JOINABLE);
    int ret = pthread_create(&th->handle, &attr, posix_thread_wrapper, th);
    if (ret != 0)
    {
        pthread_attr_destroy(&attr);
        th->running = false;
        return false;
    }
#endif
    return true;
}

void ucb_thread_join(ucb_thread* th)
{
    UCB_VERIFY_ARGS(th && (th->flags & UCB_THREAD_FLAG_JOINABLE));

#if defined(_WIN32)
    if (th->handle != INVALID_HANDLE_VALUE)
    {
        DWORD ret = WaitForSingleObject(th->handle, INFINITE);
        if (ret != 0)
        {
            if (ret == WAIT_FAILED)
                UCB_REPORT_WIN32(GetLastError(), "WaitForSingleObject failed");
            else
                UCB_VERIFY(false, UCB_ERRSYS_UNKNOWN,
                           "WaitForSingleObject failed with unknown error");
            // failover?
        }
        CloseHandle(th->handle);
        th->handle = INVALID_HANDLE_VALUE;
    }
#else
    if (th->handle != 0)
    {
        int ret = pthread_join(th->handle, NULL);
        if (ret != 0)
        {
            UCB_REPORT_ERRNO(ret, "pthread_join failed");
            // failover?
        }
        th->handle = 0;
    }
#endif
    th->running = false;
}

bool ucb_thread_is_running(ucb_thread* th)
{
    return th && th->running;
}

bool ucb_thread_get_task_status(const ucb_thread* th, int* status)
{
    UCB_VERIFY_ARGS(th && (th->flags & UCB_THREAD_FLAG_JOINABLE));

    if (!th->status[0])
        return false;
    *status = th->status[1];
    return true;
}
