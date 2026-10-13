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
#include <ucb/string_ex.h>

#include <doctest.h>

#include <string>

static std::string env_get(const char* name)
{
    ucb_str* value = ucb_env_get(UCB_STR(name));
    if (!value)
        return std::string();
    std::string out(ucb_str_cstr(value));
    ucb_str_free(value);
    return out;
}

TEST_CASE("env - general")
{
    const char* _testKey = "__UCB_ENV_TEST__";
    std::string val;

    SUBCASE("set and get")
    {
        ucb_env_unset(UCB_STR(_testKey));
        CHECK(ucb_env_set(UCB_STR(_testKey), UCB_STR("hello"), true));
        CHECK(env_get(_testKey) == "hello");
        CHECK(ucb_env_set(UCB_STR(_testKey), UCB_STR("world"), false));
        CHECK(env_get(_testKey) == "hello");
        ucb_env_unset(UCB_STR(_testKey));
    }

    SUBCASE("unset and has")
    {
        ucb_env_set(UCB_STR(_testKey), UCB_STR("val"), true);
        CHECK(ucb_env_has(UCB_STR(_testKey)));
        CHECK(ucb_env_unset(UCB_STR(_testKey)));
        CHECK_FALSE(ucb_env_has(UCB_STR(_testKey)));
    }

    SUBCASE("append")
    {
        ucb_env_unset(UCB_STR(_testKey));

        // No previous env
        CHECK(ucb_env_append(UCB_STR(_testKey), UCB_STR("first"), UCB_STR(":")));
        CHECK(env_get(_testKey) == "first");

        CHECK(ucb_env_append(UCB_STR(_testKey), UCB_STR("second"), UCB_STR(":")));
        CHECK(env_get(_testKey) == "first:second");

        CHECK(ucb_env_append(UCB_STR(_testKey), UCB_STR("third"), UCB_NULL));
        CHECK(env_get(_testKey) == "first:secondthird");

        ucb_env_unset(UCB_STR(_testKey));
    }

    SUBCASE("prepend")
    {
        ucb_env_unset(UCB_STR(_testKey));

        // No previous env
        CHECK(ucb_env_prepend(UCB_STR(_testKey), UCB_STR("first"), UCB_STR(":")));
        CHECK(env_get(_testKey) == "first");

        CHECK(ucb_env_prepend(UCB_STR(_testKey), UCB_STR("second"), UCB_STR(":")));
        CHECK(env_get(_testKey) == "second:first");

        CHECK(ucb_env_prepend(UCB_STR(_testKey), UCB_STR("third"), UCB_NULL));
        CHECK(env_get(_testKey) == "thirdsecond:first");

        ucb_env_unset(UCB_STR(_testKey));
    }
}
