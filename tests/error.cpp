/**
 * @file error.cpp
 *
 * This file is part of the UCB project
 * - SPDX-FileCopyrightText: © 2026 Åke Svedin <ake@svedin.org>
 * - SPDX-License-Identifier: MIT
 *
 * @brief Error handling tests
 *
 * NOTE: Allocation failure paths (out-of-memory while building or copying an
 * error) cannot be exercised because there is no allocator failure hook. Those
 * paths are covered by inspection only.
 */

#include "ucb/error.h"

#include "common.h"

#include "ucb/defines.h"
#include "ucb/errcodes.h"

#include <doctest.h>

#include <cerrno>
#include <cstring>
#include <string>
#include <thread>
#include <vector>

#ifdef _WIN32
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <Windows.h>
#endif

static void dummy_handler(ucb_errlvl lvl, const ucb_error* e)
{
    UCB_UNUSED(lvl);
    UCB_UNUSED(e);
}

static int s_reentry_calls = 0;
static void reentrant_handler(ucb_errlvl lvl, const ucb_error* e)
{
    UCB_UNUSED(lvl);
    UCB_UNUSED(e);
    s_reentry_calls++;
    if (s_reentry_calls < 10)
    {
        const ucb_error* nested = ucb_error_format(UCB_ERROR_INTERNAL, "nested error");
        ucb_error_report(UCB_ERRLVL_WARNING, nested);
    }
}

TEST_CASE("error - code and level strings")
{
    CHECK(std::string(ucb_error_codestr(UCB_OK)) == "SUCCESS");
    CHECK(std::string(ucb_error_codestr(UCB_ERROR_INVALID_ARG)) == "ERROR_INVALID_ARG");
    CHECK(std::string(ucb_error_codestr(UCB_ERRSYS_ENOENT)) == "ERRSYS_ENOENT");
    // Unknown values must not abort
    CHECK(std::string(ucb_error_codestr((ucb_ecode)123456)) == "UNKNOWN_ERROR");

    CHECK(std::string(ucb_error_lvlstr(UCB_ERRLVL_USER)) == "ERROR");
    CHECK(std::string(ucb_error_lvlstr(UCB_ERRLVL_FATAL)) == "FATAL ERROR");
    CHECK(std::string(ucb_error_lvlstr(UCB_ERRLVL_SYSTEM)) == "SYSTEM ERROR");
    CHECK(std::string(ucb_error_lvlstr(UCB_ERRLVL_WARNING)) == "WARNING");
    CHECK(std::string(ucb_error_lvlstr((ucb_errlvl)99)) == "UNKNOWN");
}

TEST_CASE("error - handler set and get")
{
    ucb_error_func prev = ucb_error_set_func(dummy_handler);
    CHECK(ucb_error_get_func() == dummy_handler);
    ucb_error_set_func(prev);
    CHECK(ucb_error_get_func() == prev);
}

TEST_CASE_FIXTURE(TestFailureFixture, "error - throw")
{
    SUBCASE("throw and clear")
    {
        ucb_error* err = nullptr;
        ucb_throw(&err, UCB_ERROR_INVALID_ARG, "bad thing");
        REQUIRE(UCB_IS_THROWN(err));
        CHECK(err->code == UCB_ERROR_INVALID_ARG);
        CHECK(std::string(err->msg) == "bad thing");
        CHECK(num_error == 0);

        ucb_error_clear(&err);
        CHECK(err == nullptr);
        CHECK(num_error == 0);
    }

    SUBCASE("throw formatted")
    {
        ucb_error* err = nullptr;
        ucb_throw_format(&err, UCB_ERROR_OUT_OF_BOUNDS, "index %d of %d", 7, 3);
        REQUIRE(UCB_IS_THROWN(err));
        CHECK(err->code == UCB_ERROR_OUT_OF_BOUNDS);
        CHECK(std::string(err->msg) == "index 7 of 3");
        ucb_error_clear(&err);
        CHECK(num_error == 0);
    }

    SUBCASE("throw without error pointer is a no-op")
    {
        ucb_throw(nullptr, UCB_ERROR_INVALID_ARG, "ignored");
        CHECK(num_error == 0);
    }

    SUBCASE("throw over an unhandled error warns and replaces")
    {
        ucb_error* err = nullptr;
        ucb_throw(&err, UCB_ERROR_INVALID_ARG, "first");
        ucb_throw(&err, UCB_ERROR_INTERNAL, "second");

        REQUIRE(num_error == 1);
        CHECK(errors[0].lvl == UCB_ERRLVL_WARNING);
        REQUIRE(UCB_IS_THROWN(err));
        CHECK(err->code == UCB_ERROR_INTERNAL);
        CHECK(std::string(err->msg) == "second");
        ucb_error_clear(&err);
    }
}

TEST_CASE_FIXTURE(TestFailureFixture, "error - copy and free")
{
    const ucb_error* src = ucb_error_format(UCB_ERROR_INVALID_ARG, "copy me");
    REQUIRE(src != nullptr);

    ucb_error* cpy = ucb_error_copy(src);
    REQUIRE(cpy != nullptr);
    CHECK(cpy->code == src->code);
    CHECK(std::string(cpy->msg) == "copy me");
    CHECK_FALSE(cpy->is_static);
    ucb_error_free(cpy);
    CHECK(num_error == 0);

    SUBCASE("copy of NULL is NULL")
    {
        CHECK(ucb_error_copy(nullptr) == nullptr);
    }

    SUBCASE("free NULL aborts")
    {
        CHECK_ABORTS(ucb_error_free(nullptr));
        REQUIRE(num_aborts == 1);
        REQUIRE(num_error == 1);
        CHECK(errors[0].code == UCB_ERROR_INVALID_ARG);
    }

    SUBCASE("free a static error aborts")
    {
        CHECK_ABORTS(ucb_error_free((ucb_error*)src));
        REQUIRE(num_aborts == 1);
        REQUIRE(num_error == 1);
        CHECK(errors[0].code == UCB_ERROR_INVALID_ARG);
    }
}

TEST_CASE_FIXTURE(TestFailureFixture, "error - format")
{
    SUBCASE("format truncates")
    {
        std::string big(600, 'x');
        const ucb_error* err = ucb_error_format(UCB_ERROR_INVALID_ARG, "%s", big.c_str());
        REQUIRE(err != nullptr);
        CHECK(std::strlen(err->msg) == 511);
        CHECK(num_error == 0);
    }

    SUBCASE("format rejects zero code")
    {
        CHECK_ABORTS(ucb_error_format(UCB_OK, "no code"));
        REQUIRE(num_aborts == 1);
        REQUIRE(num_error == 1);
        CHECK(errors[0].code == UCB_ERROR_INVALID_ARG);
    }

    SUBCASE("format rejects NULL format")
    {
        CHECK_ABORTS(ucb_error_format(UCB_ERROR_INVALID_ARG, nullptr));
        REQUIRE(num_aborts == 1);
        REQUIRE(num_error == 1);
        CHECK(errors[0].code == UCB_ERROR_INVALID_ARG);
    }
}

TEST_CASE_FIXTURE(TestFailureFixture, "error - print")
{
    SUBCASE("print NULL aborts")
    {
        CHECK_ABORTS(ucb_error_print(UCB_ERRLVL_WARNING, nullptr));
        REQUIRE(num_aborts == 1);
        REQUIRE(num_error == 1);
        CHECK(errors[0].code == UCB_ERROR_INVALID_ARG);
    }

    SUBCASE("print a valid error does not abort")
    {
        const ucb_error* err = ucb_error_format(UCB_ERROR_INVALID_ARG, "print me");
        ucb_error_print(UCB_ERRLVL_WARNING, err);
        CHECK(num_error == 0);
    }
}

TEST_CASE_FIXTURE(TestFailureFixture, "error - report")
{
    SUBCASE("report NULL aborts")
    {
        CHECK_ABORTS(ucb_error_report(UCB_ERRLVL_WARNING, nullptr));
        REQUIRE(num_aborts == 1);
        REQUIRE(num_error == 1);
        CHECK(errors[0].code == UCB_ERROR_INVALID_ARG);
    }

    SUBCASE("warning is reported without abort")
    {
        UCB_WARN("a warning %d", 3);
        REQUIRE(num_error == 1);
        CHECK(errors[0].lvl == UCB_ERRLVL_WARNING);
        CHECK(errors[0].code == UCB_ERROR_WARNING);
    }

    SUBCASE("report error macro is NULL safe")
    {
        ucb_error* null_err = nullptr;
        CHECK_ABORTS(UCB_REPORT_ERROR(null_err));
        REQUIRE(num_aborts == 1);
        REQUIRE(num_error == 1);
        CHECK(errors[0].code == UCB_ERROR_INVALID_ARG);
    }

    SUBCASE("report error macro tolerates a missing message")
    {
        ucb_error no_msg = {nullptr, UCB_ERROR_INTERNAL, true};
        CHECK_ABORTS(UCB_REPORT_ERROR(&no_msg));
        REQUIRE(num_aborts == 1);
        REQUIRE(num_error == 1);
        CHECK(errors[0].code == UCB_ERROR_INTERNAL);
    }
}

TEST_CASE_FIXTURE(TestFailureFixture, "error - errno wrappers")
{
    SUBCASE("zero status is a no-op")
    {
        CHECK_FALSE(ucb_report_errno(0, "no error", "test_fn"));
        CHECK_FALSE(ucb_throw_errno(nullptr, 0, "no error"));
        CHECK(num_error == 0);
    }

    SUBCASE("report a known errno")
    {
        CHECK(ucb_report_errno(ENOENT, "no such file", "test_fn"));
        REQUIRE(num_error == 1);
        CHECK(errors[0].lvl == UCB_ERRLVL_SYSTEM);
        CHECK(errors[0].code == UCB_ERRSYS_ENOENT);
    }

    SUBCASE("report an errno without a message")
    {
        CHECK(ucb_report_errno(EINVAL, nullptr, "test_fn"));
        REQUIRE(num_error == 1);
        CHECK(errors[0].lvl == UCB_ERRLVL_SYSTEM);
        CHECK(errors[0].code == UCB_ERRSYS_EINVAL);
    }

    SUBCASE("throw a known errno")
    {
        ucb_error* err = nullptr;
        CHECK(ucb_throw_errno(&err, ENOENT, nullptr));
        REQUIRE(UCB_IS_THROWN(err));
        CHECK(err->code == UCB_ERRSYS_ENOENT);
        CHECK(err->msg != nullptr);
        CHECK(err->msg[0] != '\0');
        ucb_error_clear(&err);
        CHECK(num_error == 0);
    }

    SUBCASE("throw an unknown errno")
    {
        ucb_error* err = nullptr;
        CHECK(ucb_throw_errno(&err, 123456, "mystery"));
        REQUIRE(UCB_IS_THROWN(err));
        CHECK(err->code == UCB_ERRSYS_UNKNOWN);
        ucb_error_clear(&err);
        CHECK(num_error == 0);
    }
}

TEST_CASE("error - reentrancy guard")
{
    ucb_error_func prev = ucb_error_set_func(reentrant_handler);
    s_reentry_calls = 0;

    const ucb_error* err = ucb_error_format(UCB_ERROR_INTERNAL, "outer error");
    ucb_error_report(UCB_ERRLVL_WARNING, err);

    ucb_error_set_func(prev);
    // The nested report must be suppressed, so the handler runs only once.
    CHECK(s_reentry_calls == 1);
}

TEST_CASE("error - reporting is thread safe")
{
    // Exercise the default handler path (report mutex + one-time init).
    ucb_error_func prev = ucb_error_set_func(nullptr);

    const int num_threads = 4;
    const int num_iterations = 25;
    std::vector<std::thread> threads;
    for (int i = 0; i < num_threads; i++)
    {
        threads.emplace_back([i, num_iterations]() {
            for (int j = 0; j < num_iterations; j++)
                ucb_report_warning("thread %d warning %d", i, j);
        });
    }
    for (auto& t : threads)
        t.join();

    ucb_error_set_func(prev);
}

#ifdef _WIN32
TEST_CASE("error - win32 mapping")
{
    CHECK(ucb_err_wrap_win32(ERROR_SUCCESS) == UCB_OK);
    CHECK(ucb_err_wrap_win32(ERROR_TIMEOUT) == UCB_ERRSYS_ETIMEDOUT);
    CHECK(ucb_err_wrap_win32(ERROR_FILE_NOT_FOUND) == UCB_ERRSYS_ENOENT);
    CHECK(ucb_err_wrap_win32(ERROR_ACCESS_DENIED) == UCB_ERRSYS_EACCES);
    CHECK(ucb_err_wrap_win32(0xDEADBEEF) == UCB_ERRSYS_WIN_GENERIC);
}
#endif
