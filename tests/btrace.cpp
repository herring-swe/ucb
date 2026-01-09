/**
 * @file btrace.cpp
 * 
 * This file is part of the UCB project
 * - SPDX-FileCopyrightText: © 2026 Åke Svedin <ake@svedin.org>
 * - SPDX-License-Identifier: MIT
 * 
 * @brief btrace tests
 */

#include "doctest.h"

#include "ucb/btrace.h"
#include "ucb/config.h"

#include <stdio.h>

static ucb_btrace* rec_func(int remain)
{
    if (remain)
        return rec_func(remain - 1);
    else
    {
        ucb_btrace* bt = ucb_btrace_new();
        ucb_btrace_capture(bt);
        return bt;
    }
}

TEST_CASE("backtrace")
{
    SUBCASE("10 levels")
    {
        ucb_btrace* bt = rec_func(10);
        REQUIRE(bt);
#ifdef NDEBUG
        // Don't fight the optimizer
        REQUIRE(bt->count > 1);
#else
        REQUIRE(bt->count > 10);
#endif
        ucb_btrace_print(bt, stdout, 0);
        ucb_btrace_free(bt);
    }

    SUBCASE("150 levels")
    {
        ucb_btrace* bt = rec_func(150);
        REQUIRE(bt);
        ucb_btrace_print(bt, stdout, 0);
#ifdef NDEBUG
        // Don't fight the optimizer
        REQUIRE(bt->count > 1);
#else
        REQUIRE(bt->count == UCB_BTRACE_MAX_FRAMES);
#endif
        ucb_btrace_free(bt);
    }
}
