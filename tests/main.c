/**
 * @file main.c
 *
 * This file is part of the UCB project
 * - SPDX-FileCopyrightText: © 2026 Åke Svedin <ake@svedin.org>
 * - SPDX-License-Identifier: MIT
 *
 * @brief ucb command line tool
 */

#include <ucb/ucb.h>

#include <stdio.h>

int main(int argc, char* argv[])
{
    ucb_init_console();
    printf("UCB version: %s\n", ucb_get_version());
    return 0;
}
