/**
 * @file dir.cpp
 *
 * This file is part of the UCB project
 * - SPDX-FileCopyrightText: © 2026 Åke Svedin <ake@svedin.org>
 * - SPDX-License-Identifier: MIT
 *
 * @brief directory iterator tests
 */

#include "fs_tmp.h"

#include <ucb/dir.h>
#include <ucb/error.h>
#include <ucb/fs.h>
#include <ucb/memdbg.h>

#include <doctest.h>

#include <algorithm>
#include <string>
#include <vector>

static bool has_name(const std::vector<std::string>& names, const char* name)
{
    return std::find(names.begin(), names.end(), name) != names.end();
}

TEST_SUITE_BEGIN("dir");

TEST_CASE("dir - general")
{
    UCB_MEMTRACK_PUSH();

    ucb_error* err = UCB_NULL;
    FsTmp tmp("dir_general");
    REQUIRE(ucb_fs_mkdir_p(tmp.path("d/sub").c_str(), &err));
    REQUIRE(tmp.write("d/a.txt", "a"));
    REQUIRE(tmp.write("d/b.txt", "b"));

    ucb_dir* dir = ucb_dir_open(tmp.path("d").c_str(), 0, &err);
    REQUIRE(dir != UCB_NULL);

    std::vector<std::string> files;
    std::vector<std::string> dirs;
    ucb_dir_entry entry;
    while (ucb_dir_next(dir, &entry, &err))
    {
        std::string name = entry.name;
        if (entry.kind == UCB_FS_KIND_DIR)
            dirs.push_back(name);
        else
            files.push_back(name);
    }
    CHECK(err == UCB_NULL);

    std::sort(files.begin(), files.end());
    REQUIRE(files.size() == 2);
    CHECK(files[0] == "a.txt");
    CHECK(files[1] == "b.txt");
    REQUIRE(dirs.size() == 1);
    CHECK(dirs[0] == "sub");

    ucb_dir_free(dir);
    UCB_MEMTRACK_POP();
}

TEST_CASE("dir - dot")
{
    UCB_MEMTRACK_PUSH();

    ucb_error* err = UCB_NULL;
    FsTmp tmp("dir_dot");
    REQUIRE(ucb_fs_mkdir(tmp.path("d").c_str(), &err));
    REQUIRE(tmp.write("d/a.txt", "a"));

    ucb_dir* dir = ucb_dir_open(tmp.path("d").c_str(), UCB_DIR_INCLUDE_DOT, &err);
    REQUIRE(dir != UCB_NULL);

    std::vector<std::string> names;
    ucb_dir_entry entry;
    while (ucb_dir_next(dir, &entry, &err))
        names.push_back(entry.name);

    CHECK(err == UCB_NULL);
    CHECK(has_name(names, "."));
    CHECK(has_name(names, ".."));
    CHECK(has_name(names, "a.txt"));

    ucb_dir_free(dir);
    UCB_MEMTRACK_POP();
}

TEST_CASE("dir - list")
{
    UCB_MEMTRACK_PUSH();

    ucb_error* err = UCB_NULL;
    FsTmp tmp("dir_list");
    REQUIRE(ucb_fs_mkdir_p(tmp.path("d/sub").c_str(), &err));
    REQUIRE(tmp.write("d/a.txt", "a"));
    REQUIRE(tmp.write("d/b.txt", "b"));

    ucb_vector_str* names = UCB_NULL;
    REQUIRE(ucb_dir_list(tmp.path("d").c_str(), &names, &err));
    REQUIRE(names != UCB_NULL);
    CHECK(ucb_vector_str_size(names) == 3);

    std::vector<std::string> found;
    for (size_t i = 0; i < names->size; i++)
        found.push_back(std::string(ucb_str_cstr(names->data[i])));
    CHECK(has_name(found, "a.txt"));
    CHECK(has_name(found, "b.txt"));
    CHECK(has_name(found, "sub"));

    ucb_vector_str_free_full(names);

    UCB_MEMTRACK_POP();
}

TEST_CASE("dir - errors")
{
    UCB_MEMTRACK_PUSH();

    ucb_error* err = UCB_NULL;
    FsTmp tmp("dir_errors");

    ucb_dir* dir = ucb_dir_open(tmp.path("missing").c_str(), 0, &err);
    CHECK(dir == UCB_NULL);
    REQUIRE(err != UCB_NULL);
    CHECK(err->code == UCB_ERRSYS_ENOENT);
    ucb_error_clear(&err);

    REQUIRE(tmp.write("file.txt", "x"));
    dir = ucb_dir_open(tmp.path("file.txt").c_str(), 0, &err);
    CHECK(dir == UCB_NULL);
    REQUIRE(err != UCB_NULL);
    ucb_error_clear(&err);

    REQUIRE(ucb_fs_mkdir(tmp.path("d").c_str(), &err));
    dir = ucb_dir_open(tmp.path("d").c_str(), 0, &err);
    REQUIRE(dir != UCB_NULL);
    REQUIRE(ucb_dir_close(dir, &err));

    ucb_dir_entry entry;
    CHECK_FALSE(ucb_dir_next(dir, &entry, &err));
    REQUIRE(err != UCB_NULL);
    CHECK(err->code == UCB_ERROR_INVALID_STATE);
    ucb_error_clear(&err);
    ucb_dir_free(dir);

    UCB_MEMTRACK_POP();
}

TEST_CASE("dir - unicode")
{
    UCB_MEMTRACK_PUSH();

    ucb_error* err = UCB_NULL;
    FsTmp tmp("dir_unicode");
    const char* name = "ucb_\xC3\xA5\xC3\xA4\xC3\xB6_\xE6\xB5\x8B\xE8\xAF\x95.txt";
    REQUIRE(tmp.write(name, "u"));

    ucb_vector_str* names = UCB_NULL;
    REQUIRE(ucb_dir_list(tmp.dir().c_str(), &names, &err));

    bool found = false;
    for (size_t i = 0; i < names->size; i++)
    {
        if (std::string(ucb_str_cstr(names->data[i])) == name)
            found = true;
    }
    CHECK(found);
    ucb_vector_str_free_full(names);

    UCB_MEMTRACK_POP();
}

TEST_SUITE_END();
