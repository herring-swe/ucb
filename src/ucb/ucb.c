/**
 * This file is part of the UCB project
 * SPDX-FileCopyrightText: 2025 Åke Svedin <ake@svedin.org>
 * SPDX-License-Identifier: MIT
 *
 * @brief
 */

#include "ucb/ucb.h"

#include <stdio.h>

#ifdef _WIN32
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <Windows.h>
#endif

const char* ucb_get_version(void)
{
    return "0.1.0";
}

void ucb_init_console(void)
{
#ifdef _WIN32
    SetConsoleOutputCP(CP_UTF8); // CP_UTF8 - 65001
#endif
}
