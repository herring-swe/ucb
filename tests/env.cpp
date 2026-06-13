/**
 * @file test_env.cpp
 *
 * This file is part of the UCB project
 * - SPDX-FileCopyrightText: © 2026 Åke Svedin <ake@svedin.org>
 * - SPDX-License-Identifier: MIT
 *
 * @brief Environment tests
 */

#include "ucb/env.h"

#include "ucb/cstring.h"
#include "ucb/defines.h"

#include <doctest.h>

#include <string>

TEST_CASE("environment")
{
    const char* _testKey = "__UCB_ENV_TEST__";
    std::string val;

    SUBCASE("set and get")
    {
        ucb_env_unset(_testKey);
        CHECK(ucb_env_set(_testKey, "hello", true));
        CHECK(std::string(ucb_env_get(_testKey)) == "hello");
        CHECK(ucb_env_set(_testKey, "world", false));
        CHECK(std::string(ucb_env_get(_testKey)) == "hello");
        ucb_env_unset(_testKey);
    }

    SUBCASE("unset and has")
    {
        ucb_env_set(_testKey, "val", true);
        CHECK(ucb_env_has(_testKey));
        CHECK(ucb_env_unset(_testKey));
        CHECK_FALSE(ucb_env_has(_testKey));
    }

    SUBCASE("append")
    {
        ucb_env_unset(_testKey);

        // No previous env
        CHECK(ucb_env_append(_testKey, "first", ":"));
        CHECK(std::string(ucb_env_get(_testKey)) == "first");

        CHECK(ucb_env_append(_testKey, "second", ":"));
        CHECK(std::string(ucb_env_get(_testKey)) == "first:second");

        CHECK(ucb_env_append(_testKey, "third", UCB_NULL));
        CHECK(std::string(ucb_env_get(_testKey)) == "first:secondthird");

        ucb_env_unset(_testKey);
    }

    SUBCASE("prepend")
    {
        ucb_env_unset(_testKey);

        // No previous env
        CHECK(ucb_env_prepend(_testKey, "first", ":"));
        CHECK(std::string(ucb_env_get(_testKey)) == "first");

        CHECK(ucb_env_prepend(_testKey, "second", ":"));
        CHECK(std::string(ucb_env_get(_testKey)) == "second:first");

        CHECK(ucb_env_prepend(_testKey, "third", UCB_NULL));
        CHECK(std::string(ucb_env_get(_testKey)) == "thirdsecond:first");

        ucb_env_unset(_testKey);
    }
}
