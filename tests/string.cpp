/**
 * @file string.cpp
 *
 * This file is part of the UCB project
 * - SPDX-FileCopyrightText: © 2026 Åke Svedin <ake@svedin.org>
 * - SPDX-License-Identifier: MIT
 *
 * @brief String tests
 */

#include "common.h"
#include "test_string_c.h"

#include <ucb/memdbg.h>
#include <ucb/memory.h>
#include <ucb/string.h>
#include <ucb/string_ex.h>

#include <doctest.h>

#include <cstdint>
#include <cstring>
#include <string>
#include <vector>

#ifdef _WIN32
#include <cwchar>
#endif

TEST_SUITE_BEGIN("string");

TEST_CASE("string - lifetime")
{
    UCB_MEMTRACK_PUSH();

    // Test ucb_str_make()
    SUBCASE("ucb_str_make")
    {
        ucb_str str = ucb_str_make();
        CHECK(ucb_str_cstr(&str) != nullptr);
        CHECK(strcmp(ucb_str_cstr(&str), "") == 0);
        CHECK(ucb_str_len(&str) == 0);
        CHECK(ucb_str_is_empty(&str) == true);
        CHECK(ucb_str_capacity(&str) == 0);
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
        CHECK(strlen(ucb_str_cstr(str)) == ucb_str_len(str));
        ucb_str_free(str);

        // Test with nullptr -> interned empty string
        str = ucb_str_new(nullptr, 0);
        REQUIRE(str != nullptr);
        CHECK(ucb_str_len(str) == 0);
        CHECK(ucb_str_is_empty(str) == true);
        CHECK(strcmp(ucb_str_cstr(str), "") == 0);
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

    // Test ucb_str_new_empty
    SUBCASE("ucb_str_new_empty")
    {
        ucb_str* str = ucb_str_new_empty();
        REQUIRE(str != nullptr);
        CHECK(ucb_str_len(str) == 0);
        CHECK(ucb_str_is_empty(str) == true);
        CHECK(strcmp(ucb_str_cstr(str), "") == 0);
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
        CHECK(ucb_str_capacity(cloned) > 0);

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
        CHECK(ucb_str_capacity(&str) == 12);
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

    // Test ucb_str_init_empty
    SUBCASE("ucb_str_init_empty")
    {
        ucb_str str;
        ucb_str_init_empty(&str);
        CHECK(ucb_str_len(&str) == 0);
        CHECK(ucb_str_is_empty(&str) == true);
        CHECK(strcmp(ucb_str_cstr(&str), "") == 0);
        CHECK(ucb_str_capacity(&str) == 0);
        ucb_str_release(&str);
    }

    UCB_MEMTRACK_POP();
}

TEST_CASE("string - destruction")
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

TEST_CASE("string - assignment and updating")
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
        CHECK(ucb_str_capacity(&dest) > 0);

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

    UCB_MEMTRACK_POP();
}

TEST_CASE("string - querying")
{
    UCB_MEMTRACK_PUSH();

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

        ucb_str empty;
        ucb_str_init_empty(&empty);
        CHECK(ucb_str_capacity(&empty) == 0);

        ucb_str_release(&str);
        ucb_str_release(&empty);
    }

    SUBCASE("ucb_str_used")
    {
        ucb_str str;
        ucb_str_init_c(&str, "used_test");
        size_t used = ucb_str_used(&str);
        CHECK(used >= ucb_str_len(&str));

        ucb_str empty;
        ucb_str_init_empty(&empty);
        CHECK(ucb_str_used(&empty) == 0);

        ucb_str_release(&str);
        ucb_str_release(&empty);
    }

    SUBCASE("ucb_str_avail")
    {
        ucb_str str;
        ucb_str_init_c(&str, "avail_test");
        size_t avail = ucb_str_avail(&str);
        CHECK(avail >= 0);

        ucb_str empty;
        ucb_str_init_empty(&empty);
        CHECK(ucb_str_avail(&empty) == 0);

        ucb_str_release(&str);
        ucb_str_release(&empty);
    }

    SUBCASE("ucb_str_cstr")
    {
        const char* test_str = "cstr_test";
        ucb_str str;
        ucb_str_init_c(&str, test_str);

        const char* cstr = ucb_str_cstr(&str);
        CHECK(strcmp(cstr, test_str) == 0);
        CHECK(strlen(cstr) == ucb_str_len(&str));

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

TEST_CASE("string - comparison and lookup")
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
        CHECK(pos2 == UCB_NPOS); // "missing" not found

        ucb_str_release(&str);
        ucb_str_release(&substr1);
        ucb_str_release(&substr2);
    }

    UCB_MEMTRACK_POP();
}

TEST_CASE("string - modification")
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
        CHECK(strcmp(ucb_str_cstr(&str), "") == 0);
        CHECK(ucb_str_capacity(&str) == 0);

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

    UCB_MEMTRACK_POP();
}

TEST_CASE("string - utf-8 handling")
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

    SUBCASE("num_char with combining marks")
    {
        // 'e' + U+0301 combining acute accent is a single perceived character.
        ucb_str str;
        ucb_str_init_c(&str, "e\u0301");
        CHECK(ucb_str_len(&str) == 3);
        CHECK(ucb_str_num_char(&str) == 1);
        ucb_str_release(&str);
    }

    UCB_MEMTRACK_POP();
}

TEST_CASE("string - adopt and abandon")
{
    UCB_MEMTRACK_PUSH();

    SUBCASE("ucb_str_adopt")
    {
        char* data = (char*)ucb_malloc(6);
        REQUIRE(data != nullptr);
        memcpy(data, "hello", 6);

        ucb_str str;
        ucb_str_init_empty(&str);
        ucb_str_adopt(&str, data, 5, 6);

        CHECK(ucb_str_len(&str) == 5);
        CHECK(ucb_str_capacity(&str) == 6);
        CHECK(strcmp(ucb_str_cstr(&str), "hello") == 0);
        ucb_str_release(&str);
    }

    SUBCASE("ucb_str_adopt_c")
    {
        char* data = (char*)ucb_malloc(6);
        REQUIRE(data != nullptr);
        memcpy(data, "world", 6);

        ucb_str str;
        ucb_str_init_empty(&str);
        ucb_str_adopt_c(&str, data);

        CHECK(ucb_str_len(&str) == 5);
        CHECK(ucb_str_capacity(&str) == 6);
        CHECK(strcmp(ucb_str_cstr(&str), "world") == 0);
        ucb_str_release(&str);
    }

    SUBCASE("ucb_str_abandon owned")
    {
        ucb_str str;
        ucb_str_init_c(&str, "abandon_me");
        size_t len = 0;
        size_t alloc = 0;
        char* data = nullptr;

        bool owned = ucb_str_abandon(&str, &data, &len, &alloc);
        CHECK(owned == true);
        CHECK(len == 10);
        CHECK(alloc == 11);
        CHECK(strcmp(data, "abandon_me") == 0);
        // The string must now be zeroed
        CHECK(ucb_str_len(&str) == 0);
        CHECK(ucb_str_capacity(&str) == 0);
        ucb_free(data);
    }

    SUBCASE("ucb_str_abandon interned empty")
    {
        ucb_str str;
        ucb_str_init_empty(&str);

        char* data = nullptr;
        size_t len = 99;
        size_t alloc = 99;
        bool owned = ucb_str_abandon(&str, &data, &len, &alloc);
        CHECK(owned == false);
        CHECK(data != nullptr);
        CHECK(strcmp(data, "") == 0);
        CHECK(len == 0);
        CHECK(alloc == 0);
        // Interned "" must not be freed by the caller
    }

    SUBCASE("ucb_str_abandon_c")
    {
        ucb_str str;
        ucb_str_init_c(&str, "convenience");
        char* data = ucb_str_abandon_c(&str);
        REQUIRE(data != nullptr);
        CHECK(strcmp(data, "convenience") == 0);
        CHECK(ucb_str_len(&str) == 0);
        ucb_free(data);
    }

    UCB_MEMTRACK_POP();
}

TEST_CASE("string - case-insensitive comparison")
{
    UCB_MEMTRACK_PUSH();

    SUBCASE("ucb_str_icomp")
    {
        ucb_str str1, str2, str3;
        ucb_str_init_c(&str1, "Hello");
        ucb_str_init_c(&str2, "hELLo");
        ucb_str_init_c(&str3, "world");

        CHECK(ucb_str_icomp(&str1, &str2) == 0);
        CHECK(ucb_str_icomp(&str1, &str3) < 0);
        CHECK(ucb_str_icomp(&str3, &str1) > 0);

        ucb_str_release(&str1);
        ucb_str_release(&str2);
        ucb_str_release(&str3);
    }

    SUBCASE("ucb_str_cmp_func")
    {
        ucb_str str1, str2;
        ucb_str_init_c(&str1, "abc");
        ucb_str_init_c(&str2, "abc");

        CHECK(ucb_str_cmp_func(&str1, &str2) == 0);

        ucb_str_release(&str1);
        ucb_str_release(&str2);
    }

    UCB_MEMTRACK_POP();
}

TEST_CASE("string - next char")
{
    UCB_MEMTRACK_PUSH();

    // "e" + U+0301 (combining acute) + "x": one grapheme followed by x
    ucb_str str;
    ucb_str_init_c(&str, "e\u0301x");
    REQUIRE(ucb_str_len(&str) == 4);

    CHECK(ucb_str_next_char(&str, 0) == 3);        // Boundary starting the next cluster after 'é'
    CHECK(ucb_str_next_char(&str, 3) == UCB_NPOS); // 'x' is the final cluster
    CHECK(ucb_str_next_char(&str, 4) == UCB_NPOS);

    ucb_str_release(&str);

    UCB_MEMTRACK_POP();
}

TEST_CASE("string - append and insert variants")
{
    UCB_MEMTRACK_PUSH();

    SUBCASE("ucb_str_append_cstr with explicit length")
    {
        ucb_str str;
        ucb_str_init_c(&str, "abc");
        ucb_str_append_cstr(&str, "XYZ", 2);
        CHECK(strcmp(ucb_str_cstr(&str), "abcXY") == 0);
        ucb_str_release(&str);
    }

    SUBCASE("ucb_str_append_cp")
    {
        ucb_str str;
        ucb_str_init_empty(&str);
        const ucb_cp cps[] = {0x48, 0xE9, 0x1F600}; // H, é, 😀
        ucb_str_append_cp(&str, cps, 3, nullptr);

        CHECK(ucb_str_len(&str) == 7); // 1 + 2 + 4 bytes
        CHECK(ucb_str_num_char(&str) == 3);
        CHECK(strcmp(ucb_str_cstr(&str), "H\xC3\xA9\xF0\x9F\x98\x80") == 0);
        ucb_str_release(&str);
    }

    SUBCASE("ucb_str_append_cp invalid codepoint")
    {
        ucb_str str;
        ucb_str_init_empty(&str);
        const ucb_cp bad[] = {0x41, 0x110000};
        ucb_error* err = nullptr;
        ucb_str_append_cp(&str, bad, 2, &err);
        CHECK(UCB_IS_THROWN(err));
        ucb_error_clear(&err);
        // Encoding is all-or-nothing on error: nothing is appended
        CHECK(ucb_str_len(&str) == 0);
        ucb_str_release(&str);
    }

    SUBCASE("ucb_str_insert_cstr with explicit length")
    {
        ucb_str str;
        ucb_str_init_c(&str, "abcdef");
        ucb_str_insert_cstr(&str, 3, "XY", 2);
        CHECK(strcmp(ucb_str_cstr(&str), "abcXYdef") == 0);
        ucb_str_release(&str);
    }

    SUBCASE("ucb_str_insert_cp by character index")
    {
        ucb_str str;
        ucb_str_init_c(&str, "hello");
        const ucb_cp cps[] = {0x58}; // 'X'
        ucb_str_insert_cp(&str, 2, cps, 1, nullptr);
        CHECK(strcmp(ucb_str_cstr(&str), "heXllo") == 0);
        ucb_str_release(&str);
    }

    UCB_MEMTRACK_POP();
}

TEST_CASE("string - concatenation")
{
    UCB_MEMTRACK_PUSH();

    ucb_str* a = ucb_str_new_c("Hello, ");
    ucb_str* b = ucb_str_new_c("UCB");
    ucb_str* c = ucb_str_new_c("!");

    SUBCASE("ucb_str_concat")
    {
        ucb_str* result = ucb_str_concat(a, b, c, nullptr);
        REQUIRE(result != nullptr);
        CHECK(strcmp(ucb_str_cstr(result), "Hello, UCB!") == 0);
        CHECK(ucb_str_capacity(result) > 0);
        ucb_str_free(result);
    }

    SUBCASE("ucb_str_concat with only first")
    {
        ucb_str* result = ucb_str_concat(a, nullptr);
        REQUIRE(result != nullptr);
        CHECK(strcmp(ucb_str_cstr(result), "Hello, ") == 0);
        ucb_str_free(result);
    }

    SUBCASE("ucb_str_concat empty")
    {
        ucb_str* empty = ucb_str_new_empty();
        ucb_str* result = ucb_str_concat(empty, nullptr);
        REQUIRE(result != nullptr);
        CHECK(ucb_str_len(result) == 0);
        ucb_str_free(empty);
        ucb_str_free(result);
    }

    ucb_str_free(a);
    ucb_str_free(b);
    ucb_str_free(c);

    UCB_MEMTRACK_POP();
}

TEST_CASE("string - substring semantics")
{
    UCB_MEMTRACK_PUSH();

    ucb_str* str = ucb_str_new_c("substring_test");

    SUBCASE("ucb_str_substr byte range")
    {
        ucb_str* sub = ucb_str_substr(str, 3, 10);
        REQUIRE(sub != nullptr);
        CHECK(ucb_str_capacity(sub) > 0);
        CHECK(ucb_str_len(sub) == 7);
        CHECK(strncmp(ucb_str_cstr(sub), "string_", 7) == 0);
        ucb_str_free(sub);
    }

    SUBCASE("ucb_str_substr with UCB_NPOS end")
    {
        ucb_str* sub = ucb_str_substr(str, 10, UCB_NPOS);
        REQUIRE(sub != nullptr);
        CHECK(strcmp(ucb_str_cstr(sub), "test") == 0);
        ucb_str_free(sub);
    }

    ucb_str_free(str);

    UCB_MEMTRACK_POP();
}

TEST_CASE("string - lookup with position")
{
    UCB_MEMTRACK_PUSH();

    ucb_str str, substr;
    ucb_str_init_c(&str, "abcabcabc");
    ucb_str_init_c(&substr, "abc");

    CHECK(ucb_str_find(&str, &substr, 0) == 0);
    CHECK(ucb_str_find(&str, &substr, 1) == 3);
    CHECK(ucb_str_find(&str, &substr, 4) == 6);

    // Not found
    ucb_str missing;
    ucb_str_init_c(&missing, "zzz");
    CHECK(ucb_str_find(&str, &missing, 0) == UCB_NPOS);
    ucb_str_release(&missing);

    ucb_str_release(&str);
    ucb_str_release(&substr);

    UCB_MEMTRACK_POP();
}

TEST_CASE("string - capacity querying")
{
    UCB_MEMTRACK_PUSH();

    SUBCASE("owned string used/capacity/avail")
    {
        ucb_str str;
        ucb_str_init_c(&str, "abc"); // alloc = 4 (3 bytes + null)
        CHECK(ucb_str_capacity(&str) == 4);
        CHECK(ucb_str_used(&str) == 4); // includes null-terminator
        CHECK(ucb_str_avail(&str) == 0);
        ucb_str_release(&str);
    }

    SUBCASE("reserve then fit")
    {
        ucb_str* str = ucb_str_new_c("fit");
        REQUIRE(str != nullptr);
        REQUIRE(ucb_str_reserve(str, 64));
        CHECK(ucb_str_avail(str) >= 64);

        CHECK(ucb_str_fit(str) == true);
        CHECK(ucb_str_capacity(str) == ucb_str_len(str) + 1);

        ucb_str_free(str);
    }

    SUBCASE("reserve on interned empty allocates")
    {
        ucb_str str;
        ucb_str_init_empty(&str);
        CHECK(ucb_str_capacity(&str) == 0);
        CHECK(ucb_str_reserve(&str, 8) == true);
        CHECK(ucb_str_capacity(&str) == ucb_str_len(&str) + 8 + 1);
        CHECK(strcmp(ucb_str_cstr(&str), "") == 0);
        ucb_str_release(&str);
    }

    SUBCASE("clone of interned empty is owned")
    {
        ucb_str empty;
        ucb_str_init_empty(&empty);
        ucb_str* clone = ucb_str_clone(&empty);
        REQUIRE(clone != nullptr);
        CHECK(ucb_str_capacity(clone) > 0);
        CHECK(ucb_str_equal(clone, &empty));
        ucb_str_free(clone);
        ucb_str_release(&empty);
    }

    UCB_MEMTRACK_POP();
}

TEST_CASE("string - case and normalization modification")
{
    UCB_MEMTRACK_PUSH();

    SUBCASE("ucb_str_to_lower")
    {
        ucb_str str;
        ucb_str_init_c(&str, "HeLLo");
        CHECK(ucb_str_to_lower(&str) == true);
        CHECK(strcmp(ucb_str_cstr(&str), "hello") == 0);
        ucb_str_release(&str);
    }

    SUBCASE("ucb_str_to_upper")
    {
        ucb_str str;
        ucb_str_init_c(&str, "hello");
        CHECK(ucb_str_to_upper(&str) == true);
        CHECK(strcmp(ucb_str_cstr(&str), "HELLO") == 0);
        ucb_str_release(&str);
    }

    SUBCASE("ucb_str_to_title")
    {
        ucb_str str;
        ucb_str_init_c(&str, "hello world");
        CHECK(ucb_str_to_title(&str) == true);
        CHECK(strcmp(ucb_str_cstr(&str), "Hello World") == 0);
        ucb_str_release(&str);
    }

    SUBCASE("ucb_str_casefold")
    {
        ucb_str str;
        ucb_str_init_c(&str,
                       "Stra\xC3\x9F"
                       "e"); // "Straße"
        CHECK(ucb_str_casefold(&str) == true);
        CHECK(strcmp(ucb_str_cstr(&str), "strasse") == 0);
        ucb_str_release(&str);
    }

    SUBCASE("ucb_str_normalize")
    {
        // Decomposed "Hé" (H + U+0301) normalized to NFC becomes U+00E9.
        ucb_str str;
        ucb_str_init_c(&str, "He\u0301llo");
        CHECK(ucb_str_normalize(&str, UCB_NORM_NFC) == true);
        CHECK(strcmp(ucb_str_cstr(&str), "H\xC3\xA9llo") == 0);

        CHECK(ucb_str_normalize(&str, UCB_NORM_NFD) == true);
        CHECK(strcmp(ucb_str_cstr(&str), "He\u0301llo") == 0);
        ucb_str_release(&str);
    }

    UCB_MEMTRACK_POP();
}

TEST_CASE("string - self aliasing")
{
    UCB_MEMTRACK_PUSH();

    SUBCASE("ucb_str_copy self")
    {
        ucb_str str;
        ucb_str_init_c(&str, "self_copy");
        CHECK(ucb_str_copy(&str, &str) == true);
        CHECK(strcmp(ucb_str_cstr(&str), "self_copy") == 0);
        ucb_str_release(&str);
    }

    SUBCASE("ucb_str_assign self full")
    {
        ucb_str str;
        ucb_str_init_c(&str, "self_assign");
        CHECK(ucb_str_assign(&str, ucb_str_cstr(&str), str.size) == true);
        CHECK(strcmp(ucb_str_cstr(&str), "self_assign") == 0);
        ucb_str_release(&str);
    }

    SUBCASE("ucb_str_assign self substring")
    {
        ucb_str str;
        ucb_str_init_c(&str, "abcdef");
        CHECK(ucb_str_assign(&str, ucb_str_cstr(&str) + 2, 3) == true); // "cde"
        CHECK(strcmp(ucb_str_cstr(&str), "cde") == 0);
        ucb_str_release(&str);
    }

    SUBCASE("ucb_str_append self")
    {
        ucb_str str;
        ucb_str_init_c(&str, "abc");
        ucb_str_append(&str, &str);
        CHECK(strcmp(ucb_str_cstr(&str), "abcabc") == 0);
        CHECK(ucb_str_len(&str) == 6);
        ucb_str_release(&str);
    }

    SUBCASE("ucb_str_append_cstr self")
    {
        ucb_str str;
        ucb_str_init_c(&str, "1234");
        ucb_str_append_cstr(&str, str.data, 2);
        CHECK(strcmp(ucb_str_cstr(&str), "123412") == 0);
        ucb_str_release(&str);
    }

    SUBCASE("ucb_str_insert self")
    {
        ucb_str str;
        ucb_str_init_c(&str, "hello");
        ucb_str_insert(&str, 2, &str);
        CHECK(strcmp(ucb_str_cstr(&str), "hehellollo") == 0);
        ucb_str_release(&str);
    }

    SUBCASE("ucb_str_find position past string")
    {
        ucb_str str, sub;
        ucb_str_init_c(&str, "abc");
        ucb_str_init_c(&sub, "a");
        CHECK(ucb_str_find(&str, &sub, 10) == UCB_NPOS);
        ucb_str_release(&str);
        ucb_str_release(&sub);
    }

    UCB_MEMTRACK_POP();
}

TEST_CASE("string - UCB_CSTR (C)")
{
    CHECK(test_string_c() == 0);
}

TEST_CASE("string - UCB_CSTR")
{
    UCB_MEMTRACK_PUSH();

    SUBCASE("ucb_str pointer")
    {
        ucb_str* s = ucb_str_new_c("hello");
        REQUIRE(s != nullptr);
        const char* p = UCB_CSTR(s);
        CHECK(strcmp(p, "hello") == 0);
        CHECK(p == ucb_str_cstr(s));
        ucb_str_free(s);
    }

    SUBCASE("const ucb_str pointer")
    {
        ucb_str* s = ucb_str_new_c("world");
        const ucb_str* cs = s;
        const char* p = UCB_CSTR(cs);
        CHECK(strcmp(p, "world") == 0);
        ucb_str_free(s);
    }

    SUBCASE("char pointers and literal")
    {
        char buf[] = "buffer";
        const char* cstr = "const";
        CHECK(strcmp(UCB_CSTR(buf), "buffer") == 0);
        CHECK(strcmp(UCB_CSTR(cstr), "const") == 0);
        CHECK(strcmp(UCB_CSTR("literal"), "literal") == 0);
    }

    SUBCASE("std::string")
    {
        std::string s("std_string");
        const char* p = UCB_CSTR(s);
        CHECK(strcmp(p, "std_string") == 0);
        CHECK(p == s.c_str());
    }

    UCB_MEMTRACK_POP();
}

TEST_CASE_FIXTURE(TestFailureFixture, "string - invalid input aborts")
{
    UCB_MEMTRACK_PUSH();

    SUBCASE("init with embedded null")
    {
        ucb_str str;
        CHECK_ABORTS(ucb_str_init(&str, "a\0b", 3));
        CHECK(num_aborts == 1);
        REQUIRE(num_error == 1);
        CHECK(errors[0].code == UCB_ERROR_INVALID_ARG);
    }

    SUBCASE("assign with embedded null")
    {
        ucb_str str;
        ucb_str_init_empty(&str);
        CHECK_ABORTS(ucb_str_assign(&str, "a\0b", 3));
        ucb_str_release(&str);
    }

    SUBCASE("append_cstr with embedded null")
    {
        ucb_str str;
        ucb_str_init_c(&str, "abc");
        CHECK_ABORTS(ucb_str_append_cstr(&str, "a\0b", 3));
        ucb_str_release(&str);
    }

    SUBCASE("insert_cstr with embedded null")
    {
        ucb_str str;
        ucb_str_init_c(&str, "abcdef");
        CHECK_ABORTS(ucb_str_insert_cstr(&str, 3, "a\0b", 3));
        ucb_str_release(&str);
    }

    SUBCASE("append_cp zero codepoint")
    {
        ucb_str str;
        ucb_str_init_empty(&str);
        const ucb_cp cps[] = {0x41, 0x0};
        CHECK_ABORTS(ucb_str_append_cp(&str, cps, 2, nullptr));
        ucb_str_release(&str);
    }

    SUBCASE("insert_cp zero codepoint")
    {
        ucb_str str;
        ucb_str_init_c(&str, "hello");
        const ucb_cp cps[] = {0x0};
        CHECK_ABORTS(ucb_str_insert_cp(&str, 2, cps, 1, nullptr));
        ucb_str_release(&str);
    }

    SUBCASE("adopt missing terminator")
    {
        char* data = (char*)ucb_malloc(6);
        REQUIRE(data != nullptr);
        memcpy(data, "hello", 6);
        data[5] = 'x'; // No terminator at data[len]

        ucb_str str;
        ucb_str_init_empty(&str);
        CHECK_ABORTS(ucb_str_adopt(&str, data, 5, 6));
        // Aborted before taking ownership; caller still owns data
        ucb_free(data);
    }

    SUBCASE("adopt alloc too small")
    {
        char* data = (char*)ucb_malloc(5);
        REQUIRE(data != nullptr);
        memcpy(data, "hello", 5); // No room for a terminator

        ucb_str str;
        ucb_str_init_empty(&str);
        CHECK_ABORTS(ucb_str_adopt(&str, data, 5, 5));
        ucb_free(data);
    }

    UCB_MEMTRACK_POP();
}

#ifdef _WIN32
TEST_CASE("string - wide string conversion")
{
    UCB_MEMTRACK_PUSH();

    SUBCASE("round trip")
    {
        ucb_error* err = nullptr;
        // L"H\x00E9llo" is UTF-16 for "Héllo"
        ucb_str* str = ucb_str_from_wchar(L"H\x00E9llo", 0, &err);
        REQUIRE(str != nullptr);
        CHECK(!UCB_IS_THROWN(err));
        CHECK(ucb_str_len(str) == 6); // H + 2-byte é + llo

        size_t wlen = 0;
        wchar_t* wstr = ucb_str_to_wchar(str, &wlen, &err);
        REQUIRE(wstr != nullptr);
        CHECK(!UCB_IS_THROWN(err));
        CHECK(wlen == 5);
        CHECK(wcscmp(wstr, L"H\x00E9llo") == 0);

        ucb_free(wstr);
        ucb_str_free(str);
    }

    SUBCASE("empty string")
    {
        ucb_error* err = nullptr;
        ucb_str* str = ucb_str_from_wchar(L"", 0, &err);
        REQUIRE(str != nullptr);
        CHECK(!UCB_IS_THROWN(err));
        CHECK(ucb_str_len(str) == 0);
        ucb_str_free(str);
    }

    UCB_MEMTRACK_POP();
}
#endif

TEST_SUITE_END();
