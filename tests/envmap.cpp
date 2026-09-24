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

        CHECK(ucb_envmap_set(map, _testKey, "hello"));
        CHECK(std::string(ucb_envmap_get(map, _testKey)) == "hello");
        CHECK(ucb_envmap_set(map, _testKey, "world"));
        CHECK(std::string(ucb_envmap_get(map, _testKey)) == "world");

        ucb_envmap_free(map);
    }

    SUBCASE("unset and has")
    {
        ucb_envmap* map = ucb_envmap_new();
        REQUIRE(map != UCB_NULL);

        CHECK(ucb_envmap_set(map, _testKey, "val"));
        CHECK(ucb_envmap_has(map, _testKey));
        CHECK(ucb_envmap_unset(map, _testKey));
        CHECK_FALSE(ucb_envmap_has(map, _testKey));

        ucb_envmap_free(map);
    }

    SUBCASE("append")
    {
        ucb_envmap* map = ucb_envmap_new();
        REQUIRE(map != UCB_NULL);

        // No previous value
        CHECK(ucb_envmap_append(map, _testKey, "first", ":"));
        CHECK(std::string(ucb_envmap_get(map, _testKey)) == "first");

        CHECK(ucb_envmap_append(map, _testKey, "second", ":"));
        CHECK(std::string(ucb_envmap_get(map, _testKey)) == "first:second");

        CHECK(ucb_envmap_append(map, _testKey, "third", UCB_NULL));
        CHECK(std::string(ucb_envmap_get(map, _testKey)) == "first:secondthird");

        ucb_envmap_free(map);
    }

    SUBCASE("prepend")
    {
        ucb_envmap* map = ucb_envmap_new();
        REQUIRE(map != UCB_NULL);

        // No previous value
        CHECK(ucb_envmap_prepend(map, _testKey, "first", ":"));
        CHECK(std::string(ucb_envmap_get(map, _testKey)) == "first");

        CHECK(ucb_envmap_prepend(map, _testKey, "second", ":"));
        CHECK(std::string(ucb_envmap_get(map, _testKey)) == "second:first");

        CHECK(ucb_envmap_prepend(map, _testKey, "third", UCB_NULL));
        CHECK(std::string(ucb_envmap_get(map, _testKey)) == "thirdsecond:first");

        ucb_envmap_free(map);
    }

    SUBCASE("clone")
    {
        ucb_envmap* map = ucb_envmap_new();
        REQUIRE(map != UCB_NULL);

        CHECK(ucb_envmap_set(map, _testKey, "original"));

        ucb_envmap* clone = ucb_envmap_clone(map);
        REQUIRE(clone != UCB_NULL);

        CHECK(std::string(ucb_envmap_get(clone, _testKey)) == "original");

        // Modifying the clone should not affect the original
        CHECK(ucb_envmap_set(clone, _testKey, "modified"));
        CHECK(std::string(ucb_envmap_get(clone, _testKey)) == "modified");
        CHECK(std::string(ucb_envmap_get(map, _testKey)) == "original");

        ucb_envmap_free(clone);
        ucb_envmap_free(map);
    }

    SUBCASE("copy")
    {
        ucb_envmap* src = ucb_envmap_new();
        ucb_envmap* dst = ucb_envmap_new();
        REQUIRE(src != UCB_NULL);
        REQUIRE(dst != UCB_NULL);

        CHECK(ucb_envmap_set(src, _testKey, "copied"));
        CHECK(ucb_envmap_set(dst, _testKey, "original"));

        ucb_envmap_copy(dst, src);
        CHECK(std::string(ucb_envmap_get(dst, _testKey)) == "copied");

        ucb_envmap_free(src);
        ucb_envmap_free(dst);
    }

    SUBCASE("init from current")
    {
        ucb_envmap* map = ucb_envmap_new();
        REQUIRE(map != UCB_NULL);

        ucb_envmap_init_from_current(map);

        // PATH should exist in any real environment
        if (ucb_envmap_has(map, "PATH"))
        {
            CHECK(std::string(ucb_envmap_get(map, "PATH")).size() > 0);
        }

        ucb_envmap_free(map);
    }

    SUBCASE("reserve")
    {
        ucb_envmap* map = ucb_envmap_new();
        REQUIRE(map != UCB_NULL);

        ucb_envmap_reserve(map, 100);

        // Should still work after reserving
        CHECK(ucb_envmap_set(map, _testKey, "value"));
        CHECK(std::string(ucb_envmap_get(map, _testKey)) == "value");

        ucb_envmap_free(map);
    }

    SUBCASE("apply")
    {
        ucb_envmap* map = ucb_envmap_new();
        REQUIRE(map != UCB_NULL);

        // Set a value and apply it to the process environment
        CHECK(ucb_envmap_set(map, _testKey, "applied"));
        ucb_envmap_apply(map);

        // Now check via getenv that it was applied
        const char* env_val = getenv(_testKey);
        REQUIRE(env_val != UCB_NULL);
        CHECK(std::string(env_val) == "applied");

        // Clean up the process environment
        ucb_env_unset(_testKey);

        ucb_envmap_free(map);
    }

    SUBCASE("get non-existent returns NULL")
    {
        ucb_envmap* map = ucb_envmap_new();
        REQUIRE(map != UCB_NULL);

        CHECK(ucb_envmap_get(map, "__NONEXISTENT_KEY__") == UCB_NULL);
        CHECK_FALSE(ucb_envmap_has(map, "__NONEXISTENT_KEY__"));

        ucb_envmap_free(map);
    }

    SUBCASE("free NULL is safe")
    {
        ucb_envmap_free(UCB_NULL);
    }
}
