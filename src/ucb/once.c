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

void ucb_once_init_common(ucb_once* once)
{
    UCB_VERIFY_ARGS(once);
    *once = (ucb_once)UCB_ONCE_INIT;
}

ucb_once* ucb_once_new(void)
{
    ucb_once* once = ucb_malloc_type(1, ucb_once);
    if (!once)
        return UCB_NULL;
    ucb_once_init_common(once);
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
    InitOnceExecuteOnce(&once->handle, ucb_once_trampoline, &call, UCB_NULL);
}

#else

void ucb_once_run(ucb_once* once, void (*func)(void))
{
    UCB_VERIFY_ARGS(once && func);
    UCB_REPORT_ERRNO(pthread_once(&once->handle, func), "Failed to run once function");
}

#endif
