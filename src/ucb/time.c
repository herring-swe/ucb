/**
 * This file is part of the UCB project
 * SPDX-FileCopyrightText: © 2026 Åke Svedin <ake@svedin.org>
 * SPDX-License-Identifier: MIT
 *
 * @brief Time handling utilities implementation
 */

#include "ucb/time.h"

#include "ucb/debug.h"
#include "ucb/error.h"

#ifdef _WIN32
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <Windows.h>
#else // POSIX
#include <errno.h>
#include <time.h>
#endif

void ucb_sleep(ucb_stime dur)
{
    UCB_VERIFY_ARGS(dur >= 0 && dur <= INT32_MAX / 1000);

#ifdef _WIN32
    Sleep(dur * 1000);
#else
    struct timespec ts;
    ts.tv_sec  = dur;
    ts.tv_nsec = 0;
    while (nanosleep(&ts, &ts) == -1 && errno == EINTR)
    {
        // Loop until sleep completes or error other than EINTR
    }
    UCB_ASSERT_INTERNAL(errno == 0, "nanosleep failed");
#endif
}

void ucb_sleep_ms(ucb_stime_ms dur)
{
    UCB_VERIFY_ARGS(dur >= 0 && dur <= INT32_MAX);

#ifdef _WIN32
    Sleep((DWORD)dur);
#else
    struct timespec ts;
    ts.tv_sec  = dur / 1000;
    ts.tv_nsec = (dur % 1000) * 1000000;
    while (nanosleep(&ts, &ts) == -1 && errno == EINTR)
    {
        // Loop until sleep completes or error other than EINTR
    }
    UCB_ASSERT_INTERNAL(errno == 0, "nanosleep failed");
#endif
}
