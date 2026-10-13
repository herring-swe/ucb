/**
 * @file path.cpp
 *
 * This file is part of the UCB project
 * - SPDX-FileCopyrightText: © 2026 Åke Svedin <ake@svedin.org>
 * - SPDX-License-Identifier: MIT
 *
 * @brief Path tests
 */

#include "common.h"

#include <ucb/memdbg.h>
#include <ucb/memory.h>
#include <ucb/path.h>
#include <ucb/string.h>
#include <ucb/string_ex.h>
#include <ucb/ucb.h>

#include <doctest.h>

#include <cstring>
#include <string>

static void check_path(const ucb_path* path, const char* dir, const char* stem, const char* ext)
{
    CHECK(std::strcmp(ucb_str_cstr(ucb_path_dir(path)), dir) == 0);
    CHECK(std::strcmp(ucb_str_cstr(ucb_path_stem(path)), stem) == 0);
    CHECK(std::strcmp(ucb_str_cstr(ucb_path_ext(path)), ext) == 0);
}

static void check_cstr(ucb_path* path, const char* expected)
{
    CHECK(std::string(ucb_path_cstr(path)) == std::string(expected));
    CHECK(ucb_path_len(path) == std::strlen(expected));
}

static void check_cstr_value(const ucb_str* str, const char* expected)
{
    CAPTURE(expected);
    REQUIRE(str != nullptr);
    CHECK(std::string(ucb_str_cstr(str)) == std::string(expected));
}

TEST_SUITE_BEGIN("path");

TEST_CASE("path - lifetime")
{
    UCB_MEMTRACK_PUSH();

    SUBCASE("make")
    {
        ucb_path path = ucb_path_make();
        CHECK(ucb_path_is_empty(&path));
        check_cstr(&path, "");
        CHECK(ucb_path_num_parts(&path) == 0);
        ucb_path_release(&path);
    }

    SUBCASE("init")
    {
        ucb_path path = ucb_path_make();
        REQUIRE(ucb_path_init_style(&path, UCB_STR("a/b/c.txt"), UCB_PATH_STYLE_POSIX));
        CHECK_FALSE(ucb_path_is_empty(&path));
        check_path(&path, "a/b", "c", ".txt");
        ucb_path_release(&path);
    }

    SUBCASE("init empty")
    {
        ucb_path path = ucb_path_make();
        REQUIRE(ucb_path_init_style(&path, UCB_STR(""), UCB_PATH_STYLE_POSIX));
        CHECK(ucb_path_is_empty(&path));
        ucb_path_release(&path);
    }

    SUBCASE("new/free")
    {
        ucb_path* path = ucb_path_new_style(UCB_STR("a/b/c.txt"), UCB_PATH_STYLE_POSIX);
        REQUIRE(path != nullptr);
        check_path(path, "a/b", "c", ".txt");
        ucb_path_free(path);
    }

    SUBCASE("clone")
    {
        ucb_path* path = ucb_path_new_style(UCB_STR("a/b/c.txt"), UCB_PATH_STYLE_POSIX);
        REQUIRE(path != nullptr);
        ucb_path* clone = ucb_path_clone(path);
        REQUIRE(clone != nullptr);
        CHECK(ucb_path_equals(path, clone));
        check_path(clone, "a/b", "c", ".txt");
        ucb_path_free(path);
        ucb_path_free(clone);
    }

    SUBCASE("copy")
    {
        ucb_path src = ucb_path_make();
        ucb_path dst = ucb_path_make();
        REQUIRE(ucb_path_init_style(&src, UCB_STR("a/b/c.txt"), UCB_PATH_STYLE_POSIX));
        REQUIRE(ucb_path_copy(&dst, &src));
        CHECK(ucb_path_equals(&src, &dst));
        check_path(&dst, "a/b", "c", ".txt");
        ucb_path_release(&src);
        ucb_path_release(&dst);
    }

    SUBCASE("clear and assign")
    {
        ucb_path* path = ucb_path_new_style(UCB_STR("a/b/c.txt"), UCB_PATH_STYLE_POSIX);
        REQUIRE(path != nullptr);
        ucb_path_clear(path);
        CHECK(ucb_path_is_empty(path));
        REQUIRE(ucb_path_assign(path, UCB_STR("x/y.z")));
        check_path(path, "x", "y", ".z");
        ucb_path_free(path);
    }

    UCB_MEMTRACK_POP();
}

TEST_CASE_FIXTURE(TestFailureFixture, "path - null string arg validation")
{
    CHECK_ABORTS(ucb_path_new(nullptr));
    CHECK_ABORTS(ucb_path_new_style(nullptr, UCB_PATH_STYLE_POSIX));

    ucb_path path = ucb_path_make();
    CHECK_ABORTS(ucb_path_init_style(&path, nullptr, UCB_PATH_STYLE_POSIX));
    CHECK_ABORTS(ucb_path_assign(&path, nullptr));
    ucb_path_release(&path);
}

TEST_CASE("path - parse components")
{
    UCB_MEMTRACK_PUSH();

    struct case_t
    {
        const char* input;
        const char* dir;
        const char* stem;
        const char* ext;
    };
    const case_t cases[] = {
        {"a/b/c.txt", "a/b", "c", ".txt"},
        {"c.txt", "", "c", ".txt"},
        {"/c.txt", "/", "c", ".txt"},
        {"a/b/", "a/b", "", ""},
        {"a//b", "a", "b", ""},
        {"..", "", "..", ""},
        {".", "", ".", ""},
        {"a/b/c", "a/b", "c", ""},
        {"/", "/", "", ""},
    };

    for (const case_t& c : cases)
    {
        ucb_path path = ucb_path_make();
        REQUIRE(ucb_path_init_style(&path, UCB_STR(c.input), UCB_PATH_STYLE_POSIX));
        CAPTURE(c.input);
        check_path(&path, c.dir, c.stem, c.ext);
        ucb_path_release(&path);
    }

#ifdef _WIN32
    SUBCASE("windows drive and UNC")
    {
        ucb_path path = ucb_path_make();
        REQUIRE(ucb_path_init_style(&path, UCB_STR("C:\\a\\b.txt"), UCB_PATH_STYLE_WINDOWS));
        check_path(&path, "C:\\a", "b", ".txt");
        check_cstr(&path, "C:\\a\\b.txt");
        CHECK(ucb_path_is_absolute(&path));
        ucb_path_release(&path);

        path = ucb_path_make();
        REQUIRE(ucb_path_init_style(&path, UCB_STR("C:\\b.txt"), UCB_PATH_STYLE_WINDOWS));
        check_path(&path, "C:\\", "b", ".txt");
        check_cstr(&path, "C:\\b.txt");
        ucb_path_release(&path);

        path = ucb_path_make();
        REQUIRE(ucb_path_init_style(&path,
                                    UCB_STR("\\\\server\\share\\file.txt"),
                                    UCB_PATH_STYLE_WINDOWS));
        check_path(&path, "\\\\server\\share", "file", ".txt");
        check_cstr(&path, "\\\\server\\share\\file.txt");
        CHECK(ucb_path_is_absolute(&path));
        ucb_path_release(&path);
    }
#endif

    UCB_MEMTRACK_POP();
}

TEST_CASE("path - extensions")
{
    UCB_MEMTRACK_PUSH();

    struct case_t
    {
        const char* input;
        const char* stem;
        const char* ext;
    };
    const case_t cases[] = {
        {".bashrc", ".bashrc", ""},
        {"archive.tar.gz", "archive.tar", ".gz"},
        {"a.", "a.", ""},
        {"a.b", "a", ".b"},
        {".a.b", ".a", ".b"},
        {"..a", "..a", ""},
        {"a..b", "a.", ".b"},
    };

    for (const case_t& c : cases)
    {
        ucb_path path = ucb_path_make();
        REQUIRE(ucb_path_init_style(&path, UCB_STR(c.input), UCB_PATH_STYLE_POSIX));
        CAPTURE(c.input);
        CHECK(std::strcmp(ucb_str_cstr(ucb_path_stem(&path)), c.stem) == 0);
        CHECK(std::strcmp(ucb_str_cstr(ucb_path_ext(&path)), c.ext) == 0);
        ucb_path_release(&path);
    }

    SUBCASE("set_ext normalization")
    {
        ucb_path path = ucb_path_make();
        REQUIRE(ucb_path_init_style(&path, UCB_STR("a/b"), UCB_PATH_STYLE_POSIX));

        ucb_str ext = ucb_str_make();
        REQUIRE(ucb_str_assign_c(&ext, "tar.gz"));
        REQUIRE(ucb_path_set_ext(&path, &ext));
        check_cstr(&path, "a/b.tar.gz");
        ucb_str_assign_c(&ext, ".txt");
        REQUIRE(ucb_path_set_ext(&path, &ext));
        check_cstr(&path, "a/b.txt");
        ucb_str_assign_c(&ext, "");
        REQUIRE(ucb_path_set_ext(&path, &ext));
        check_cstr(&path, "a/b");
        ucb_str_release(&ext);
        ucb_path_release(&path);
    }

    SUBCASE("set_filename")
    {
        ucb_path path = ucb_path_make();
        REQUIRE(ucb_path_init_style(&path, UCB_STR("a/b/c.txt"), UCB_PATH_STYLE_POSIX));
        REQUIRE(ucb_path_set_filename(&path, UCB_STR("d.tar.gz")));
        check_path(&path, "a/b", "d.tar", ".gz");
        ucb_path_release(&path);
    }

    SUBCASE("replace_ext")
    {
        ucb_path path = ucb_path_make();
        REQUIRE(ucb_path_init_style(&path, UCB_STR("a/b/c.txt"), UCB_PATH_STYLE_POSIX));
        REQUIRE(ucb_path_replace_ext(&path, UCB_STR("md")));
        check_cstr(&path, "a/b/c.md");
        ucb_path_release(&path);
    }

    UCB_MEMTRACK_POP();
}

TEST_CASE("path - styles")
{
    UCB_MEMTRACK_PUSH();

    SUBCASE("explicit styles")
    {
        ucb_path path = ucb_path_make();
        REQUIRE(ucb_path_init_style(&path, UCB_STR("a/b/c.txt"), UCB_PATH_STYLE_POSIX));
        CHECK(ucb_path_get_style(&path) == UCB_PATH_STYLE_POSIX);
        check_cstr(&path, "a/b/c.txt");

        ucb_path_set_style(&path, UCB_PATH_STYLE_WINDOWS);
        CHECK(ucb_path_get_style(&path) == UCB_PATH_STYLE_WINDOWS);
        check_cstr(&path, "a\\b\\c.txt");

        ucb_path_set_style(&path, UCB_PATH_STYLE_POSIX);
        check_cstr(&path, "a/b/c.txt");
        ucb_path_release(&path);

        ucb_path* win = ucb_path_new_style(UCB_STR("a/b/c.txt"), UCB_PATH_STYLE_WINDOWS);
        REQUIRE(win != nullptr);
        CHECK(ucb_path_num_parts(win) == 3);
        ucb_str* part = ucb_path_part(win, 1);
        check_cstr_value(part, "b");
        ucb_str_free(part);
        ucb_path* parent = ucb_path_parent(win);
        REQUIRE(parent != nullptr);
        check_cstr(parent, "a\\b");
        ucb_path_free(parent);
        ucb_path_free(win);
    }

    SUBCASE("config default")
    {
        ucb_config prev = ucb_conf_get();

        ucb_config cfg = ucb_conf_get();
        cfg.default_path_style = UCB_PATH_STYLE_POSIX;
        ucb_conf_set(&cfg);

        ucb_path* path = ucb_path_new(UCB_STR("a/b/c.txt"));
        REQUIRE(path != nullptr);
        CHECK(ucb_path_get_style(path) == UCB_PATH_STYLE_POSIX);
        check_cstr(path, "a/b/c.txt");
        ucb_path_free(path);

        cfg.default_path_style = UCB_PATH_STYLE_WINDOWS;
        ucb_conf_set(&cfg);
        path = ucb_path_new(UCB_STR("a/b/c.txt"));
        REQUIRE(path != nullptr);
        check_cstr(path, "a\\b\\c.txt");
        ucb_path_free(path);

        ucb_conf_set(&prev);
    }

    SUBCASE("default separator")
    {
        ucb_config prev = ucb_conf_get();

        ucb_config cfg = ucb_conf_get();
        cfg.default_path_style = UCB_PATH_STYLE_POSIX;
        ucb_conf_set(&cfg);
        CHECK(ucb_path_sep() == '/');

        cfg.default_path_style = UCB_PATH_STYLE_WINDOWS;
        ucb_conf_set(&cfg);
        CHECK(ucb_path_sep() == '\\');

        ucb_conf_set(&prev);
    }

#ifdef _WIN32
    SUBCASE("windows input accepts both separators")
    {
        ucb_path path = ucb_path_make();
        REQUIRE(ucb_path_init_style(&path, UCB_STR("a\\b/c.txt"), UCB_PATH_STYLE_POSIX));
        check_path(&path, "a/b", "c", ".txt");
        check_cstr(&path, "a/b/c.txt");
        ucb_path_release(&path);
    }
#endif

    UCB_MEMTRACK_POP();
}

TEST_CASE("path - cstr and to_str")
{
    UCB_MEMTRACK_PUSH();

    ucb_path* path = ucb_path_new_style(UCB_STR("a/b/c.txt"), UCB_PATH_STYLE_POSIX);
    REQUIRE(path != nullptr);
    check_cstr(path, "a/b/c.txt");

    ucb_str* str = ucb_path_to_str(path);
    REQUIRE(str != nullptr);
    check_cstr_value(str, "a/b/c.txt");
    ucb_str_free(str);

    const ucb_str* borrowed = ucb_path_str(path);
    check_cstr_value(borrowed, "a/b/c.txt");

    // Cache invalidation after mutation
    REQUIRE(ucb_path_set_stem(path, UCB_STR("renamed")));
    check_cstr(path, "a/b/renamed.txt");
    CHECK(ucb_path_len(path) == std::strlen(ucb_path_cstr(path)));

    ucb_path_free(path);

    UCB_MEMTRACK_POP();
}

TEST_CASE("path - join append")
{
    UCB_MEMTRACK_PUSH();

    SUBCASE("relative join")
    {
        ucb_path base = ucb_path_make();
        REQUIRE(ucb_path_init_style(&base, UCB_STR("a/b/c"), UCB_PATH_STYLE_POSIX));
        ucb_path* joined = ucb_path_clone(&base);
        REQUIRE(joined != nullptr);
        REQUIRE(ucb_path_append_str(joined, UCB_STR("d.txt")));
        check_cstr(joined, "a/b/c/d.txt");
        ucb_path_free(joined);
        ucb_path_release(&base);
    }

    SUBCASE("absolute replacement")
    {
        ucb_path base = ucb_path_make();
        REQUIRE(ucb_path_init_style(&base, UCB_STR("a/b/c"), UCB_PATH_STYLE_POSIX));
        ucb_path* joined = ucb_path_clone(&base);
        REQUIRE(joined != nullptr);
        REQUIRE(ucb_path_append_str(joined, UCB_STR("/x/y.txt")));
        check_cstr(joined, "/x/y.txt");
        ucb_path_free(joined);
        ucb_path_release(&base);
    }

    SUBCASE("in-place append")
    {
        ucb_path* path = ucb_path_new_style(UCB_STR("a/b"), UCB_PATH_STYLE_POSIX);
        REQUIRE(path != nullptr);
        REQUIRE(ucb_path_append_str(path, UCB_STR("c/d.txt")));
        check_cstr(path, "a/b/c/d.txt");
        ucb_path_free(path);
    }

    SUBCASE("join with nested dir")
    {
        ucb_path base = ucb_path_make();
        REQUIRE(ucb_path_init_style(&base, UCB_STR("/a"), UCB_PATH_STYLE_POSIX));
        ucb_path* joined = ucb_path_clone(&base);
        REQUIRE(joined != nullptr);
        REQUIRE(ucb_path_append_str(joined, UCB_STR("x/y.txt")));
        check_cstr(joined, "/a/x/y.txt");
        ucb_path_free(joined);
        ucb_path_release(&base);
    }

    UCB_MEMTRACK_POP();
}

TEST_CASE("path - parent parts")
{
    UCB_MEMTRACK_PUSH();

    SUBCASE("relative")
    {
        ucb_path* path = ucb_path_new_style(UCB_STR("a/b/c.txt"), UCB_PATH_STYLE_POSIX);
        REQUIRE(path != nullptr);

        CHECK(ucb_path_num_parts(path) == 3);
        ucb_str* p0 = ucb_path_part(path, 0);
        ucb_str* p1 = ucb_path_part(path, 1);
        ucb_str* p2 = ucb_path_part(path, 2);
        check_cstr_value(p0, "a");
        check_cstr_value(p1, "b");
        check_cstr_value(p2, "c.txt");
        ucb_str_free(p0);
        ucb_str_free(p1);
        ucb_str_free(p2);

        ucb_path* parent = ucb_path_parent(path);
        REQUIRE(parent != nullptr);
        check_cstr(parent, "a/b");
        ucb_path_free(parent);

        ucb_vector_str* parts = ucb_path_parts(path);
        REQUIRE(parts != nullptr);
        REQUIRE(ucb_vector_str_size(parts) == 3);
        check_cstr_value(ucb_vector_str_get(parts, 0), "a");
        check_cstr_value(ucb_vector_str_get(parts, 1), "b");
        check_cstr_value(ucb_vector_str_get(parts, 2), "c.txt");
        ucb_vector_str_free_full(parts);

        ucb_vector_str* parents = ucb_path_parents(path);
        REQUIRE(parents != nullptr);
        REQUIRE(ucb_vector_str_size(parents) == 2);
        check_cstr_value(ucb_vector_str_get(parents, 0), "a/b");
        check_cstr_value(ucb_vector_str_get(parents, 1), "a");
        ucb_vector_str_free_full(parents);

        ucb_path_free(path);
    }

    SUBCASE("absolute with anchor")
    {
        ucb_path* path = ucb_path_new_style(UCB_STR("/a/b/c.txt"), UCB_PATH_STYLE_POSIX);
        REQUIRE(path != nullptr);

        CHECK(ucb_path_num_parts(path) == 4);
        ucb_str* anchor = ucb_path_part(path, 0);
        check_cstr_value(anchor, "/");
        ucb_str_free(anchor);

        ucb_path* parent = ucb_path_parent(path);
        REQUIRE(parent != nullptr);
        check_cstr(parent, "/a/b");
        ucb_path_free(parent);

        ucb_vector_str* parents = ucb_path_parents(path);
        REQUIRE(parents != nullptr);
        REQUIRE(ucb_vector_str_size(parents) == 3);
        check_cstr_value(ucb_vector_str_get(parents, 0), "/a/b");
        check_cstr_value(ucb_vector_str_get(parents, 1), "/a");
        check_cstr_value(ucb_vector_str_get(parents, 2), "/");
        ucb_vector_str_free_full(parents);

        ucb_path_free(path);
    }

    SUBCASE("parent of root is itself")
    {
        ucb_path* path = ucb_path_new_style(UCB_STR("/"), UCB_PATH_STYLE_POSIX);
        REQUIRE(path != nullptr);
        ucb_path* parent = ucb_path_parent(path);
        REQUIRE(parent != nullptr);
        check_cstr(parent, "/");
        ucb_path_free(parent);
        ucb_path_free(path);
    }

    SUBCASE("parent drops filename only")
    {
        ucb_path* path = ucb_path_new_style(UCB_STR("a/b/"), UCB_PATH_STYLE_POSIX);
        REQUIRE(path != nullptr);
        ucb_path* parent = ucb_path_parent(path);
        REQUIRE(parent != nullptr);
        check_cstr(parent, "a");
        ucb_path_free(parent);
        ucb_path_free(path);
    }

    UCB_MEMTRACK_POP();
}

TEST_CASE("path - normalize")
{
    UCB_MEMTRACK_PUSH();

    struct case_t
    {
        const char* input;
        const char* expected;
    };
    const case_t cases[] = {
        {"a/./b", "a/b"},
        {"a/b/../c", "a/c"},
        {"/a/../..", "/"},
        {"a//b", "a/b"},
        {"a/b/", "a/b"},
        {".", "."},
        {"..", ".."},
        {"a/..", "."},
        {"a/../../b", "../b"},
        {"../..", "../.."},
        {"../../a", "../../a"},
        {"a/../../..", "../.."},
        {"/..", "/"},
        {"/../..", "/"},
        {"../a/../b", "../b"},
        {"/a/b/../c", "/a/c"},
        {"", ""},
    };

    for (const case_t& c : cases)
    {
        ucb_path path = ucb_path_make();
        REQUIRE(ucb_path_init_style(&path, UCB_STR(c.input), UCB_PATH_STYLE_POSIX));
        REQUIRE(ucb_path_normalize(&path));
        ucb_str* result = ucb_path_to_str(&path);
        CAPTURE(std::string(c.input));
        check_cstr_value(result, c.expected);
        ucb_str_free(result);
        ucb_path_release(&path);
    }

    SUBCASE("in place")
    {
        ucb_path* path = ucb_path_new_style(UCB_STR("a/./b/../c.txt"), UCB_PATH_STYLE_POSIX);
        REQUIRE(path != nullptr);
        REQUIRE(ucb_path_normalize(path));
        check_cstr(path, "a/c.txt");
        ucb_path_free(path);
    }

    UCB_MEMTRACK_POP();
}

TEST_CASE("path - compare")
{
    UCB_MEMTRACK_PUSH();

    ucb_path* a = ucb_path_new_style(UCB_STR("a/b/c.txt"), UCB_PATH_STYLE_POSIX);
    ucb_path* b = ucb_path_new_style(UCB_STR("a/b/c.txt"), UCB_PATH_STYLE_POSIX);
    ucb_path* c = ucb_path_new_style(UCB_STR("a/b/d.txt"), UCB_PATH_STYLE_POSIX);
    REQUIRE(a != nullptr);
    REQUIRE(b != nullptr);
    REQUIRE(c != nullptr);

    CHECK(ucb_path_equals(a, b));
    CHECK_FALSE(ucb_path_equals(a, c));
    CHECK(ucb_path_comp(a, b) == 0);
    CHECK(ucb_path_comp(a, c) < 0);

    ucb_str* upper = ucb_str_new_c("A/B/C.TXT");
    REQUIRE(upper != nullptr);
    ucb_path* up = ucb_path_new_style(upper, UCB_PATH_STYLE_POSIX);
    ucb_str_free(upper);
    REQUIRE(up != nullptr);
    CHECK(ucb_path_icomp(a, up) == 0);
    CHECK(ucb_path_comp(a, up) != 0);
    ucb_path_free(up);

    ucb_path eq_a = ucb_path_make();
    ucb_path_init(&eq_a, UCB_STR("a/b"));
    ucb_path eq_b = ucb_path_make();
    ucb_path_init(&eq_b, UCB_STR("a/b"));
    CHECK(ucb_path_equals(&eq_a, &eq_b));

    ucb_path eq_c = ucb_path_make();
    ucb_path_init(&eq_c, UCB_STR("a/c"));
    CHECK_FALSE(ucb_path_equals(&eq_a, &eq_c));

    ucb_path comp_a = ucb_path_make();
    ucb_path_init(&comp_a, UCB_STR("a"));
    ucb_path comp_b = ucb_path_make();
    ucb_path_init(&comp_b, UCB_STR("b"));
    CHECK(ucb_path_comp(&comp_a, &comp_b) < 0);

    ucb_path icomp_a = ucb_path_make();
    ucb_path_init(&icomp_a, UCB_STR("A"));
    ucb_path icomp_b = ucb_path_make();
    ucb_path_init(&icomp_b, UCB_STR("a"));
    CHECK(ucb_path_icomp(&icomp_a, &icomp_b) == 0);

    ucb_path_release(&eq_a);
    ucb_path_release(&eq_b);
    ucb_path_release(&eq_c);
    ucb_path_release(&comp_a);
    ucb_path_release(&comp_b);
    ucb_path_release(&icomp_a);
    ucb_path_release(&icomp_b);

    ucb_path_free(a);
    ucb_path_free(b);
    ucb_path_free(c);

    UCB_MEMTRACK_POP();
}

TEST_CASE("path - is_inside")
{
    UCB_MEMTRACK_PUSH();

    ucb_path* dir = ucb_path_new_style(UCB_STR("/a/b"), UCB_PATH_STYLE_POSIX);
    ucb_path* child = ucb_path_new_style(UCB_STR("/a/b/c.txt"), UCB_PATH_STYLE_POSIX);
    ucb_path* sibling = ucb_path_new_style(UCB_STR("/a/x"), UCB_PATH_STYLE_POSIX);
    ucb_path* root = ucb_path_new_style(UCB_STR("/"), UCB_PATH_STYLE_POSIX);
    ucb_path* relative = ucb_path_new_style(UCB_STR("a/b"), UCB_PATH_STYLE_POSIX);
    REQUIRE(dir != nullptr);
    REQUIRE(child != nullptr);
    REQUIRE(sibling != nullptr);
    REQUIRE(root != nullptr);
    REQUIRE(relative != nullptr);

    CHECK(ucb_path_is_inside(child, dir));
    CHECK(ucb_path_is_inside(dir, child));
    CHECK_FALSE(ucb_path_is_inside(dir, sibling));
    CHECK_FALSE(ucb_path_is_inside(dir, dir));
    CHECK(ucb_path_is_inside(root, child));
    CHECK_FALSE(ucb_path_is_inside(dir, relative));

    ucb_path_free(dir);
    ucb_path_free(child);
    ucb_path_free(sibling);
    ucb_path_free(root);
    ucb_path_free(relative);

    UCB_MEMTRACK_POP();
}

TEST_CASE("path - c string helpers")
{
    UCB_MEMTRACK_PUSH();

    ucb_config prev = ucb_conf_get();
    ucb_config cfg = ucb_conf_get();
    cfg.default_path_style = UCB_PATH_STYLE_POSIX;
    ucb_conf_set(&cfg);

    ucb_path path = ucb_path_make();
    ucb_path_init(&path, UCB_STR("a/b/c.txt"));

    const ucb_str* dir = ucb_path_dir(&path);
    check_cstr_value(dir, "a/b");

    const ucb_str* stem = ucb_path_stem(&path);
    check_cstr_value(stem, "c");

    const ucb_str* ext = ucb_path_ext(&path);
    check_cstr_value(ext, ".txt");

    const ucb_str* filename = ucb_path_filename(&path);
    check_cstr_value(filename, "c.txt");

    ucb_path_release(&path);

    ucb_path dir_path = ucb_path_make();
    REQUIRE(ucb_path_init_style(&dir_path, UCB_STR("/a/b/c.txt"), UCB_PATH_STYLE_POSIX));
    dir = ucb_path_dir(&dir_path);
    check_cstr_value(dir, "/a/b");
    ucb_path_release(&dir_path);

    ucb_path parent_path = ucb_path_make();
    REQUIRE(ucb_path_init_style(&parent_path, UCB_STR("a/b/c.txt"), UCB_PATH_STYLE_POSIX));
    ucb_path* parent = ucb_path_parent(&parent_path);
    REQUIRE(parent != nullptr);
    check_cstr(parent, "a/b");
    ucb_path_free(parent);
    ucb_path_release(&parent_path);

    ucb_path norm_path = ucb_path_make();
    REQUIRE(ucb_path_init_style(&norm_path, UCB_STR("a/./b/../c"), UCB_PATH_STYLE_POSIX));
    REQUIRE(ucb_path_normalize(&norm_path));
    ucb_str* norm = ucb_path_to_str(&norm_path);
    check_cstr_value(norm, "a/c");
    ucb_str_free(norm);
    ucb_path_release(&norm_path);

    ucb_path parts_path = ucb_path_make();
    REQUIRE(ucb_path_init_style(&parts_path, UCB_STR("a/b.txt"), UCB_PATH_STYLE_POSIX));
    ucb_vector_str* parts = ucb_path_parts(&parts_path);
    REQUIRE(parts != nullptr);
    REQUIRE(ucb_vector_str_size(parts) == 2);
    check_cstr_value(ucb_vector_str_get(parts, 0), "a");
    check_cstr_value(ucb_vector_str_get(parts, 1), "b.txt");
    ucb_vector_str_free_full(parts);
    ucb_path_release(&parts_path);

    ucb_path parents_path = ucb_path_make();
    REQUIRE(ucb_path_init_style(&parents_path, UCB_STR("a/b/c"), UCB_PATH_STYLE_POSIX));
    ucb_vector_str* parents = ucb_path_parents(&parents_path);
    REQUIRE(parents != nullptr);
    REQUIRE(ucb_vector_str_size(parents) == 2);
    check_cstr_value(ucb_vector_str_get(parents, 0), "a/b");
    check_cstr_value(ucb_vector_str_get(parents, 1), "a");
    ucb_vector_str_free_full(parents);
    ucb_path_release(&parents_path);

    ucb_conf_set(&prev);

    UCB_MEMTRACK_POP();
}

TEST_SUITE_END();
