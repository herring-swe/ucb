/**
 * @file ucb.c
 *
 * This file is part of the UCB project
 * - SPDX-FileCopyrightText: © 2026 Åke Svedin <ake@svedin.org>
 * - SPDX-License-Identifier: MIT
 *
 * @brief UCB core implementation
 */

#include "ucb/ucb.h"

#include "ucb/threads.h"

#include <stdio.h>

#ifdef _WIN32
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <Windows.h>
#endif

UCB_THREAD_LOCAL ucb_config s_config = {
    ._placeholder = 0,
};

const char* ucb_get_version(void)
{
    return UCB_VER_STRING;
}

void ucb_init_console(void)
{
#ifdef _WIN32
    SetConsoleOutputCP(CP_UTF8); // CP_UTF8 - 65001
#endif
}

ucb_config ucb_conf_get(void)
{
    return s_config;
}

void ucb_conf_set(const ucb_config* config)
{
    if (config)
    {
        s_config = *config;
    }
    else
    {
        s_config = (ucb_config){
            ._placeholder = 0,
        };
    }
}
