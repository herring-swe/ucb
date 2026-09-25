/**
 * @file math.cpp
 *
 * This file is part of the UCB project
 * - SPDX-FileCopyrightText: © 2026 Åke Svedin <ake@svedin.org>
 * - SPDX-License-Identifier: MIT
 *
 * @brief math tests
 */

#include "test_tgfunc.h"

#include <doctest.h>

#include <cstdio>

TEST_CASE("tgfunc - general")
{
    CHECK(test_tgfunc() == 0);
}

TEST_CASE("tgfunc - generics")
{
    CHECK(test_tgfunc_generics() == 0);
}
