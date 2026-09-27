/**
 * @file process.cpp
 *
 * This file is part of the UCB project
 * - SPDX-FileCopyrightText: © 2026 Åke Svedin <ake@svedin.org>
 * - SPDX-License-Identifier: MIT
 *
 * @brief process launching, waiting and capture tests
 */

#include "common.h"
#include "fs_tmp.h"

#include <ucb/buffer.h>
#include <ucb/env.h>
#include <ucb/envmap.h>
#include <ucb/error.h>
#include <ucb/file.h>
#include <ucb/memdbg.h>
#include <ucb/pipe.h>
#include <ucb/process.h>

#include <doctest.h>

#include <algorithm>
#include <cctype>
#include <string>
#include <vector>

#ifdef _WIN32
#include <cstdlib>
#else
#include <signal.h>
#endif

/* -------------------------------------------------------------------------- */
/*                                   Helpers                                  */
/* -------------------------------------------------------------------------- */

static const char* null_device(void)
{
#ifdef _WIN32
    return "NUL";
#else
    return "/dev/null";
#endif
}

static std::vector<std::string> make_shell_args(const std::string& script)
{
#ifdef _WIN32
    return {"cmd.exe", "/C", script};
#else
    return {"/bin/sh", "-c", script};
#endif
}

static std::vector<const char*> to_argv(const std::vector<std::string>& args)
{
    std::vector<const char*> argv;
    argv.reserve(args.size() + 1);
    for (const std::string& arg : args)
        argv.push_back(arg.c_str());
    argv.push_back(nullptr);
    return argv;
}

static std::string strip_newlines(std::string value)
{
    while (!value.empty() && (value.back() == '\n' || value.back() == '\r'))
        value.pop_back();
    return value;
}

static std::string normalize_path(std::string value)
{
    std::replace(value.begin(), value.end(), '\\', '/');
#ifdef _WIN32
    std::transform(value.begin(), value.end(), value.begin(), [](unsigned char c) {
        return static_cast<char>(std::tolower(c));
    });
#endif
    return value;
}

static std::string buffer_to_string(const ucb_buffer* buf)
{
    return std::string(buf->data, buf->size);
}

static bool read_all_string(ucb_file* file, std::string& out)
{
    ucb_buffer* buf = ucb_buffer_new_heap(64);
    bool ok = ucb_file_read_buffer(file, buf, nullptr);
    if (ok)
        out.assign(buf->data, buf->size);
    ucb_buffer_free(buf);
    return ok;
}

static std::vector<std::string> long_running_args(void)
{
#ifdef _WIN32
    return {"ping", "-n", "6", "127.0.0.1"};
#else
    return {"sleep", "5"};
#endif
}

#ifdef _WIN32
static void copy_parent_var(ucb_envmap* env, const char* name)
{
    const char* value = ucb_env_get(name);
    if (value)
        ucb_envmap_set(env, name, value);
}
#endif

/* -------------------------------------------------------------------------- */
/*                                   Tests                                    */
/* -------------------------------------------------------------------------- */

TEST_SUITE_BEGIN("process");

TEST_CASE("process - spawn wait exit code")
{
    UCB_MEMTRACK_PUSH();

    std::vector<std::string> args = make_shell_args("exit 7");
    std::vector<const char*> argv = to_argv(args);
    ucb_error* err = UCB_NULL;

    ucb_process_opts opts = ucb_process_opts_make();
    opts.argv = argv.data();

    ucb_process* proc = ucb_process_spawn(&opts, &err);
    REQUIRE(proc != UCB_NULL);
    CHECK(err == UCB_NULL);
    CHECK(ucb_process_get_pid(proc) != UCB_PID_INVALID);

    int code = -1;
    CHECK(ucb_process_wait(proc, -1, &code, &err));
    CHECK(err == UCB_NULL);
    CHECK(code == 7);

    // Waiting again returns the cached exit code.
    int cached = -1;
    CHECK(ucb_process_wait(proc, 0, &cached, &err));
    CHECK(cached == 7);

    ucb_process_free(proc);

    UCB_MEMTRACK_POP();
}

TEST_CASE("process - capture stdout")
{
    UCB_MEMTRACK_PUSH();

    std::vector<std::string> args = make_shell_args("echo hello");
    std::vector<const char*> argv = to_argv(args);
    ucb_error* err = UCB_NULL;

    ucb_buffer* out = ucb_buffer_new_heap(64);
    ucb_buffer* errbuf = ucb_buffer_new_heap(64);

    ucb_process_capture_opts opts = ucb_process_capture_opts_make();
    opts.argv = argv.data();

    int code = -1;
    REQUIRE(ucb_process_capture(&opts, out, errbuf, &code, &err));
    CHECK(err == UCB_NULL);
    CHECK(strip_newlines(buffer_to_string(out)) == "hello");
    CHECK(buffer_to_string(errbuf).empty());
    CHECK(code == 0);

    ucb_buffer_free(out);
    ucb_buffer_free(errbuf);

    UCB_MEMTRACK_POP();
}

TEST_CASE("process - capture stderr")
{
    UCB_MEMTRACK_PUSH();

    std::vector<std::string> args = make_shell_args("echo oops>&2");
    std::vector<const char*> argv = to_argv(args);
    ucb_error* err = UCB_NULL;

    ucb_buffer* out = ucb_buffer_new_heap(64);
    ucb_buffer* errbuf = ucb_buffer_new_heap(64);

    ucb_process_capture_opts opts = ucb_process_capture_opts_make();
    opts.argv = argv.data();

    int code = -1;
    REQUIRE(ucb_process_capture(&opts, out, errbuf, &code, &err));
    CHECK(buffer_to_string(out).empty());
    CHECK(strip_newlines(buffer_to_string(errbuf)) == "oops");
    CHECK(code == 0);

    ucb_buffer_free(out);
    ucb_buffer_free(errbuf);

    UCB_MEMTRACK_POP();
}

TEST_CASE("process - capture exit code")
{
    UCB_MEMTRACK_PUSH();

    std::vector<std::string> args = make_shell_args("exit 3");
    std::vector<const char*> argv = to_argv(args);
    ucb_error* err = UCB_NULL;

    ucb_buffer* out = ucb_buffer_new_heap(16);

    ucb_process_capture_opts opts = ucb_process_capture_opts_make();
    opts.argv = argv.data();

    int code = -1;
    REQUIRE(ucb_process_capture(&opts, out, UCB_NULL, &code, &err));
    CHECK(code == 3);

    ucb_buffer_free(out);

    UCB_MEMTRACK_POP();
}

TEST_CASE("process - environment")
{
    UCB_MEMTRACK_PUSH();

    ucb_envmap* env = ucb_envmap_new();
    REQUIRE(env != UCB_NULL);
    REQUIRE(ucb_envmap_set(env, "UCB_PROC_TEST", "custom-value"));

#ifdef _WIN32
    // cmd.exe needs a minimal environment to initialize.
    copy_parent_var(env, "SystemRoot");
    copy_parent_var(env, "windir");
    copy_parent_var(env, "PATH");
    copy_parent_var(env, "TEMP");
    copy_parent_var(env, "ComSpec");
#endif

    // A variable that exists in the parent but not in the replaced environment.
    REQUIRE(ucb_env_set("UCB_PROC_PARENT_ONLY", "leaked-from-parent", true));

#ifdef _WIN32
    std::vector<std::string> args = make_shell_args("echo %UCB_PROC_TEST%/%UCB_PROC_PARENT_ONLY%");
#else
    std::vector<std::string> args =
        make_shell_args("echo \"$UCB_PROC_TEST/$UCB_PROC_PARENT_ONLY\"");
#endif
    std::vector<const char*> argv = to_argv(args);
    ucb_error* err = UCB_NULL;

    ucb_buffer* out = ucb_buffer_new_heap(64);

    ucb_process_capture_opts opts = ucb_process_capture_opts_make();
    opts.argv = argv.data();
    opts.env = env;

    int code = -1;
    REQUIRE(ucb_process_capture(&opts, out, UCB_NULL, &code, &err));
    std::string text = buffer_to_string(out);
    CHECK(text.find("custom-value") != std::string::npos);
    CHECK(text.find("leaked") == std::string::npos);

    ucb_buffer_free(out);
    ucb_envmap_free(env);
    ucb_env_unset("UCB_PROC_PARENT_ONLY");

    UCB_MEMTRACK_POP();
}

TEST_CASE("process - working directory")
{
    UCB_MEMTRACK_PUSH();

    FsTmp tmp("ucb_proc_cwd");
#ifdef _WIN32
    std::vector<std::string> args = make_shell_args("cd");
#else
    std::vector<std::string> args = make_shell_args("pwd");
#endif
    std::vector<const char*> argv = to_argv(args);
    ucb_error* err = UCB_NULL;

    ucb_buffer* out = ucb_buffer_new_heap(64);

    ucb_process_capture_opts opts = ucb_process_capture_opts_make();
    opts.argv = argv.data();
    opts.cwd = tmp.dir().c_str();

    int code = -1;
    REQUIRE(ucb_process_capture(&opts, out, UCB_NULL, &code, &err));
    CHECK(normalize_path(strip_newlines(buffer_to_string(out))) == normalize_path(tmp.dir()));

    ucb_buffer_free(out);

    UCB_MEMTRACK_POP();
}

TEST_CASE("process - pipe redirection")
{
    UCB_MEMTRACK_PUSH();

    ucb_error* err = UCB_NULL;
    ucb_file* read_end = UCB_NULL;
    ucb_file* write_end = UCB_NULL;
    REQUIRE(ucb_pipe_create(&read_end, &write_end, UCB_FILE_CLOEXEC, &err));

#ifdef _WIN32
    REQUIRE(ucb_file_set_inherit(write_end, true, &err));
#endif

    std::vector<std::string> args = make_shell_args("echo redirected");
    std::vector<const char*> argv = to_argv(args);

    ucb_process_opts opts = ucb_process_opts_make();
    opts.argv = argv.data();
    opts.out = write_end;

    ucb_process* proc = ucb_process_spawn(&opts, &err);
    REQUIRE(proc != UCB_NULL);

    // Close the parent write end so the child becomes the only writer.
    ucb_file_free(write_end);
    write_end = UCB_NULL;

    std::string content;
    REQUIRE(read_all_string(read_end, content));
    CHECK(strip_newlines(content) == "redirected");

    int code = -1;
    CHECK(ucb_process_wait(proc, -1, &code, &err));
    CHECK(code == 0);

    ucb_process_free(proc);
    ucb_file_free(read_end);

    UCB_MEMTRACK_POP();
}

TEST_CASE("process - wait timeout")
{
    UCB_MEMTRACK_PUSH();

    std::vector<std::string> args = long_running_args();
    std::vector<const char*> argv = to_argv(args);
    ucb_error* err = UCB_NULL;

    // Discard the child's output so it cannot keep a test harness pipe open.
    ucb_file* devnull = ucb_file_open(null_device(), UCB_FILE_WRITE, &err);
    REQUIRE(devnull != UCB_NULL);

    ucb_process_opts opts = ucb_process_opts_make();
    opts.argv = argv.data();
    opts.out = devnull;

    ucb_process* proc = ucb_process_spawn(&opts, &err);
    REQUIRE(proc != UCB_NULL);

    int code = -1;
    CHECK_FALSE(ucb_process_wait(proc, 50, &code, &err));
    CHECK(err == UCB_NULL);

    CHECK(ucb_process_terminate(proc, &err));
    CHECK(ucb_process_wait(proc, -1, &code, &err));

#ifndef _WIN32
    // A SIGTERM terminated child maps to 128 + signal.
    CHECK(code == 128 + SIGTERM);
#endif

    ucb_process_free(proc);
    ucb_file_free(devnull);

    UCB_MEMTRACK_POP();
}

TEST_CASE("process - detach")
{
    UCB_MEMTRACK_PUSH();

    std::vector<std::string> args = make_shell_args("exit 0");
    std::vector<const char*> argv = to_argv(args);
    ucb_error* err = UCB_NULL;

    ucb_process_opts opts = ucb_process_opts_make();
    opts.argv = argv.data();
    opts.flags = UCB_PROCESS_DETACH;

    ucb_process* proc = ucb_process_spawn(&opts, &err);
    REQUIRE(proc != UCB_NULL);

    int code = -1;
    CHECK(ucb_process_wait(proc, -1, &code, &err));
    CHECK(code == 0);

    ucb_process_free(proc);

    UCB_MEMTRACK_POP();
}

TEST_CASE("process - free reap")
{
    UCB_MEMTRACK_PUSH();

    std::vector<std::string> args = make_shell_args("exit 0");
    std::vector<const char*> argv = to_argv(args);
    ucb_error* err = UCB_NULL;

    ucb_process_opts opts = ucb_process_opts_make();
    opts.argv = argv.data();

    ucb_process* proc = ucb_process_spawn(&opts, &err);
    REQUIRE(proc != UCB_NULL);

    // Releasing without waiting must not block or leak the handle.
    ucb_process_free(proc);

    UCB_MEMTRACK_POP();
}

TEST_CASE("process - capture rejects detach")
{
    UCB_MEMTRACK_PUSH();

    std::vector<std::string> args = make_shell_args("exit 0");
    std::vector<const char*> argv = to_argv(args);
    ucb_error* err = UCB_NULL;

    ucb_process_capture_opts opts = ucb_process_capture_opts_make();
    opts.argv = argv.data();
    opts.flags = UCB_PROCESS_DETACH;

    CHECK_FALSE(ucb_process_capture(&opts, UCB_NULL, UCB_NULL, UCB_NULL, &err));
    REQUIRE(err != UCB_NULL);
    CHECK(err->code == UCB_ERROR_INVALID_ARG);
    ucb_error_clear(&err);

    UCB_MEMTRACK_POP();
}

TEST_CASE("process - capture rejects shared buffer")
{
    UCB_MEMTRACK_PUSH();

    std::vector<std::string> args = make_shell_args("exit 0");
    std::vector<const char*> argv = to_argv(args);
    ucb_error* err = UCB_NULL;

    ucb_buffer* buf = ucb_buffer_new_heap(16);

    ucb_process_capture_opts opts = ucb_process_capture_opts_make();
    opts.argv = argv.data();

    CHECK_FALSE(ucb_process_capture(&opts, buf, buf, UCB_NULL, &err));
    REQUIRE(err != UCB_NULL);
    CHECK(err->code == UCB_ERROR_INVALID_ARG);
    ucb_error_clear(&err);

    ucb_buffer_free(buf);

    UCB_MEMTRACK_POP();
}

TEST_CASE_FIXTURE(TestFailureFixture, "process - invalid args")
{
    UCB_MEMTRACK_PUSH();

    {
        SUBCASE("null opts")
        {
            CHECK_ABORTS(ucb_process_spawn(UCB_NULL, UCB_NULL));
        }
    }

    {
        SUBCASE("null argv")
        {
            ucb_process_opts opts = ucb_process_opts_make();
            opts.argv = UCB_NULL;
            CHECK_ABORTS(ucb_process_spawn(&opts, UCB_NULL));
        }
    }

    {
        SUBCASE("empty argv")
        {
            const char* bad[] = {UCB_NULL};
            ucb_process_opts opts = ucb_process_opts_make();
            opts.argv = bad;
            CHECK_ABORTS(ucb_process_spawn(&opts, UCB_NULL));
        }
    }

    {
        SUBCASE("empty cwd")
        {
            std::vector<std::string> args = make_shell_args("exit 0");
            std::vector<const char*> argv = to_argv(args);
            ucb_process_opts opts = ucb_process_opts_make();
            opts.argv = argv.data();
            opts.cwd = "";

            ucb_error* err = UCB_NULL;
            CHECK(ucb_process_spawn(&opts, &err) == UCB_NULL);
            REQUIRE(err != UCB_NULL);
            CHECK(err->code == UCB_ERROR_INVALID_ARG);
            ucb_error_clear(&err);
        }
    }

    {
        SUBCASE("invalid redirect handle")
        {
            ucb_error* err = UCB_NULL;
            ucb_file* closed = ucb_file_open(null_device(), UCB_FILE_READ, &err);
            REQUIRE(closed != UCB_NULL);
            REQUIRE(ucb_file_close(closed, &err));

            std::vector<std::string> args = make_shell_args("exit 0");
            std::vector<const char*> argv = to_argv(args);
            ucb_process_opts opts = ucb_process_opts_make();
            opts.argv = argv.data();
            opts.in = closed;

            CHECK(ucb_process_spawn(&opts, &err) == UCB_NULL);
            REQUIRE(err != UCB_NULL);
            CHECK(err->code == UCB_ERROR_INVALID_STATE);
            ucb_error_clear(&err);

            ucb_file_free(closed);
        }
    }

    UCB_MEMTRACK_POP();
}

TEST_SUITE_END();
