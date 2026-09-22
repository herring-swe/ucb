/**
 * @file once.c
 * This file is part of the UCB project
 * - SPDX-FileCopyrightText: © 2026 Åke Svedin <ake@svedin.org>
 * - SPDX-License-Identifier: MIT
 *
 * @brief Cross platform one-time initialization implementation
 */

#include "once_private.h"

#include "ucb/error.h"
#include "ucb/memory.h"

_Static_assert(sizeof(struct ucb_once_impl) <= UCB_ONCE_STORAGE_SIZE,
               "ucb_once storage is too small for the platform once object");

#if !defined(_WIN32)
// UCB_ONCE_INIT is an all-zero initializer. Verify that the platform once
// object has the same all-zero representation, so static initialization is
// equivalent to ucb_once_init. The supported POSIX platforms (glibc/musl)
// define PTHREAD_ONCE_INIT as 0.
_Static_assert(PTHREAD_ONCE_INIT == 0, "UCB_ONCE_INIT assumes a zero-initialized pthread_once_t");
#endif

// Both PTHREAD_ONCE_INIT and INIT_ONCE_STATIC_INIT are all-zero, which is what
// UCB_ONCE_INIT provides. This is asserted indirectly by ucb_once_init, which
// assigns UCB_ONCE_INIT rather than relying on the platform macro.

void ucb_once_init(ucb_once* once)
{
    UCB_VERIFY_ARGS(once);
    *once = (ucb_once)UCB_ONCE_INIT;
}

ucb_once* ucb_once_new(void)
{
    ucb_once* once = ucb_malloc_type(1, ucb_once);
    if (!once)
        return UCB_NULL;
    ucb_once_init(once);
    return once;
}

void ucb_once_free(ucb_once* once)
{
    ucb_free(once);
}

#if defined(_WIN32)

typedef struct ucb_once_call
{
    void (*func)(void);
} ucb_once_call;

static BOOL CALLBACK ucb_once_trampoline(PINIT_ONCE init_once, PVOID param, PVOID* context)
{
    UCB_UNUSED(init_once);
    UCB_UNUSED(context);
    const ucb_once_call* call = (const ucb_once_call*)param;
    call->func();
    return TRUE;
}

void ucb_once_run(ucb_once* once, void (*func)(void))
{
    UCB_VERIFY_ARGS(once && func);
    // The call object is only used for the duration of this synchronous call.
    ucb_once_call call = {func};
    if (!InitOnceExecuteOnce(&UCB_ONCE_IMPL(once)->handle, ucb_once_trampoline, &call, UCB_NULL))
    {
        UCB_REPORT_WIN32(GetLastError(), "InitOnceExecuteOnce failed");
    }
}

#else

void ucb_once_run(ucb_once* once, void (*func)(void))
{
    UCB_VERIFY_ARGS(once && func);
    UCB_REPORT_ERRNO(pthread_once(&UCB_ONCE_IMPL(once)->handle, func),
                     "Failed to run once function");
}

#endif
