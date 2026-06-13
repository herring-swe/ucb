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

TEST_CASE("tgfunc")
{
    CHECK(test_tgfunc() == 0);
}
