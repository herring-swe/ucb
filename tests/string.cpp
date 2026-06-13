/**
 * @file string.cpp
 *
 * This file is part of the UCB project
 * - SPDX-FileCopyrightText: © 2026 Åke Svedin <ake@svedin.org>
 * - SPDX-License-Identifier: MIT
 *
 * @brief String tests
 */

#include <ucb/memdbg.h>
#include <ucb/memory.h>
#include <ucb/string.h>

#include <doctest.h>

#include <cstring>
#include <string>
#include <vector>

TEST_SUITE_BEGIN("string");

TEST_CASE("string lifetime")
{
    UCB_MEMTRACK_PUSH();

    // Test ucb_str_make()
    SUBCASE("ucb_str_make")
    {
        ucb_str str = ucb_str_make();
        CHECK(ucb_str_cstr(&str) != nullptr);
        CHECK(ucb_str_len(&str) == 0);
        CHECK(ucb_str_is_empty(&str) == true);
        CHECK(ucb_str_is_owned(&str) == false);
        ucb_str_release(&str);
    }

    // Test ucb_str_new()
    SUBCASE("ucb_str_new")
    {
        ucb_str* str = ucb_str_new("hello", 5);
        REQUIRE(str != nullptr);
        CHECK(strcmp(ucb_str_cstr(str), "hello") == 0);
        CHECK(ucb_str_len(str) == 5);
        CHECK(ucb_str_is_empty(str) == false);
        CHECK(ucb_str_is_owned(str) == true);
        ucb_str_free(str);

        // Test with nullptr
        str = ucb_str_new(nullptr, 0);
        REQUIRE(str != nullptr);
        CHECK(ucb_str_len(str) == 0);
        CHECK(ucb_str_is_empty(str) == true);
        ucb_str_free(str);

        // Test with length 0 (null-terminated)
        str = ucb_str_new("test", 0);
        REQUIRE(str != nullptr);
        CHECK(strcmp(ucb_str_cstr(str), "test") == 0);
        CHECK(ucb_str_len(str) == 4);
        ucb_str_free(str);
    }

    // Test ucb_str_new_c
    SUBCASE("ucb_str_new_c")
    {
        ucb_str* str = ucb_str_new_c("world");
        REQUIRE(str != nullptr);
        CHECK(strcmp(ucb_str_cstr(str), "world") == 0);
        CHECK(ucb_str_len(str) == 5);
        ucb_str_free(str);
    }

    // Test ucb_str_new_wrap
    SUBCASE("ucb_str_new_wrap")
    {
        ucb_str* str = ucb_str_new_wrap("wrapped", 7);
        REQUIRE(str != nullptr);
        CHECK(strcmp(ucb_str_cstr(str), "wrapped") == 0);
        CHECK(ucb_str_len(str) == 7);
        CHECK(ucb_str_is_owned(str) == false);
        ucb_str_free(str);

        // Test with length 0 (null-terminated)
        str = ucb_str_new_wrap("test", 0);
        REQUIRE(str != nullptr);
        CHECK(strcmp(ucb_str_cstr(str), "test") == 0);
        CHECK(ucb_str_len(str) == 4);
        CHECK(ucb_str_is_owned(str) == false);
        ucb_str_free(str);
    }

    // Test ucb_str_new_wrap_c
    SUBCASE("ucb_str_new_wrap_c")
    {
        ucb_str* str = ucb_str_new_wrap_c("wrapped_string");
        REQUIRE(str != nullptr);
        CHECK(strcmp(ucb_str_cstr(str), "wrapped_string") == 0);
        CHECK(ucb_str_len(str) == 14);
        CHECK(ucb_str_is_owned(str) == false);
        ucb_str_free(str);
    }

    // Test ucb_str_new_empty
    SUBCASE("ucb_str_new_empty")
    {
        ucb_str* str = ucb_str_new_empty();
        REQUIRE(str != nullptr);
        CHECK(ucb_str_len(str) == 0);
        CHECK(ucb_str_is_empty(str) == true);
        CHECK(ucb_str_is_owned(str) == false);
        ucb_str_free(str);
    }

    // Test ucb_str_clone
    SUBCASE("ucb_str_clone")
    {
        ucb_str* orig = ucb_str_new_c("clone_test");
        REQUIRE(orig != nullptr);

        ucb_str* cloned = ucb_str_clone(orig);
        REQUIRE(cloned != nullptr);
        CHECK(ucb_str_equal(orig, cloned));
        CHECK(ucb_str_is_owned(cloned) == true);

        ucb_str_free(orig);
        ucb_str_free(cloned);
    }

    // Test ucb_str_init
    SUBCASE("ucb_str_init")
    {
        ucb_str str;
        bool result = ucb_str_init(&str, "initialized", 11);
        REQUIRE(result == true);
        CHECK(strcmp(ucb_str_cstr(&str), "initialized") == 0);
        CHECK(ucb_str_len(&str) == 11);
        CHECK(ucb_str_is_owned(&str) == true);
        ucb_str_release(&str);
    }

    // Test ucb_str_init_c
    SUBCASE("ucb_str_init_c")
    {
        ucb_str str;
        ucb_str_init_c(&str, "init_c_test");
        CHECK(strcmp(ucb_str_cstr(&str), "init_c_test") == 0);
        CHECK(ucb_str_len(&str) == 11);
        ucb_str_release(&str);
    }

    // Test ucb_str_init_wrap
    SUBCASE("ucb_str_init_wrap")
    {
        ucb_str str;
        ucb_str_init_wrap(&str, "wrapped_init", 12);
        CHECK(strcmp(ucb_str_cstr(&str), "wrapped_init") == 0);
        CHECK(ucb_str_len(&str) == 12);
        CHECK(ucb_str_is_owned(&str) == false);
        ucb_str_release(&str);
    }

    // Test ucb_str_init_wrap_c
    SUBCASE("ucb_str_init_wrap_c")
    {
        ucb_str str;
        ucb_str_init_wrap_c(&str, "wrapped_init_c");
        CHECK(strcmp(ucb_str_cstr(&str), "wrapped_init_c") == 0);
        CHECK(ucb_str_len(&str) == 14);
        CHECK(ucb_str_is_owned(&str) == false);
        ucb_str_release(&str);
    }

    // Test ucb_str_init_empty
    SUBCASE("ucb_str_init_empty")
    {
        ucb_str str;
        ucb_str_init_empty(&str);
        CHECK(ucb_str_len(&str) == 0);
        CHECK(ucb_str_is_empty(&str) == true);
        CHECK(ucb_str_is_owned(&str) == false);
        ucb_str_release(&str);
    }

    UCB_MEMTRACK_POP();
}

TEST_CASE("string destruction")
{
    UCB_MEMTRACK_PUSH();

    SUBCASE("ucb_str_release")
    {
        ucb_str str;
        ucb_str_init_c(&str, "to_be_released");
        CHECK(ucb_str_len(&str) == 14);
        ucb_str_release(&str);
        // After release, the string should be empty and unowned
        CHECK(ucb_str_len(&str) == 0);
        CHECK(ucb_str_is_empty(&str) == true);
    }

    SUBCASE("ucb_str_free")
    {
        ucb_str* str = ucb_str_new_c("to_be_freed");
        REQUIRE(str != nullptr);
        CHECK(ucb_str_len(str) == 11);
        ucb_str_free(str);
        // After free, str pointer is invalid
    }

    UCB_MEMTRACK_POP();
}

TEST_CASE("string assignment and updating")
{
    UCB_MEMTRACK_PUSH();

    SUBCASE("ucb_str_copy")
    {
        ucb_str src;
        ucb_str_init_c(&src, "source_string");
        ucb_str dest = ucb_str_make();

        bool result = ucb_str_copy(&dest, &src);
        REQUIRE(result == true);
        CHECK(ucb_str_equal(&src, &dest));
        CHECK(ucb_str_is_owned(&dest) == true);

        ucb_str_release(&src);
        ucb_str_release(&dest);
    }

    SUBCASE("ucb_str_assign")
    {
        ucb_str str;
        ucb_str_init_empty(&str);

        bool result = ucb_str_assign(&str, "assigned_value", 14);
        REQUIRE(result == true);
        CHECK(strcmp(ucb_str_cstr(&str), "assigned_value") == 0);
        CHECK(ucb_str_len(&str) == 14);

        ucb_str_release(&str);
    }

    SUBCASE("ucb_str_assign_c")
    {
        ucb_str str;
        ucb_str_init_empty(&str);

        bool result = ucb_str_assign_c(&str, "assign_c_value");
        REQUIRE(result == true);
        CHECK(strcmp(ucb_str_cstr(&str), "assign_c_value") == 0);

        ucb_str_release(&str);
    }

    SUBCASE("ucb_str_detach")
    {
        ucb_str str;
        ucb_str_init_wrap_c(&str, "detach_test");
        CHECK(ucb_str_is_owned(&str) == false);

        bool modified = ucb_str_detach(&str);
        CHECK(modified == true);
        CHECK(ucb_str_is_owned(&str) == true);

        ucb_str_release(&str);
    }

    SUBCASE("ucb_str_fit")
    {
        ucb_str* str = ucb_str_new_c("fit_test");
        REQUIRE(str != nullptr);
        REQUIRE(ucb_str_reserve(str, 100)); // Ensure excess capacity

        // Now fit should reduce capacity
        bool modified = ucb_str_fit(str);
        CHECK(modified == true);

        ucb_str_free(str);
    }

    SUBCASE("ucb_str_reserve")
    {
        ucb_str str;
        ucb_str_init_empty(&str);

        bool result = ucb_str_reserve(&str, 50);
        REQUIRE(result == true);
        CHECK(ucb_str_avail(&str) >= 50);

        ucb_str_release(&str);
    }

    SUBCASE("ucb_str_wrap")
    {
        ucb_str str;
        ucb_str_init_c(&str, "old_value");
        CHECK(ucb_str_is_owned(&str) == true);

        ucb_str_wrap(&str, "new_wrapped", 11);
        CHECK(strcmp(ucb_str_cstr(&str), "new_wrapped") == 0);
        CHECK(ucb_str_len(&str) == 11);
        CHECK(ucb_str_is_owned(&str) == false);

        ucb_str_release(&str);
    }

    SUBCASE("ucb_str_wrap_c")
    {
        ucb_str str;
        ucb_str_init_c(&str, "old_value");
        CHECK(ucb_str_is_owned(&str) == true);

        ucb_str_wrap_c(&str, "new_wrapped_c");
        CHECK(strcmp(ucb_str_cstr(&str), "new_wrapped_c") == 0);
        CHECK(ucb_str_len(&str) == 13); // "new_wrapped_c" length is 13
        CHECK(ucb_str_is_owned(&str) == false);

        ucb_str_release(&str);
    }

    UCB_MEMTRACK_POP();
}

TEST_CASE("string querying")
{
    UCB_MEMTRACK_PUSH();

    SUBCASE("ucb_str_is_owned")
    {
        ucb_str owned_str;
        ucb_str_init_c(&owned_str, "owned");
        CHECK(ucb_str_is_owned(&owned_str) == true);

        ucb_str wrapped_str;
        ucb_str_init_wrap_c(&wrapped_str, "wrapped");
        CHECK(ucb_str_is_owned(&wrapped_str) == false);

        ucb_str_release(&owned_str);
        ucb_str_release(&wrapped_str);
    }

    SUBCASE("ucb_str_is_empty")
    {
        ucb_str empty_str = ucb_str_make();
        CHECK(ucb_str_is_empty(&empty_str) == true);

        ucb_str non_empty_str;
        ucb_str_init_c(&non_empty_str, "not_empty");
        CHECK(ucb_str_is_empty(&non_empty_str) == false);

        ucb_str_release(&empty_str);
        ucb_str_release(&non_empty_str);
    }

    SUBCASE("ucb_str_capacity")
    {
        ucb_str str;
        ucb_str_init_c(&str, "capacity_test");
        size_t cap = ucb_str_capacity(&str);
        CHECK(cap > 0);
        CHECK(cap >= ucb_str_len(&str));

        ucb_str wrapped;
        ucb_str_init_wrap_c(&wrapped, "wrapped");
        CHECK(ucb_str_capacity(&wrapped) == 0); // Wrapped strings have 0 capacity

        ucb_str_release(&str);
        ucb_str_release(&wrapped);
    }

    SUBCASE("ucb_str_used")
    {
        ucb_str str;
        ucb_str_init_c(&str, "used_test");
        size_t used = ucb_str_used(&str);
        CHECK(used >= ucb_str_len(&str));

        ucb_str wrapped;
        ucb_str_init_wrap_c(&wrapped, "wrapped");
        CHECK(ucb_str_used(&wrapped) == 0); // Wrapped strings have 0 used

        ucb_str_release(&str);
        ucb_str_release(&wrapped);
    }

    SUBCASE("ucb_str_avail")
    {
        ucb_str str;
        ucb_str_init_c(&str, "avail_test");
        size_t avail = ucb_str_avail(&str);
        CHECK(avail >= 0);

        ucb_str wrapped;
        ucb_str_init_wrap_c(&wrapped, "wrapped");
        CHECK(ucb_str_avail(&wrapped) == 0); // Wrapped strings have 0 available

        ucb_str_release(&str);
        ucb_str_release(&wrapped);
    }

    SUBCASE("ucb_str_cstr")
    {
        const char* test_str = "cstr_test";
        ucb_str str;
        ucb_str_init_c(&str, test_str);

        const char* cstr = ucb_str_cstr(&str);
        CHECK(strcmp(cstr, test_str) == 0);

        ucb_str_release(&str);
    }

    SUBCASE("ucb_str_len")
    {
        ucb_str str;
        ucb_str_init_c(&str, "len_test");
        size_t len = ucb_str_len(&str);
        CHECK(len == 8);

        ucb_str_release(&str);
    }

    UCB_MEMTRACK_POP();
}

TEST_CASE("string comparison and lookup")
{
    UCB_MEMTRACK_PUSH();

    SUBCASE("ucb_str_equal")
    {
        ucb_str str1, str2, str3;
        ucb_str_init_c(&str1, "equal_test");
        ucb_str_init_c(&str2, "equal_test");
        ucb_str_init_c(&str3, "different");

        CHECK(ucb_str_equal(&str1, &str2) == true);
        CHECK(ucb_str_equal(&str1, &str3) == false);

        ucb_str_release(&str1);
        ucb_str_release(&str2);
        ucb_str_release(&str3);
    }

    SUBCASE("ucb_str_comp")
    {
        ucb_str str1, str2, str3;
        ucb_str_init_c(&str1, "abc");
        ucb_str_init_c(&str2, "abc");
        ucb_str_init_c(&str3, "def");

        CHECK(ucb_str_comp(&str1, &str2) == 0);
        CHECK(ucb_str_comp(&str1, &str3) < 0);
        CHECK(ucb_str_comp(&str3, &str1) > 0);

        ucb_str_release(&str1);
        ucb_str_release(&str2);
        ucb_str_release(&str3);
    }

    SUBCASE("ucb_str_startswith")
    {
        ucb_str str, prefix1, prefix2;
        ucb_str_init_c(&str, "startswith_test");
        ucb_str_init_c(&prefix1, "start");
        ucb_str_init_c(&prefix2, "wrong");

        CHECK(ucb_str_startswith(&str, &prefix1) == true);
        CHECK(ucb_str_startswith(&str, &prefix2) == false);

        ucb_str_release(&str);
        ucb_str_release(&prefix1);
        ucb_str_release(&prefix2);
    }

    SUBCASE("ucb_str_endswith")
    {
        ucb_str str, suffix1, suffix2;
        ucb_str_init_c(&str, "endswith_test");
        ucb_str_init_c(&suffix1, "test");
        ucb_str_init_c(&suffix2, "wrong");

        CHECK(ucb_str_endswith(&str, &suffix1) == true);
        CHECK(ucb_str_endswith(&str, &suffix2) == false);

        ucb_str_release(&str);
        ucb_str_release(&suffix1);
        ucb_str_release(&suffix2);
    }

    SUBCASE("ucb_str_find")
    {
        ucb_str str, substr1, substr2;
        ucb_str_init_c(&str, "find_substring_test");
        ucb_str_init_c(&substr1, "sub");
        ucb_str_init_c(&substr2, "missing");

        size_t pos1 = ucb_str_find(&str, &substr1, 0);
        CHECK(pos1 == 5); // "sub" starts at position 5

        size_t pos2 = ucb_str_find(&str, &substr2, 0);
        CHECK(pos2 == SIZE_MAX); // "missing" not found

        ucb_str_release(&str);
        ucb_str_release(&substr1);
        ucb_str_release(&substr2);
    }

    UCB_MEMTRACK_POP();
}

TEST_CASE("string modification")
{
    UCB_MEMTRACK_PUSH();

    SUBCASE("ucb_str_clear")
    {
        ucb_str str;
        ucb_str_init_c(&str, "to_be_cleared");
        CHECK(ucb_str_len(&str) == 13);
        CHECK(ucb_str_is_empty(&str) == false);

        ucb_str_clear(&str);
        CHECK(ucb_str_len(&str) == 0);
        CHECK(ucb_str_is_empty(&str) == true);

        ucb_str_release(&str);
    }

    SUBCASE("ucb_str_append")
    {
        ucb_str str, append_str;
        ucb_str_init_c(&str, "original_");
        ucb_str_init_c(&append_str, "appended");

        ucb_str_append(&str, &append_str);
        CHECK(strcmp(ucb_str_cstr(&str), "original_appended") == 0);
        CHECK(ucb_str_len(&str) == 17);

        ucb_str_release(&str);
        ucb_str_release(&append_str);
    }

    SUBCASE("ucb_str_append_c")
    {
        ucb_str str;
        ucb_str_init_c(&str, "original_");
        ucb_str_append_c(&str, "appended_c");

        CHECK(strcmp(ucb_str_cstr(&str), "original_appended_c") == 0);
        CHECK(ucb_str_len(&str) == 19);

        ucb_str_release(&str);
    }

    SUBCASE("ucb_str_insert")
    {
        ucb_str str, insert_str;
        ucb_str_init_c(&str, "original_string");
        ucb_str_init_c(&insert_str, "_inserted");

        ucb_str_insert(&str, 8, &insert_str); // Insert at position 8 ("_string")
        CHECK(std::string(str.data) == "original_inserted_string");

        ucb_str_release(&str);
        ucb_str_release(&insert_str);
    }

    SUBCASE("ucb_str_insert_c")
    {
        ucb_str str;
        ucb_str_init_c(&str, "original_string");
        ucb_str_insert_c(&str, 8, "_inserted_c");

        CHECK(std::string(str.data) == "original_inserted_c_string");

        ucb_str_release(&str);
    }

    SUBCASE("ucb_str_substr")
    {
        ucb_str* str = ucb_str_new_c("substring_test");
        REQUIRE(str != nullptr);

        ucb_str* substr = ucb_str_substr(str, 3, 10); // Extract "string_"
        REQUIRE(substr != nullptr);
        CHECK(std::string(substr->data) == "string_");

        ucb_str_free(str);
        ucb_str_free(substr);
    }

    SUBCASE("ucb_str_substr_wrapped")
    {
        ucb_str* str = ucb_str_new_c("substring_wrapped_test");
        REQUIRE(str != nullptr);

        ucb_str* substr = ucb_str_substr_wrapped(str, 10, 17); // Extract "wrapped"
        REQUIRE(substr != nullptr);
        CHECK(strncmp(substr->data, "wrapped", 7) == 0);
        CHECK(ucb_str_is_owned(substr) == false); // Should be wrapped

        ucb_str_free(str);
        ucb_str_free(substr);
    }

    UCB_MEMTRACK_POP();
}

TEST_CASE("string utf-8 handling")
{
    UCB_MEMTRACK_PUSH();

    SUBCASE("utf-8 basic")
    {
        const char* test_string = "Hello 世界";
        ucb_str str;
        ucb_str_init_c(&str, test_string); // Contains multi-byte UTF-8 characters

        // Length should count bytes, not characters
        CHECK(ucb_str_len(&str) == strlen(test_string));
        CHECK(ucb_str_num_char(&str) == 8); // "Hello " (6) + 2 unicode characters

        ucb_str_release(&str);
    }

    UCB_MEMTRACK_POP();
}

TEST_SUITE_END();
