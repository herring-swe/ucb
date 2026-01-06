/**
 * This file is part of the UCB project
 * SPDX-FileCopyrightText: © 2025 Åke Svedin <ake@svedin.org>
 * SPDX-License-Identifier: MIT
 *
 * @brief
 */

#include "sys_private.h"

#ifdef _WIN32
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <Windows.h>
#else
#include <unistd.h>
#endif

size_t ucb_sys_get_thread_alignment(void)
{
#ifdef _WIN32
    SYSTEM_INFO info;
    GetNativeSystemInfo(&info);
    return info.dwAllocationGranularity;
#else
    return (size_t)getpagesize();
#endif
}
