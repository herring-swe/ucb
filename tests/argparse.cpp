/**
 * @file argparse.cpp
 *
 * This file is part of the UCB project
 * - SPDX-FileCopyrightText: © 2026 Åke Svedin <ake@svedin.org>
 * - SPDX-License-Identifier: MIT
 *
 * @brief argument parser tests
 */

#include "common.h"
#include "fs_tmp.h"

#include <ucb/argparse.h>
#include <ucb/container/impl/vector_str.h>
#include <ucb/error.h>
#include <ucb/file.h>
#include <ucb/memdbg.h>
#include <ucb/string.h>

#include <doctest.h>

#include <string>
#include <vector>

/* -------------------------------------------------------------------------- */
/*                                   Helpers                                  */
/* -------------------------------------------------------------------------- */

static std::vector<std::string> args_with_prog(std::initializer_list<const char*> items)
{
    std::vector<std::string> v;
    v.reserve(items.size() + 1);
    v.emplace_back("prog");
    for (const char* item : items)
        v.emplace_back(item);
    return v;
}

static std::vector<const char*> to_argv(const std::vector<std::string>& v)
{
    std::vector<const char*> out;
    out.reserve(v.size());
    for (const auto& s : v)
        out.push_back(s.c_str());
    return out;
}

static std::string str_text(const ucb_str* s)
{
    return std::string(s->data, s->size);
}

static ucb_arg_status run_parse(ucb_arg_parser* p,
                                const std::vector<std::string>& args,
                                std::vector<const char*>& argv,
                                ucb_vector_str* positionals,
                                ucb_error** perr)
{
    argv = to_argv(args);
    return ucb_arg_parser_parse(p, (int)argv.size(), argv.data(), positionals, perr);
}

/* -------------------------------------------------------------------------- */
/*                                   Tests                                    */
/* -------------------------------------------------------------------------- */

TEST_SUITE_BEGIN("argparse");

TEST_CASE("argparse - flags and bundles")
{
    UCB_MEMTRACK_PUSH();

    ucb_error* err = UCB_NULL;
    ucb_arg_parser* p = ucb_arg_parser_new("prog", &err);
    REQUIRE(p != UCB_NULL);

    int verbose = 0;
    bool all = false;
    bool brief = true;

    ucb_arg_opt opt = ucb_arg_opt_make();
    opt.name = "verbose";
    opt.short_name = 'v';
    opt.long_name = "verbose";
    opt.action = UCB_ARG_ACTION_COUNT;
    opt.dest = &verbose;
    REQUIRE(ucb_arg_parser_add(p, &opt, &err));

    opt = ucb_arg_opt_make();
    opt.name = "all";
    opt.short_name = 'a';
    opt.action = UCB_ARG_ACTION_STORE_TRUE;
    opt.dest = &all;
    REQUIRE(ucb_arg_parser_add(p, &opt, &err));

    opt = ucb_arg_opt_make();
    opt.name = "brief";
    opt.short_name = 'b';
    opt.action = UCB_ARG_ACTION_STORE_FALSE;
    opt.dest = &brief;
    REQUIRE(ucb_arg_parser_add(p, &opt, &err));

    std::vector<const char*> argv;
    std::vector<std::string> args = args_with_prog({"-vvv", "-ab"});
    CHECK(run_parse(p, args, argv, UCB_NULL, &err) == UCB_ARG_OK);
    CHECK(err == UCB_NULL);
    CHECK(verbose == 3);
    CHECK(all == true);
    CHECK(brief == false);

    args = args_with_prog({"--verbose", "--verbose"});
    CHECK(run_parse(p, args, argv, UCB_NULL, &err) == UCB_ARG_OK);
    CHECK(verbose == 2);

    ucb_arg_parser_free(p);

    UCB_MEMTRACK_POP();
}

TEST_CASE("argparse - typed values")
{
    UCB_MEMTRACK_PUSH();

    ucb_error* err = UCB_NULL;
    ucb_arg_parser* p = ucb_arg_parser_new("prog", &err);
    REQUIRE(p != UCB_NULL);

    int i = 0;
    unsigned int u = 0;
    long l = 0;
    unsigned long long ull = 0;
    size_t sz = 0;
    float f = 0.0f;
    double d = 0.0;
    bool b = false;
    ucb_str s = ucb_str_make();

    ucb_arg_opt opt = ucb_arg_opt_make();
    opt.name = "int";
    opt.long_name = "int";
    opt.type = UCB_ARG_TYPE_INT;
    opt.dest = &i;
    REQUIRE(ucb_arg_parser_add(p, &opt, &err));

    opt = ucb_arg_opt_make();
    opt.name = "uint";
    opt.long_name = "uint";
    opt.type = UCB_ARG_TYPE_UINT;
    opt.dest = &u;
    REQUIRE(ucb_arg_parser_add(p, &opt, &err));

    opt = ucb_arg_opt_make();
    opt.name = "long";
    opt.long_name = "long";
    opt.type = UCB_ARG_TYPE_LONG;
    opt.dest = &l;
    REQUIRE(ucb_arg_parser_add(p, &opt, &err));

    opt = ucb_arg_opt_make();
    opt.name = "ull";
    opt.long_name = "ull";
    opt.type = UCB_ARG_TYPE_ULLONG;
    opt.dest = &ull;
    REQUIRE(ucb_arg_parser_add(p, &opt, &err));

    opt = ucb_arg_opt_make();
    opt.name = "size";
    opt.long_name = "size";
    opt.type = UCB_ARG_TYPE_SIZE;
    opt.dest = &sz;
    REQUIRE(ucb_arg_parser_add(p, &opt, &err));

    opt = ucb_arg_opt_make();
    opt.name = "float";
    opt.long_name = "float";
    opt.type = UCB_ARG_TYPE_FLOAT;
    opt.dest = &f;
    REQUIRE(ucb_arg_parser_add(p, &opt, &err));

    opt = ucb_arg_opt_make();
    opt.name = "double";
    opt.long_name = "double";
    opt.type = UCB_ARG_TYPE_DOUBLE;
    opt.dest = &d;
    REQUIRE(ucb_arg_parser_add(p, &opt, &err));

    opt = ucb_arg_opt_make();
    opt.name = "bool";
    opt.long_name = "bool";
    opt.type = UCB_ARG_TYPE_BOOL;
    opt.dest = &b;
    REQUIRE(ucb_arg_parser_add(p, &opt, &err));

    opt = ucb_arg_opt_make();
    opt.name = "str";
    opt.long_name = "str";
    opt.type = UCB_ARG_TYPE_STR;
    opt.dest = &s;
    REQUIRE(ucb_arg_parser_add(p, &opt, &err));

    std::vector<const char*> argv;
    std::vector<std::string> args = args_with_prog({"--int=-42",
                                                    "--uint=42",
                                                    "--long=100000",
                                                    "--ull=18446744073709551615",
                                                    "--size=123",
                                                    "--float=3.5",
                                                    "--double=2.25",
                                                    "--bool=yes",
                                                    "--str=hello"});
    CHECK(run_parse(p, args, argv, UCB_NULL, &err) == UCB_ARG_OK);
    CHECK(i == -42);
    CHECK(u == 42u);
    CHECK(l == 100000L);
    CHECK(ull == 18446744073709551615ull);
    CHECK(sz == 123u);
    CHECK(f == doctest::Approx(3.5f));
    CHECK(d == doctest::Approx(2.25));
    CHECK(b == true);
    CHECK(str_text(&s) == "hello");

    // The parser overwrites an already owned string without leaking.
    args = args_with_prog({"--str=again"});
    CHECK(run_parse(p, args, argv, UCB_NULL, &err) == UCB_ARG_OK);
    CHECK(str_text(&s) == "again");

    ucb_str_release(&s);
    ucb_arg_parser_free(p);

    UCB_MEMTRACK_POP();
}

TEST_CASE("argparse - value forms")
{
    UCB_MEMTRACK_PUSH();

    ucb_error* err = UCB_NULL;
    ucb_arg_parser* p = ucb_arg_parser_new("prog", &err);
    REQUIRE(p != UCB_NULL);

    ucb_str out = ucb_str_make();
    ucb_arg_opt opt = ucb_arg_opt_make();
    opt.name = "output";
    opt.short_name = 'o';
    opt.long_name = "output";
    opt.type = UCB_ARG_TYPE_STR;
    opt.dest = &out;
    REQUIRE(ucb_arg_parser_add(p, &opt, &err));

    std::vector<const char*> argv;
    std::vector<std::string> args = args_with_prog({"--output=file1"});
    CHECK(run_parse(p, args, argv, UCB_NULL, &err) == UCB_ARG_OK);
    CHECK(str_text(&out) == "file1");

    args = args_with_prog({"-ofile2"});
    CHECK(run_parse(p, args, argv, UCB_NULL, &err) == UCB_ARG_OK);
    CHECK(str_text(&out) == "file2");

    args = args_with_prog({"-o", "file3"});
    CHECK(run_parse(p, args, argv, UCB_NULL, &err) == UCB_ARG_OK);
    CHECK(str_text(&out) == "file3");

    ucb_str_release(&out);
    ucb_arg_parser_free(p);

    UCB_MEMTRACK_POP();
}

TEST_CASE("argparse - append action")
{
    UCB_MEMTRACK_PUSH();

    ucb_error* err = UCB_NULL;
    ucb_arg_parser* p = ucb_arg_parser_new("prog", &err);
    REQUIRE(p != UCB_NULL);

    ucb_vector_str* inc = ucb_vector_str_new();
    REQUIRE(inc != UCB_NULL);

    ucb_arg_opt opt = ucb_arg_opt_make();
    opt.name = "include";
    opt.short_name = 'I';
    opt.long_name = "include";
    opt.action = UCB_ARG_ACTION_APPEND;
    opt.dest = inc;
    REQUIRE(ucb_arg_parser_add(p, &opt, &err));

    std::vector<const char*> argv;
    std::vector<std::string> args = args_with_prog({"-I", "a", "--include=b", "-Ic"});
    CHECK(run_parse(p, args, argv, UCB_NULL, &err) == UCB_ARG_OK);
    REQUIRE(ucb_vector_str_size(inc) == 3);
    CHECK(str_text(ucb_vector_str_get(inc, 0)) == "a");
    CHECK(str_text(ucb_vector_str_get(inc, 1)) == "b");
    CHECK(str_text(ucb_vector_str_get(inc, 2)) == "c");

    // Parsing again clears the previously appended values.
    args = args_with_prog({"-I", "z"});
    CHECK(run_parse(p, args, argv, UCB_NULL, &err) == UCB_ARG_OK);
    REQUIRE(ucb_vector_str_size(inc) == 1);
    CHECK(str_text(ucb_vector_str_get(inc, 0)) == "z");

    ucb_vector_str_free_full(inc);
    ucb_arg_parser_free(p);

    UCB_MEMTRACK_POP();
}

TEST_CASE("argparse - positionals")
{
    UCB_MEMTRACK_PUSH();

    ucb_error* err = UCB_NULL;
    ucb_arg_parser* p = ucb_arg_parser_new("prog", &err);
    REQUIRE(p != UCB_NULL);

    bool verbose = false;
    ucb_arg_opt opt = ucb_arg_opt_make();
    opt.name = "verbose";
    opt.short_name = 'v';
    opt.action = UCB_ARG_ACTION_STORE_TRUE;
    opt.dest = &verbose;
    REQUIRE(ucb_arg_parser_add(p, &opt, &err));

    std::vector<const char*> argv;
    std::vector<std::string> args = args_with_prog({"one", "--", "-x", "--flag", "two"});
    ucb_vector_str* pos = ucb_vector_str_new();
    REQUIRE(pos != UCB_NULL);
    CHECK(run_parse(p, args, argv, pos, &err) == UCB_ARG_OK);
    CHECK(verbose == false);
    REQUIRE(ucb_vector_str_size(pos) == 4);
    CHECK(str_text(ucb_vector_str_get(pos, 0)) == "one");
    CHECK(str_text(ucb_vector_str_get(pos, 1)) == "-x");
    CHECK(str_text(ucb_vector_str_get(pos, 2)) == "--flag");
    CHECK(str_text(ucb_vector_str_get(pos, 3)) == "two");
    ucb_vector_str_free_full(pos);

    // Interspersed options and negative number lookalikes.
    args = args_with_prog({"a", "-v", "-5", "-0.5", "-", "b"});
    pos = ucb_vector_str_new();
    REQUIRE(pos != UCB_NULL);
    CHECK(run_parse(p, args, argv, pos, &err) == UCB_ARG_OK);
    CHECK(verbose == true);
    REQUIRE(ucb_vector_str_size(pos) == 5);
    CHECK(str_text(ucb_vector_str_get(pos, 0)) == "a");
    CHECK(str_text(ucb_vector_str_get(pos, 1)) == "-5");
    CHECK(str_text(ucb_vector_str_get(pos, 2)) == "-0.5");
    CHECK(str_text(ucb_vector_str_get(pos, 3)) == "-");
    CHECK(str_text(ucb_vector_str_get(pos, 4)) == "b");
    ucb_vector_str_free_full(pos);

    ucb_arg_parser_free(p);

    UCB_MEMTRACK_POP();
}

TEST_CASE("argparse - defaults and interpolation")
{
    UCB_MEMTRACK_PUSH();

    ucb_error* err = UCB_NULL;
    ucb_arg_parser* p = ucb_arg_parser_new("/usr/bin/myprog", &err);
    REQUIRE(p != UCB_NULL);
    REQUIRE(ucb_arg_parser_set_description(p, "The %prog tool"));

    int num = 0;
    ucb_str name = ucb_str_make();

    ucb_arg_opt opt = ucb_arg_opt_make();
    opt.name = "num";
    opt.long_name = "num";
    opt.type = UCB_ARG_TYPE_INT;
    opt.dest = &num;
    opt.def = "42";
    opt.help = "Number of items";
    REQUIRE(ucb_arg_parser_add(p, &opt, &err));

    opt = ucb_arg_opt_make();
    opt.name = "name";
    opt.short_name = 'n';
    opt.long_name = "name";
    opt.type = UCB_ARG_TYPE_STR;
    opt.dest = &name;
    opt.def = "default-name";
    opt.help = "%(name)s value";
    REQUIRE(ucb_arg_parser_add(p, &opt, &err));

    std::vector<const char*> argv;
    std::vector<std::string> args = args_with_prog({});
    CHECK(run_parse(p, args, argv, UCB_NULL, &err) == UCB_ARG_OK);
    CHECK(num == 42);
    CHECK(str_text(&name) == "default-name");

    ucb_str* help = ucb_arg_parser_help_str(p, &err);
    REQUIRE(help != UCB_NULL);
    std::string text = str_text(help);
    CHECK(text.find("The myprog tool") != std::string::npos);
    CHECK(text.find("Number of items (default: 42)") != std::string::npos);
    CHECK(text.find("name value") != std::string::npos);
    ucb_str_free(help);

    ucb_str_release(&name);
    ucb_arg_parser_free(p);

    UCB_MEMTRACK_POP();
}

TEST_CASE("argparse - defaults and interpolation edge cases")
{
    UCB_MEMTRACK_PUSH();

    ucb_error* err = UCB_NULL;

    // Unsigned converters reject a negative sign even after leading whitespace.
    {
        ucb_arg_parser* p = ucb_arg_parser_new("prog", &err);
        size_t sz = 5;
        ucb_arg_opt opt = ucb_arg_opt_make();
        opt.name = "size";
        opt.long_name = "size";
        opt.type = UCB_ARG_TYPE_SIZE;
        opt.dest = &sz;
        REQUIRE(ucb_arg_parser_add(p, &opt, &err));

        std::vector<const char*> argv;
        std::vector<std::string> args = args_with_prog({"--size=-1"});
        CHECK(run_parse(p, args, argv, UCB_NULL, &err) == UCB_ARG_ERROR);
        REQUIRE(err != UCB_NULL);
        CHECK(err->code == UCB_ERROR_INVALID_ARG);
        ucb_error_clear(&err);

        args = args_with_prog({"--size= -1"});
        CHECK(run_parse(p, args, argv, UCB_NULL, &err) == UCB_ARG_ERROR);
        REQUIRE(err != UCB_NULL);
        ucb_error_clear(&err);

        args = args_with_prog({"--size=10"});
        CHECK(run_parse(p, args, argv, UCB_NULL, &err) == UCB_ARG_OK);
        CHECK(sz == 10u);
        ucb_arg_parser_free(p);
    }

    // An invalid string default is reported instead of silently ignored.
    {
        ucb_arg_parser* p = ucb_arg_parser_new("prog", &err);
        int num = 123;
        ucb_arg_opt opt = ucb_arg_opt_make();
        opt.name = "num";
        opt.long_name = "num";
        opt.type = UCB_ARG_TYPE_INT;
        opt.dest = &num;
        opt.def = "abc";
        REQUIRE(ucb_arg_parser_add(p, &opt, &err));

        std::vector<const char*> argv;
        std::vector<std::string> args = args_with_prog({});
        CHECK(run_parse(p, args, argv, UCB_NULL, &err) == UCB_ARG_ERROR);
        REQUIRE(err != UCB_NULL);
        CHECK(err->code == UCB_ERROR_INVALID_ARG);
        ucb_error_clear(&err);
        ucb_arg_parser_free(p);
    }

    // Help rendering does not duplicate whitespace, explicit defaults or malformed fields.
    {
        ucb_arg_parser* p = ucb_arg_parser_new("prog", &err);
        REQUIRE(ucb_arg_parser_set_description(p, "  alpha beta"));

        int num = 0;
        ucb_arg_opt opt = ucb_arg_opt_make();
        opt.name = "num";
        opt.long_name = "num";
        opt.type = UCB_ARG_TYPE_INT;
        opt.dest = &num;
        opt.def = "7";
        opt.help = "value %(default)s";
        REQUIRE(ucb_arg_parser_add(p, &opt, &err));

        opt = ucb_arg_opt_make();
        opt.name = "raw";
        opt.long_name = "raw";
        opt.type = UCB_ARG_TYPE_STR;
        ucb_str raw = ucb_str_make();
        opt.dest = &raw;
        opt.help = "%()s";
        REQUIRE(ucb_arg_parser_add(p, &opt, &err));

        ucb_str* help = ucb_arg_parser_help_str(p, &err);
        REQUIRE(help != UCB_NULL);
        std::string text = str_text(help);

        CHECK(text.find("alpha beta") != std::string::npos);
        size_t first = text.find("alpha");
        REQUIRE(first != std::string::npos);
        CHECK(text.find("alpha", first + 1) == std::string::npos);
        CHECK(text.find("value 7") != std::string::npos);
        CHECK(text.find("(default:") == std::string::npos);
        CHECK(text.find("%()s") != std::string::npos);

        ucb_str_free(help);
        ucb_str_release(&raw);
        ucb_arg_parser_free(p);
    }

    UCB_MEMTRACK_POP();
}

TEST_CASE("argparse - choices and required")
{
    UCB_MEMTRACK_PUSH();

    ucb_error* err = UCB_NULL;
    ucb_arg_parser* p = ucb_arg_parser_new("prog", &err);
    REQUIRE(p != UCB_NULL);

    static const char* const color_choices[] = {"red", "green"};
    ucb_str color = ucb_str_make();
    ucb_str req = ucb_str_make();

    ucb_arg_opt opt = ucb_arg_opt_make();
    opt.name = "color";
    opt.long_name = "color";
    opt.type = UCB_ARG_TYPE_STR;
    opt.dest = &color;
    opt.choices = color_choices;
    opt.num_choices = 2;
    REQUIRE(ucb_arg_parser_add(p, &opt, &err));

    opt = ucb_arg_opt_make();
    opt.name = "req";
    opt.long_name = "req";
    opt.type = UCB_ARG_TYPE_STR;
    opt.dest = &req;
    opt.required = true;
    REQUIRE(ucb_arg_parser_add(p, &opt, &err));

    std::vector<const char*> argv;
    std::vector<std::string> args = args_with_prog({"--color=red", "--req=ok"});
    CHECK(run_parse(p, args, argv, UCB_NULL, &err) == UCB_ARG_OK);
    CHECK(str_text(&color) == "red");

    args = args_with_prog({"--color=blue", "--req=ok"});
    CHECK(run_parse(p, args, argv, UCB_NULL, &err) == UCB_ARG_ERROR);
    REQUIRE(err != UCB_NULL);
    CHECK(err->code == UCB_ERROR_INVALID_ARG);
    ucb_error_clear(&err);

    args = args_with_prog({"--color=green"});
    CHECK(run_parse(p, args, argv, UCB_NULL, &err) == UCB_ARG_ERROR);
    REQUIRE(err != UCB_NULL);
    CHECK(err->code == UCB_ERROR_INVALID_ARG);
    ucb_error_clear(&err);

    ucb_str_release(&color);
    ucb_str_release(&req);
    ucb_arg_parser_free(p);

    UCB_MEMTRACK_POP();
}

TEST_CASE("argparse - errors")
{
    UCB_MEMTRACK_PUSH();

    ucb_error* err = UCB_NULL;
    ucb_arg_parser* p = ucb_arg_parser_new("prog", &err);
    REQUIRE(p != UCB_NULL);

    int num = 0;
    bool flag = false;

    ucb_arg_opt opt = ucb_arg_opt_make();
    opt.name = "num";
    opt.long_name = "num";
    opt.type = UCB_ARG_TYPE_INT;
    opt.dest = &num;
    REQUIRE(ucb_arg_parser_add(p, &opt, &err));

    opt = ucb_arg_opt_make();
    opt.name = "flag";
    opt.long_name = "flag";
    opt.action = UCB_ARG_ACTION_STORE_TRUE;
    opt.dest = &flag;
    REQUIRE(ucb_arg_parser_add(p, &opt, &err));

    std::vector<const char*> argv;
    std::vector<std::string> args;

    auto expect_error = [&](std::initializer_list<const char*> items) {
        args = args_with_prog(items);
        CHECK(run_parse(p, args, argv, UCB_NULL, &err) == UCB_ARG_ERROR);
        REQUIRE(err != UCB_NULL);
        CHECK(err->code == UCB_ERROR_INVALID_ARG);
        ucb_error_clear(&err);
    };

    expect_error({"--nope"});
    expect_error({"-z"});
    expect_error({"--num"});
    expect_error({"-n"});
    expect_error({"--num=abc"});
    expect_error({"--num=99999999999999999999"});
    expect_error({"--flag=1"});

    // Duplicate and reserved names are rejected at add time.
    opt = ucb_arg_opt_make();
    opt.name = "num2";
    opt.long_name = "num";
    opt.type = UCB_ARG_TYPE_INT;
    opt.dest = &num;
    CHECK_FALSE(ucb_arg_parser_add(p, &opt, &err));
    REQUIRE(err != UCB_NULL);
    CHECK(err->code == UCB_ERROR_INVALID_ARG);
    ucb_error_clear(&err);

    opt = ucb_arg_opt_make();
    opt.name = "help2";
    opt.long_name = "help";
    opt.type = UCB_ARG_TYPE_INT;
    opt.dest = &num;
    CHECK_FALSE(ucb_arg_parser_add(p, &opt, &err));
    REQUIRE(err != UCB_NULL);
    CHECK(err->code == UCB_ERROR_INVALID_ARG);
    ucb_error_clear(&err);

    opt = ucb_arg_opt_make();
    opt.name = "novalue";
    opt.long_name = "novalue";
    opt.type = UCB_ARG_TYPE_NONE;
    opt.action = UCB_ARG_ACTION_STORE;
    opt.dest = &num;
    CHECK_FALSE(ucb_arg_parser_add(p, &opt, &err));
    REQUIRE(err != UCB_NULL);
    CHECK(err->code == UCB_ERROR_INVALID_ARG);
    ucb_error_clear(&err);

    ucb_arg_parser_free(p);

    UCB_MEMTRACK_POP();
}

TEST_CASE("argparse - add_options table")
{
    UCB_MEMTRACK_PUSH();

    static int table_count = 0;
    static bool table_flag = false;

    static ucb_arg_opt s_table[] = {
        {"count",
         'c',
         "count",
         UCB_ARG_TYPE_NONE,
         UCB_ARG_ACTION_COUNT,
         &table_count,
         UCB_NULL,
         UCB_NULL,
         "count occurrences",
         UCB_NULL,
         0,
         false},
        {"flag",
         'f',
         "flag",
         UCB_ARG_TYPE_NONE,
         UCB_ARG_ACTION_STORE_TRUE,
         &table_flag,
         UCB_NULL,
         UCB_NULL,
         "set the flag",
         UCB_NULL,
         0,
         false},
    };

    ucb_error* err = UCB_NULL;
    ucb_arg_parser* p = ucb_arg_parser_new("prog", &err);
    REQUIRE(p != UCB_NULL);
    REQUIRE(ucb_arg_parser_add_options(p, s_table, 2, &err));

    std::vector<const char*> argv;
    std::vector<std::string> args = args_with_prog({"-cc", "-f"});
    CHECK(run_parse(p, args, argv, UCB_NULL, &err) == UCB_ARG_OK);
    CHECK(table_count == 2);
    CHECK(table_flag == true);

    ucb_arg_parser_free(p);

    UCB_MEMTRACK_POP();
}

TEST_CASE("argparse - help formatting")
{
    UCB_MEMTRACK_PUSH();

    ucb_error* err = UCB_NULL;
    ucb_arg_parser* p = ucb_arg_parser_new("/usr/bin/myprog", &err);
    REQUIRE(p != UCB_NULL);
    REQUIRE(ucb_arg_parser_set_usage(p, "%prog [options] FILE"));
    REQUIRE(ucb_arg_parser_set_description(p, "Does a thing with FILE."));
    ucb_arg_parser_set_width(p, 60);

    ucb_str out = ucb_str_make();
    int num = 0;
    bool flag = false;

    ucb_arg_opt opt = ucb_arg_opt_make();
    opt.name = "output";
    opt.short_name = 'o';
    opt.long_name = "output";
    opt.type = UCB_ARG_TYPE_STR;
    opt.dest = &out;
    opt.metavar = "FILE";
    opt.help = "Write the result to FILE.";
    REQUIRE(ucb_arg_parser_add(p, &opt, &err));

    opt = ucb_arg_opt_make();
    opt.name = "num";
    opt.long_name = "num";
    opt.type = UCB_ARG_TYPE_INT;
    opt.dest = &num;
    opt.def = "5";
    opt.help = "Number of items to process in one go, wrapped when long.";
    REQUIRE(ucb_arg_parser_add(p, &opt, &err));

    opt = ucb_arg_opt_make();
    opt.name = "flag";
    opt.short_name = 'f';
    opt.action = UCB_ARG_ACTION_STORE_TRUE;
    opt.dest = &flag;
    opt.help = "Set the flag.";
    REQUIRE(ucb_arg_parser_add(p, &opt, &err));

    ucb_str* help = ucb_arg_parser_help_str(p, &err);
    REQUIRE(help != UCB_NULL);
    std::string text = str_text(help);

    CHECK(text.rfind("Usage: myprog [options] FILE\n", 0) == 0);
    CHECK(text.find("Does a thing with FILE.") != std::string::npos);
    CHECK(text.find("Options:") != std::string::npos);
    CHECK(text.find("-h, --help") != std::string::npos);
    CHECK(text.find("-o, --output=FILE") != std::string::npos);
    CHECK(text.find("Write the result to FILE.") != std::string::npos);
    CHECK(text.find("default: 5") != std::string::npos);
    CHECK(text.find("    --num=NUM") != std::string::npos);

    // Verify the printed output matches the string form.
    FsTmp tmp("ucb_argparse");
    ucb_file* file =
        ucb_file_open(tmp.path("help.txt").c_str(),
                      UCB_FILE_WRITE | UCB_FILE_CREATE | UCB_FILE_TRUNCATE | UCB_FILE_BINARY,
                      &err);
    REQUIRE(file != UCB_NULL);
    CHECK(ucb_arg_parser_print_help(p, file, &err) == true);
    ucb_file_free(file);

    std::string readback;
    REQUIRE(tmp.read("help.txt", readback));
    CHECK(readback == text);

    ucb_str_free(help);
    ucb_str_release(&out);
    ucb_arg_parser_free(p);

    UCB_MEMTRACK_POP();
}

TEST_CASE("argparse - version and help status")
{
    UCB_MEMTRACK_PUSH();

    ucb_error* err = UCB_NULL;
    ucb_arg_parser* p = ucb_arg_parser_new("prog", &err);
    REQUIRE(p != UCB_NULL);
    REQUIRE(ucb_arg_parser_set_version(p, "1.2.3"));

    ucb_str* version = ucb_arg_parser_version_str(p, &err);
    REQUIRE(version != UCB_NULL);
    CHECK(str_text(version) == "prog version 1.2.3\n");
    ucb_str_free(version);

    std::vector<const char*> argv;
    std::vector<std::string> args = args_with_prog({"--version"});
    CHECK(run_parse(p, args, argv, UCB_NULL, &err) == UCB_ARG_VERSION);
    CHECK(err == UCB_NULL);

    args = args_with_prog({"--help", "--bogus"});
    CHECK(run_parse(p, args, argv, UCB_NULL, &err) == UCB_ARG_HELP);
    CHECK(err == UCB_NULL);

    args = args_with_prog({"-h"});
    CHECK(run_parse(p, args, argv, UCB_NULL, &err) == UCB_ARG_HELP);

    ucb_arg_parser_free(p);

    UCB_MEMTRACK_POP();
}

TEST_CASE("argparse - print error")
{
    UCB_MEMTRACK_PUSH();

    ucb_error* err = UCB_NULL;
    ucb_arg_parser* p = ucb_arg_parser_new("myprog", &err);
    REQUIRE(p != UCB_NULL);

    int num = 0;
    ucb_arg_opt opt = ucb_arg_opt_make();
    opt.name = "num";
    opt.long_name = "num";
    opt.type = UCB_ARG_TYPE_INT;
    opt.dest = &num;
    REQUIRE(ucb_arg_parser_add(p, &opt, &err));

    std::vector<const char*> argv;
    std::vector<std::string> args = args_with_prog({"--num=abc"});
    CHECK(run_parse(p, args, argv, UCB_NULL, &err) == UCB_ARG_ERROR);
    REQUIRE(err != UCB_NULL);

    FsTmp tmp("ucb_argparse_err");
    ucb_file* file =
        ucb_file_open(tmp.path("err.txt").c_str(),
                      UCB_FILE_WRITE | UCB_FILE_CREATE | UCB_FILE_TRUNCATE | UCB_FILE_BINARY,
                      &err);
    REQUIRE(file != UCB_NULL);
    CHECK(ucb_arg_parser_print_error(p, err, file, &err));
    ucb_file_free(file);

    std::string readback;
    REQUIRE(tmp.read("err.txt", readback));
    CHECK(readback.find("Usage: myprog") != std::string::npos);
    CHECK(readback.find("myprog: error:") != std::string::npos);

    ucb_error_clear(&err);
    ucb_arg_parser_free(p);

    UCB_MEMTRACK_POP();
}

TEST_SUITE_END();