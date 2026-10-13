/**
 * @file test_envmap.cpp
 *
 * This file is part of the UCB project
 * - SPDX-FileCopyrightText: © 2026 Åke Svedin <ake@svedin.org>
 * - SPDX-License-Identifier: MIT
 *
 * @brief Environment map tests
 */

#include "ucb/envmap.h"

#include "ucb/defines.h"
#include "ucb/env.h"
#include <ucb/string_ex.h>

#include <doctest.h>

#include <string>

TEST_CASE("envmap - general")
{
    const char* _testKey = "__UCB_ENVMAP_TEST__";
    std::string val;

    SUBCASE("new and free")
    {
        ucb_envmap* map = ucb_envmap_new();
        REQUIRE(map != UCB_NULL);
        ucb_envmap_free(map);
    }

    SUBCASE("set and get")
    {
        ucb_envmap* map = ucb_envmap_new();
        REQUIRE(map != UCB_NULL);

        CHECK(ucb_envmap_set(map, UCB_STR(_testKey), UCB_STR("hello")));
        CHECK(std::string(ucb_str_cstr(ucb_envmap_get(map, UCB_STR(_testKey)))) == "hello");
        CHECK(ucb_envmap_set(map, UCB_STR(_testKey), UCB_STR("world")));
        CHECK(std::string(ucb_str_cstr(ucb_envmap_get(map, UCB_STR(_testKey)))) == "world");

        ucb_envmap_free(map);
    }

    SUBCASE("unset and has")
    {
        ucb_envmap* map = ucb_envmap_new();
        REQUIRE(map != UCB_NULL);

        CHECK(ucb_envmap_set(map, UCB_STR(_testKey), UCB_STR("val")));
        CHECK(ucb_envmap_has(map, UCB_STR(_testKey)));
        CHECK(ucb_envmap_unset(map, UCB_STR(_testKey)));
        CHECK_FALSE(ucb_envmap_has(map, UCB_STR(_testKey)));

        ucb_envmap_free(map);
    }

    SUBCASE("append")
    {
        ucb_envmap* map = ucb_envmap_new();
        REQUIRE(map != UCB_NULL);

        // No previous value
        CHECK(ucb_envmap_append(map, UCB_STR(_testKey), UCB_STR("first"), UCB_STR(":")));
        CHECK(std::string(ucb_str_cstr(ucb_envmap_get(map, UCB_STR(_testKey)))) == "first");

        CHECK(ucb_envmap_append(map, UCB_STR(_testKey), UCB_STR("second"), UCB_STR(":")));
        CHECK(std::string(ucb_str_cstr(ucb_envmap_get(map, UCB_STR(_testKey)))) == "first:second");

        CHECK(ucb_envmap_append(map, UCB_STR(_testKey), UCB_STR("third"), UCB_NULL));
        CHECK(std::string(ucb_str_cstr(ucb_envmap_get(map, UCB_STR(_testKey)))) ==
              "first:secondthird");

        ucb_envmap_free(map);
    }

    SUBCASE("prepend")
    {
        ucb_envmap* map = ucb_envmap_new();
        REQUIRE(map != UCB_NULL);

        // No previous value
        CHECK(ucb_envmap_prepend(map, UCB_STR(_testKey), UCB_STR("first"), UCB_STR(":")));
        CHECK(std::string(ucb_str_cstr(ucb_envmap_get(map, UCB_STR(_testKey)))) == "first");

        CHECK(ucb_envmap_prepend(map, UCB_STR(_testKey), UCB_STR("second"), UCB_STR(":")));
        CHECK(std::string(ucb_str_cstr(ucb_envmap_get(map, UCB_STR(_testKey)))) == "second:first");

        CHECK(ucb_envmap_prepend(map, UCB_STR(_testKey), UCB_STR("third"), UCB_NULL));
        CHECK(std::string(ucb_str_cstr(ucb_envmap_get(map, UCB_STR(_testKey)))) ==
              "thirdsecond:first");

        ucb_envmap_free(map);
    }

    SUBCASE("clone")
    {
        ucb_envmap* map = ucb_envmap_new();
        REQUIRE(map != UCB_NULL);

        CHECK(ucb_envmap_set(map, UCB_STR(_testKey), UCB_STR("original")));

        ucb_envmap* clone = ucb_envmap_clone(map);
        REQUIRE(clone != UCB_NULL);

        CHECK(std::string(ucb_str_cstr(ucb_envmap_get(clone, UCB_STR(_testKey)))) == "original");

        // Modifying the clone should not affect the original
        CHECK(ucb_envmap_set(clone, UCB_STR(_testKey), UCB_STR("modified")));
        CHECK(std::string(ucb_str_cstr(ucb_envmap_get(clone, UCB_STR(_testKey)))) == "modified");
        CHECK(std::string(ucb_str_cstr(ucb_envmap_get(map, UCB_STR(_testKey)))) == "original");

        ucb_envmap_free(clone);
        ucb_envmap_free(map);
    }

    SUBCASE("copy")
    {
        ucb_envmap* src = ucb_envmap_new();
        ucb_envmap* dst = ucb_envmap_new();
        REQUIRE(src != UCB_NULL);
        REQUIRE(dst != UCB_NULL);

        CHECK(ucb_envmap_set(src, UCB_STR(_testKey), UCB_STR("copied")));
        CHECK(ucb_envmap_set(dst, UCB_STR(_testKey), UCB_STR("original")));

        ucb_envmap_copy(dst, src);
        CHECK(std::string(ucb_str_cstr(ucb_envmap_get(dst, UCB_STR(_testKey)))) == "copied");

        ucb_envmap_free(src);
        ucb_envmap_free(dst);
    }

    SUBCASE("init from current")
    {
        ucb_envmap* map = ucb_envmap_new();
        REQUIRE(map != UCB_NULL);

        ucb_envmap_init_from_current(map);

        // PATH should exist in any real environment
        if (ucb_envmap_has(map, UCB_STR("PATH")))
        {
            CHECK(std::string(ucb_str_cstr(ucb_envmap_get(map, UCB_STR("PATH")))).size() > 0);
        }

        ucb_envmap_free(map);
    }

    SUBCASE("reserve")
    {
        ucb_envmap* map = ucb_envmap_new();
        REQUIRE(map != UCB_NULL);

        ucb_envmap_reserve(map, 100);

        // Should still work after reserving
        CHECK(ucb_envmap_set(map, UCB_STR(_testKey), UCB_STR("value")));
        CHECK(std::string(ucb_str_cstr(ucb_envmap_get(map, UCB_STR(_testKey)))) == "value");

        ucb_envmap_free(map);
    }

    SUBCASE("apply")
    {
        ucb_envmap* map = ucb_envmap_new();
        REQUIRE(map != UCB_NULL);

        // Set a value and apply it to the process environment
        CHECK(ucb_envmap_set(map, UCB_STR(_testKey), UCB_STR("applied")));
        ucb_envmap_apply(map);

        // Now check via getenv that it was applied
        const char* env_val = getenv(_testKey);
        REQUIRE(env_val != UCB_NULL);
        CHECK(std::string(env_val) == "applied");

        // Clean up the process environment
        ucb_env_unset(UCB_STR(_testKey));

        ucb_envmap_free(map);
    }

    SUBCASE("get non-existent returns NULL")
    {
        ucb_envmap* map = ucb_envmap_new();
        REQUIRE(map != UCB_NULL);

        CHECK(ucb_envmap_get(map, UCB_STR("__NONEXISTENT_KEY__")) == UCB_NULL);
        CHECK_FALSE(ucb_envmap_has(map, UCB_STR("__NONEXISTENT_KEY__")));

        ucb_envmap_free(map);
    }

    SUBCASE("free NULL is safe")
    {
        ucb_envmap_free(UCB_NULL);
    }
}
