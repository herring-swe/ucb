/**
 * @file version.cpp
 * This file is part of the UCB project
 * - SPDX-FileCopyrightText: © 2026 Åke Svedin <ake@svedin.org>
 * - SPDX-License-Identifier: MIT
 *
 * @brief Version and capability tests
 */

#include <ucb/ucb.h>
#include <ucb/version.h>

#include <doctest.h>

#include <string>

TEST_SUITE_BEGIN("version");

TEST_CASE("version - general")
{
    const char* version = ucb_get_version();
    REQUIRE(version != nullptr);
    CHECK(std::string(version) == UCB_TEST_EXPECTED_VERSION);
    CHECK(std::string(UCB_VER_STRING) == UCB_TEST_EXPECTED_VERSION);

    CHECK(UCB_VER == UCB_VER_MAJOR * 10000 + UCB_VER_MINOR * 100 + UCB_VER_PATCH);

    CHECK(UCB_OS_WINDOWS + UCB_OS_LINUX == 1);
    CHECK(UCB_COMPILER_CLANG + UCB_COMPILER_MSVC + UCB_COMPILER_GCC == 1);

    CHECK((UCB_BUILD_SHARED == 0 || UCB_BUILD_SHARED == 1));
}

TEST_SUITE_END();