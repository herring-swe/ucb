/**
 * @file types.cpp
 * 
 * This file is part of the UCB project
 * - SPDX-FileCopyrightText: © 2026 Åke Svedin <ake@svedin.org>
 * - SPDX-License-Identifier: MIT
 * 
 * @brief types tests
 */

#include "doctest.h"

#include "test_types.h"

#include <cstdio>

TEST_CASE("types")
{
    CHECK(test_types() == 0);
}
