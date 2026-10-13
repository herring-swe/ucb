/**
 * @file fs.cpp
 *
 * This file is part of the UCB project
 * - SPDX-FileCopyrightText: © 2026 Åke Svedin <ake@svedin.org>
 * - SPDX-License-Identifier: MIT
 *
 * @brief filesystem API tests
 */

#include "common.h"
#include "fs_tmp.h"

#include <ucb/error.h>
#include <ucb/file.h>
#include <ucb/fs.h>
#include <ucb/fs_ex.h>
#include <ucb/memdbg.h>
#include <ucb/string.h>
#include <ucb/string_ex.h>

#include <doctest.h>

#include <string>

static void clear_err(ucb_error** perr)
{
    ucb_error_clear(perr);
}

TEST_SUITE_BEGIN("fs");

TEST_CASE("fs - predicates")
{
    UCB_MEMTRACK_PUSH();

    ucb_error* err = UCB_NULL;
    FsTmp tmp("fs_pred");
    REQUIRE(tmp.write("a.txt", "hello"));
    REQUIRE(ucb_fs_mkdir_p(UCB_STR(tmp.path("sub")), &err));

    CHECK(ucb_fs_exists(UCB_STR(tmp.path("a.txt"))));
    CHECK(ucb_fs_is_file(UCB_STR(tmp.path("a.txt"))));
    CHECK_FALSE(ucb_fs_is_dir(UCB_STR(tmp.path("a.txt"))));
    CHECK(ucb_fs_is_dir(UCB_STR(tmp.path("sub"))));
    CHECK_FALSE(ucb_fs_is_file(UCB_STR(tmp.path("sub"))));

    CHECK_FALSE(ucb_fs_exists(UCB_STR(tmp.path("missing"))));
    CHECK_FALSE(ucb_fs_exists(UCB_STR("")));

    // A trailing separator is insignificant.
    std::string sub_slash = tmp.path("sub") + "/";
    CHECK(ucb_fs_is_dir(UCB_STR(sub_slash)));

    if (fs_create_symlink(tmp.path("a.txt").c_str(), tmp.path("link").c_str(), false))
    {
        // A link to a file: exists and is_file follow it, is_symlink sees it.
        CHECK(ucb_fs_is_symlink(UCB_STR(tmp.path("link"))));
        CHECK(ucb_fs_exists(UCB_STR(tmp.path("link"))));
        CHECK(ucb_fs_is_file(UCB_STR(tmp.path("link"))));
        CHECK_FALSE(ucb_fs_is_dir(UCB_STR(tmp.path("link"))));
    }
    else
    {
        MESSAGE("symlinks unavailable; skipping symlink predicate coverage");
    }

    // A broken link: exists is false, is_symlink is true.
    std::string broken_target = tmp.path("no_such_target");
    if (fs_create_symlink(broken_target.c_str(), tmp.path("broken").c_str(), false))
    {
        CHECK(ucb_fs_is_symlink(UCB_STR(tmp.path("broken"))));
        CHECK_FALSE(ucb_fs_exists(UCB_STR(tmp.path("broken"))));
        CHECK_FALSE(ucb_fs_is_file(UCB_STR(tmp.path("broken"))));
    }

    UCB_MEMTRACK_POP();
}

TEST_CASE_FIXTURE(TestFailureFixture, "fs - predicate arg validation")
{
    CHECK_ABORTS(ucb_fs_exists(UCB_NULL));
    CHECK_ABORTS(ucb_fs_is_file(UCB_NULL));
    CHECK_ABORTS(ucb_fs_is_dir(UCB_NULL));
    CHECK_ABORTS(ucb_fs_is_symlink(UCB_NULL));
}

TEST_CASE("fs - stat")
{
    UCB_MEMTRACK_PUSH();

    ucb_error* err = UCB_NULL;
    FsTmp tmp("fs_stat");
    REQUIRE(tmp.write("data.bin", std::string(1234, 'z')));

    ucb_fs_stat st;
    REQUIRE(ucb_fs_get_stat(UCB_STR(tmp.path("data.bin")), 0, &st, &err));
    CHECK(st.kind == UCB_FS_KIND_FILE);
    CHECK(st.size == 1234u);
    CHECK(st.mtime > 0);
    CHECK_FALSE(st.is_readonly);

    ucb_fs_kind kind = UCB_FS_KIND_UNKNOWN;
    REQUIRE(ucb_fs_get_kind(UCB_STR(tmp.path("data.bin")), 0, &kind, &err));
    CHECK(kind == UCB_FS_KIND_FILE);

    ucb_fs_stat_ns ns = {};
    REQUIRE(ucb_fs_get_stat_ns(UCB_STR(tmp.path("data.bin")), 0, &ns, &err));
    CHECK(ns.mtime_ns / 1000000000 == st.mtime);

    ucb_fs_stat missing;
    CHECK_FALSE(ucb_fs_get_stat(UCB_STR(tmp.path("nope")), 0, &missing, &err));
    REQUIRE(err != UCB_NULL);
    CHECK(err->code == UCB_ERRSYS_ENOENT);
    clear_err(&err);

    UCB_MEMTRACK_POP();
}

TEST_CASE("fs - mkdir")
{
    UCB_MEMTRACK_PUSH();

    ucb_error* err = UCB_NULL;
    FsTmp tmp("fs_mkdir");

    REQUIRE(ucb_fs_mkdir(UCB_STR(tmp.path("one")), &err));
    CHECK(ucb_fs_is_dir(UCB_STR(tmp.path("one"))));

    CHECK_FALSE(ucb_fs_mkdir(UCB_STR(tmp.path("one")), &err));
    REQUIRE(err != UCB_NULL);
    CHECK(err->code == UCB_ERRSYS_EEXIST);
    clear_err(&err);

    REQUIRE(ucb_fs_mkdir_p(UCB_STR(tmp.path("a/b/c")), &err));
    CHECK(ucb_fs_is_dir(UCB_STR(tmp.path("a/b/c"))));

    // Existing directory is idempotent.
    REQUIRE(ucb_fs_mkdir_p(UCB_STR(tmp.path("a/b/c")), &err));

    REQUIRE(tmp.write("file", "x"));
    CHECK_FALSE(ucb_fs_mkdir_p(UCB_STR(tmp.path("file/sub")), &err));
    REQUIRE(err != UCB_NULL);
    CHECK(err->code == UCB_ERRSYS_ENOTDIR);
    clear_err(&err);

    UCB_MEMTRACK_POP();
}

TEST_CASE("fs - remove")
{
    UCB_MEMTRACK_PUSH();

    ucb_error* err = UCB_NULL;
    FsTmp tmp("fs_remove");

    REQUIRE(tmp.write("f", "x"));
    REQUIRE(ucb_fs_remove(UCB_STR(tmp.path("f")), &err));
    CHECK_FALSE(ucb_fs_exists(UCB_STR(tmp.path("f"))));

    CHECK_FALSE(ucb_fs_remove(UCB_STR(tmp.path("f")), &err));
    REQUIRE(err != UCB_NULL);
    CHECK(err->code == UCB_ERRSYS_ENOENT);
    clear_err(&err);

    REQUIRE(ucb_fs_mkdir(UCB_STR(tmp.path("d")), &err));
    REQUIRE(ucb_fs_remove(UCB_STR(tmp.path("d")), &err));
    CHECK_FALSE(ucb_fs_exists(UCB_STR(tmp.path("d"))));

    REQUIRE(ucb_fs_mkdir(UCB_STR(tmp.path("nd")), &err));
    REQUIRE(tmp.write("nd/child", "x"));
    CHECK_FALSE(ucb_fs_remove(UCB_STR(tmp.path("nd")), &err));
    REQUIRE(err != UCB_NULL);
    CHECK(err->code == UCB_ERRSYS_ENOTEMPTY);
    clear_err(&err);

    UCB_MEMTRACK_POP();
}

TEST_CASE("fs - remove_all")
{
    UCB_MEMTRACK_PUSH();

    ucb_error* err = UCB_NULL;
    FsTmp tmp("fs_removeall");

    REQUIRE(ucb_fs_mkdir_p(UCB_STR(tmp.path("t/a/b")), &err));
    REQUIRE(tmp.write("t/a/b/x.txt", "x"));
    REQUIRE(tmp.write("t/top.txt", "y"));

    REQUIRE(ucb_fs_remove_all(UCB_STR(tmp.path("t")), &err));
    CHECK_FALSE(ucb_fs_exists(UCB_STR(tmp.path("t"))));

    // Missing path is a successful no-op.
    REQUIRE(ucb_fs_remove_all(UCB_STR(tmp.path("t")), &err));

    // A filesystem root is refused.
    CHECK_FALSE(ucb_fs_remove_all(UCB_STR("/"), &err));
    REQUIRE(err != UCB_NULL);
    CHECK(err->code == UCB_ERROR_INVALID_ARG);
    clear_err(&err);

    // A '..' component is resolved, so a path that reaches a root is refused.
    CHECK_FALSE(ucb_fs_remove_all(UCB_STR("/a/b/../.."), &err));
    REQUIRE(err != UCB_NULL);
    CHECK(err->code == UCB_ERROR_INVALID_ARG);
    clear_err(&err);

    UCB_MEMTRACK_POP();
}

TEST_CASE("fs - copy file")
{
    UCB_MEMTRACK_PUSH();

    ucb_error* err = UCB_NULL;
    FsTmp tmp("fs_copyfile");
    REQUIRE(tmp.write("src.txt", "content-123"));

    REQUIRE(ucb_fs_copy_file(UCB_STR(tmp.path("src.txt")), UCB_STR(tmp.path("dst.txt")), 0, &err));
    std::string content;
    REQUIRE(tmp.read("dst.txt", content));
    CHECK(content == "content-123");

    CHECK_FALSE(
        ucb_fs_copy_file(UCB_STR(tmp.path("src.txt")), UCB_STR(tmp.path("dst.txt")), 0, &err));
    REQUIRE(err != UCB_NULL);
    CHECK(err->code == UCB_ERRSYS_EEXIST);
    clear_err(&err);

    REQUIRE(tmp.write("src.txt", "replaced"));
    REQUIRE(ucb_fs_copy_file(UCB_STR(tmp.path("src.txt")),
                             UCB_STR(tmp.path("dst.txt")),
                             UCB_FS_OVERWRITE,
                             &err));
    REQUIRE(tmp.read("dst.txt", content));
    CHECK(content == "replaced");

    REQUIRE(ucb_fs_mkdir(UCB_STR(tmp.path("dir")), &err));
    CHECK_FALSE(ucb_fs_copy_file(UCB_STR(tmp.path("src.txt")), UCB_STR(tmp.path("dir")), 0, &err));
    REQUIRE(err != UCB_NULL);
    CHECK(err->code == UCB_ERRSYS_EISDIR);
    clear_err(&err);

    CHECK_FALSE(ucb_fs_copy_file(UCB_STR(tmp.path("dir")), UCB_STR(tmp.path("other")), 0, &err));
    REQUIRE(err != UCB_NULL);
    CHECK(err->code == UCB_ERRSYS_EISDIR);
    clear_err(&err);

    CHECK_FALSE(
        ucb_fs_copy_file(UCB_STR(tmp.path("missing")), UCB_STR(tmp.path("other")), 0, &err));
    REQUIRE(err != UCB_NULL);
    CHECK(err->code == UCB_ERRSYS_ENOENT);
    clear_err(&err);

    // Overwriting a file onto itself must not destroy it.
    REQUIRE(tmp.write("self.txt", "self-content"));
    REQUIRE(ucb_fs_copy_file(UCB_STR(tmp.path("self.txt")),
                             UCB_STR(tmp.path("self.txt")),
                             UCB_FS_OVERWRITE,
                             &err));
    REQUIRE(tmp.read("self.txt", content));
    CHECK(content == "self-content");

    UCB_MEMTRACK_POP();
}

TEST_CASE("fs - copy tree")
{
    UCB_MEMTRACK_PUSH();

    ucb_error* err = UCB_NULL;
    FsTmp tmp("fs_copytree");
    REQUIRE(ucb_fs_mkdir_p(UCB_STR(tmp.path("tree/sub")), &err));
    REQUIRE(tmp.write("tree/a.txt", "A"));
    REQUIRE(tmp.write("tree/sub/b.txt", "B"));

    REQUIRE(ucb_fs_copy_tree(UCB_STR(tmp.path("tree")), UCB_STR(tmp.path("copy")), 0, &err));
    CHECK(ucb_fs_is_file(UCB_STR(tmp.path("copy/a.txt"))));
    CHECK(ucb_fs_is_file(UCB_STR(tmp.path("copy/sub/b.txt"))));
    std::string content;
    REQUIRE(tmp.read("copy/sub/b.txt", content));
    CHECK(content == "B");

    REQUIRE(tmp.write("plain.txt", "p"));
    CHECK_FALSE(
        ucb_fs_copy_tree(UCB_STR(tmp.path("tree")), UCB_STR(tmp.path("plain.txt")), 0, &err));
    REQUIRE(err != UCB_NULL);
    CHECK(err->code == UCB_ERRSYS_ENOTDIR);
    clear_err(&err);

    REQUIRE(ucb_fs_mkdir(UCB_STR(tmp.path("dest")), &err));
    CHECK_FALSE(ucb_fs_copy_tree(UCB_STR(tmp.path("tree")), UCB_STR(tmp.path("dest")), 0, &err));
    REQUIRE(err != UCB_NULL);
    CHECK(err->code == UCB_ERRSYS_EEXIST);
    clear_err(&err);

    REQUIRE(ucb_fs_copy_tree(UCB_STR(tmp.path("tree")),
                             UCB_STR(tmp.path("dest")),
                             UCB_FS_OVERWRITE,
                             &err));
    CHECK(ucb_fs_is_file(UCB_STR(tmp.path("dest/a.txt"))));

    // Missing destination parents are created.
    REQUIRE(ucb_fs_copy_tree(UCB_STR(tmp.path("tree")),
                             UCB_STR(tmp.path("deep/nested/copy")),
                             0,
                             &err));
    CHECK(ucb_fs_is_file(UCB_STR(tmp.path("deep/nested/copy/sub/b.txt"))));

    // A destination inside the source is refused.
    CHECK_FALSE(
        ucb_fs_copy_tree(UCB_STR(tmp.path("tree")), UCB_STR(tmp.path("tree/inner")), 0, &err));
    REQUIRE(err != UCB_NULL);
    CHECK(err->code == UCB_ERROR_INVALID_ARG);
    clear_err(&err);

    // Copying a directory onto itself is refused.
    CHECK_FALSE(ucb_fs_copy_tree(UCB_STR(tmp.path("tree")), UCB_STR(tmp.path("tree")), 0, &err));
    REQUIRE(err != UCB_NULL);
    CHECK(err->code == UCB_ERROR_INVALID_ARG);
    clear_err(&err);

    // A '..' destination that resolves back to the source is refused.
    CHECK_FALSE(
        ucb_fs_copy_tree(UCB_STR(tmp.path("tree")), UCB_STR(tmp.path("tree/sub/..")), 0, &err));
    REQUIRE(err != UCB_NULL);
    CHECK(err->code == UCB_ERROR_INVALID_ARG);
    clear_err(&err);

    UCB_MEMTRACK_POP();
}

TEST_CASE("fs - move")
{
    UCB_MEMTRACK_PUSH();

    ucb_error* err = UCB_NULL;
    FsTmp tmp("fs_move");
    REQUIRE(tmp.write("m.txt", "M"));

    REQUIRE(ucb_fs_move(UCB_STR(tmp.path("m.txt")), UCB_STR(tmp.path("m2.txt")), 0, &err));
    CHECK_FALSE(ucb_fs_exists(UCB_STR(tmp.path("m.txt"))));
    CHECK(ucb_fs_is_file(UCB_STR(tmp.path("m2.txt"))));

    CHECK_FALSE(ucb_fs_move(UCB_STR(tmp.path("m.txt")), UCB_STR(tmp.path("z.txt")), 0, &err));
    REQUIRE(err != UCB_NULL);
    CHECK(err->code == UCB_ERRSYS_ENOENT);
    clear_err(&err);

    REQUIRE(tmp.write("a.txt", "a"));
    REQUIRE(tmp.write("b.txt", "b"));
    CHECK_FALSE(ucb_fs_move(UCB_STR(tmp.path("a.txt")), UCB_STR(tmp.path("b.txt")), 0, &err));
    REQUIRE(err != UCB_NULL);
    CHECK(err->code == UCB_ERRSYS_EEXIST);
    clear_err(&err);

    REQUIRE(ucb_fs_move(UCB_STR(tmp.path("a.txt")),
                        UCB_STR(tmp.path("b.txt")),
                        UCB_FS_OVERWRITE,
                        &err));
    CHECK_FALSE(ucb_fs_exists(UCB_STR(tmp.path("a.txt"))));
    CHECK(ucb_fs_is_file(UCB_STR(tmp.path("b.txt"))));

    REQUIRE(ucb_fs_mkdir_p(UCB_STR(tmp.path("d/sub")), &err));
    REQUIRE(ucb_fs_move(UCB_STR(tmp.path("d")), UCB_STR(tmp.path("d2")), 0, &err));
    CHECK(ucb_fs_is_dir(UCB_STR(tmp.path("d2/sub"))));

    REQUIRE(tmp.write("f.txt", "f"));
    CHECK_FALSE(ucb_fs_move(UCB_STR(tmp.path("f.txt")), UCB_STR(tmp.path("d2")), 0, &err));
    REQUIRE(err != UCB_NULL);
    CHECK(err->code == UCB_ERRSYS_EEXIST);
    clear_err(&err);

    REQUIRE(tmp.write("target.txt", "t"));
    REQUIRE(ucb_fs_mkdir(UCB_STR(tmp.path("src_dir")), &err));
    CHECK_FALSE(
        ucb_fs_move(UCB_STR(tmp.path("src_dir")), UCB_STR(tmp.path("target.txt")), 0, &err));
    REQUIRE(err != UCB_NULL);
    CHECK(err->code == UCB_ERRSYS_ENOTDIR);
    clear_err(&err);

    UCB_MEMTRACK_POP();
}

TEST_CASE("fs - cwd")
{
    UCB_MEMTRACK_PUSH();

    ucb_error* err = UCB_NULL;
    ucb_str* before = ucb_fs_cwd();
    REQUIRE(before != UCB_NULL);
    CHECK(before->size > 0);

    FsTmp tmp("fs_cwd");
    REQUIRE(ucb_fs_chdir(UCB_STR(tmp.dir()), &err));

    ucb_str* here = ucb_fs_cwd();
    REQUIRE(here != UCB_NULL);
    CHECK(here->size > 0);

    // A relative write must land inside the new working directory.
    {
        ucb_file* file = ucb_file_open(UCB_STR("relative_fs_cwd.txt"),
                                       UCB_FILE_CREATE | UCB_FILE_TRUNCATE | UCB_FILE_WRITE,
                                       &err);
        REQUIRE(file != UCB_NULL);
        ucb_file_free(file);
    }
    CHECK(ucb_fs_is_file(UCB_STR("relative_fs_cwd.txt")));

    REQUIRE(ucb_fs_chdir(before, &err));

    ucb_str_free(before);
    ucb_str_free(here);

    UCB_MEMTRACK_POP();
}

TEST_CASE("fs - path semantics")
{
    UCB_MEMTRACK_PUSH();

    ucb_error* err = UCB_NULL;
    FsTmp tmp("fs_pathsem");
    REQUIRE(ucb_fs_mkdir_p(UCB_STR(tmp.path("p/q")), &err));

    std::string with_slash = tmp.path("p/q") + "/";
    std::string duplicate = tmp.path("p") + "//q";
    CHECK(ucb_fs_is_dir(UCB_STR(with_slash)));
    CHECK(ucb_fs_is_dir(UCB_STR(duplicate)));
    CHECK(ucb_fs_is_dir(UCB_STR(tmp.path("p/./q"))));

    REQUIRE(tmp.write("seed.txt", "s"));
    REQUIRE(ucb_fs_copy_file(UCB_STR(tmp.path("seed.txt")),
                             UCB_STR(tmp.path("p/q/copied.txt")),
                             0,
                             &err));
    CHECK(ucb_fs_is_file(UCB_STR(tmp.path("p/q/copied.txt"))));

    CHECK_FALSE(ucb_fs_mkdir(UCB_STR(""), &err));
    REQUIRE(err != UCB_NULL);
    CHECK(err->code == UCB_ERROR_INVALID_ARG);
    clear_err(&err);

    ucb_fs_kind kind = UCB_FS_KIND_UNKNOWN;
    CHECK_FALSE(ucb_fs_get_kind(UCB_STR(""), 0, &kind, &err));
    REQUIRE(err != UCB_NULL);
    CHECK(err->code == UCB_ERROR_INVALID_ARG);
    clear_err(&err);

    UCB_MEMTRACK_POP();
}

TEST_CASE("fs - symlink copy")
{
    UCB_MEMTRACK_PUSH();

    ucb_error* err = UCB_NULL;
    FsTmp tmp("fs_symcopy");
    REQUIRE(tmp.write("target.txt", "T"));

    if (!fs_create_symlink(tmp.path("target.txt").c_str(), tmp.path("link.txt").c_str(), false))
    {
        MESSAGE("symlinks unavailable; skipping symlink copy coverage");
        UCB_MEMTRACK_POP();
        return;
    }

    // Default: recreate the link.
    REQUIRE(ucb_fs_copy_file(UCB_STR(tmp.path("link.txt")),
                             UCB_STR(tmp.path("copy_link.txt")),
                             0,
                             &err));
    CHECK(ucb_fs_is_symlink(UCB_STR(tmp.path("copy_link.txt"))));

    // FOLLOW: copy the target contents.
    REQUIRE(ucb_fs_copy_file(UCB_STR(tmp.path("link.txt")),
                             UCB_STR(tmp.path("copy_follow.txt")),
                             UCB_FS_FOLLOW,
                             &err));
    CHECK_FALSE(ucb_fs_is_symlink(UCB_STR(tmp.path("copy_follow.txt"))));
    CHECK(ucb_fs_is_file(UCB_STR(tmp.path("copy_follow.txt"))));
    std::string content;
    REQUIRE(tmp.read("copy_follow.txt", content));
    CHECK(content == "T");

    // A tree containing a link preserves the link by default.
    REQUIRE(ucb_fs_mkdir_p(UCB_STR(tmp.path("srctree")), &err));
    REQUIRE(tmp.write("srctree/data.txt", "D"));
    REQUIRE(fs_create_symlink(tmp.path("srctree/data.txt").c_str(),
                              tmp.path("srctree/link.txt").c_str(),
                              false));
    REQUIRE(ucb_fs_copy_tree(UCB_STR(tmp.path("srctree")), UCB_STR(tmp.path("tree")), 0, &err));
    CHECK(ucb_fs_is_symlink(UCB_STR(tmp.path("tree/link.txt"))));

    UCB_MEMTRACK_POP();
}

TEST_CASE("fs - unicode")
{
    UCB_MEMTRACK_PUSH();

    ucb_error* err = UCB_NULL;
    FsTmp tmp("fs_unicode");
    const char* name = "ucb_\xC3\xA5\xC3\xA4\xC3\xB6_\xE6\xB5\x8B\xE8\xAF\x95.txt";

    REQUIRE(tmp.write(name, "u"));
    CHECK(ucb_fs_is_file(UCB_STR(tmp.path(name))));

    ucb_fs_stat st;
    REQUIRE(ucb_fs_get_stat(UCB_STR(tmp.path(name)), 0, &st, &err));
    CHECK(st.kind == UCB_FS_KIND_FILE);

    REQUIRE(ucb_fs_copy_file(UCB_STR(tmp.path(name)), UCB_STR(tmp.path("copy.txt")), 0, &err));
    CHECK(ucb_fs_is_file(UCB_STR(tmp.path("copy.txt"))));

    UCB_MEMTRACK_POP();
}

TEST_SUITE_END();
