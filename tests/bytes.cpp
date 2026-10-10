/**
 * @file bytes.cpp
 *
 * This file is part of the UCB project
 * - SPDX-FileCopyrightText: © 2026 Åke Svedin <ake@svedin.org>
 * - SPDX-License-Identifier: MIT
 *
 * @brief byte size unit tests
 */

#include "test_bytes_c.h"

#include <ucb/error.h>
#include <ucb/memdbg.h>
#include <ucb/string.h>
#include <ucb/types.h>
#include <ucb/units/bytes.h>

#include <doctest.h>

#include <cstdint>
#include <cstring>
#include <string>

static std::string bytes_str(ucb_bytes_value value, int precision)
{
    ucb_str* s = ucb_bytes_to_str(value, precision);
    REQUIRE(s != UCB_NULL);
    std::string out(ucb_str_cstr(s));
    ucb_str_free(s);
    return out;
}

TEST_SUITE_BEGIN("bytes");

TEST_CASE("bytes - convert si")
{
    UCB_MEMTRACK_PUSH();

    SUBCASE("auto")
    {
        ucb_bytes_value r = ucb_bytes_convert(0, UCB_BYTES_AUTO_SI);
        CHECK(r.unit == UCB_BYTES_B);
        CHECK(r.value == 0.0);

        r = ucb_bytes_convert(999, UCB_BYTES_AUTO_SI);
        CHECK(r.unit == UCB_BYTES_B);
        CHECK(r.value == 999.0);

        r = ucb_bytes_convert(1000, UCB_BYTES_AUTO_SI);
        CHECK(r.unit == UCB_BYTES_KB);
        CHECK(r.value == 1.0);

        r = ucb_bytes_convert(1500, UCB_BYTES_AUTO_SI);
        CHECK(r.unit == UCB_BYTES_KB);
        CHECK(r.value == 1.5);

        r = ucb_bytes_convert(1000000000, UCB_BYTES_AUTO_SI);
        CHECK(r.unit == UCB_BYTES_GB);
        CHECK(r.value == 1.0);

        r = ucb_bytes_convert(UINT64_MAX, UCB_BYTES_AUTO_SI);
        CHECK(r.unit == UCB_BYTES_EB);
    }

    SUBCASE("explicit")
    {
        CHECK(ucb_bytes_convert(1000000, UCB_BYTES_MB).value == 1.0);
        CHECK(ucb_bytes_convert(2000, UCB_BYTES_KB).value == 2.0);
        CHECK(ucb_bytes_convert(1, UCB_BYTES_B).value == 1.0);
    }

    SUBCASE("to_str")
    {
        CHECK(bytes_str(ucb_bytes_convert(1000, UCB_BYTES_AUTO_SI), 0) == "1 kB");
        CHECK(bytes_str(ucb_bytes_convert(1500, UCB_BYTES_AUTO_SI), 1) == "1.5 kB");
        CHECK(bytes_str({2.0, UCB_BYTES_B}, 0) == "2 B");
        CHECK(bytes_str({0.0, UCB_BYTES_B}, 0) == "0 B");
        CHECK(bytes_str({1.234, UCB_BYTES_MB}, 2) == "1.23 MB");
    }

    UCB_MEMTRACK_POP();
}

TEST_CASE("bytes - convert iec")
{
    UCB_MEMTRACK_PUSH();

    SUBCASE("auto")
    {
        ucb_bytes_value r = ucb_bytes_convert(0, UCB_BYTES_AUTO_IEC);
        CHECK(r.unit == UCB_BYTES_B);
        CHECK(r.value == 0.0);

        r = ucb_bytes_convert(1023, UCB_BYTES_AUTO_IEC);
        CHECK(r.unit == UCB_BYTES_B);
        CHECK(r.value == 1023.0);

        r = ucb_bytes_convert(1024, UCB_BYTES_AUTO_IEC);
        CHECK(r.unit == UCB_BYTES_KIB);
        CHECK(r.value == 1.0);

        r = ucb_bytes_convert(1536, UCB_BYTES_AUTO_IEC);
        CHECK(r.unit == UCB_BYTES_KIB);
        CHECK(r.value == 1.5);

        r = ucb_bytes_convert(1073741824, UCB_BYTES_AUTO_IEC);
        CHECK(r.unit == UCB_BYTES_GIB);
        CHECK(r.value == 1.0);

        r = ucb_bytes_convert(UINT64_MAX, UCB_BYTES_AUTO_IEC);
        CHECK(r.unit == UCB_BYTES_EIB);
    }

    SUBCASE("explicit")
    {
        CHECK(ucb_bytes_convert(1048576, UCB_BYTES_MIB).value == 1.0);
        CHECK(ucb_bytes_convert(2048, UCB_BYTES_KIB).value == 2.0);
    }

    SUBCASE("to_str")
    {
        CHECK(bytes_str(ucb_bytes_convert(1536, UCB_BYTES_AUTO_IEC), 0) == "2 KiB");
        CHECK(bytes_str(ucb_bytes_convert(1536, UCB_BYTES_AUTO_IEC), 1) == "1.5 KiB");
        CHECK(bytes_str({1.0, UCB_BYTES_MIB}, 0) == "1 MiB");
        CHECK(bytes_str({1024.0, UCB_BYTES_B}, 0) == "1024 B");
        CHECK(bytes_str({2.0, UCB_BYTES_GIB}, 0) == "2 GiB");
    }

    UCB_MEMTRACK_POP();
}

TEST_CASE("bytes - parse")
{
    UCB_MEMTRACK_PUSH();

    struct parse_case
    {
        const char* in;
        ucb_bytes bytes;
        ucb_bytes_unit unit;
    };

    const parse_case cases[] = {
        {"0", 0, UCB_BYTES_B},
        {"1024", 1024, UCB_BYTES_B},
        {"1B", 1, UCB_BYTES_B},
        {"1 b", 1, UCB_BYTES_B},
        {"1000kB", 1000000, UCB_BYTES_KB},
        {"10MB", 10000000, UCB_BYTES_MB},
        {"3.5MB", 3500000, UCB_BYTES_MB},
        {"1EB", 1000000000000000000ULL, UCB_BYTES_EB},
        {"512M", 536870912, UCB_BYTES_MIB},
        {"1m", 1048576, UCB_BYTES_MIB},
        {"1MiB", 1048576, UCB_BYTES_MIB},
        {"1mib", 1048576, UCB_BYTES_MIB},
        {"1Ki", 1024, UCB_BYTES_KIB},
        {"2 kb", 2000, UCB_BYTES_KB},
        {"  4K  ", 4096, UCB_BYTES_KIB},
        {"1.5GiB", 1610612736, UCB_BYTES_GIB},
        {"5T", 5497558138880ULL, UCB_BYTES_TIB},
        {"1E", 1152921504606846976ULL, UCB_BYTES_EIB},
        {"1.5", 2, UCB_BYTES_B},
        {"2.4", 2, UCB_BYTES_B},
    };

    for (const parse_case& c : cases)
    {
        CAPTURE(c.in);
        ucb_bytes_parsed r;
        ucb_error* err = UCB_NULL;
        REQUIRE(ucb_bytes_parse(c.in, &r, &err));
        CHECK(!UCB_IS_THROWN(err));
        CHECK(r.bytes == c.bytes);
        CHECK(r.unit == c.unit);
        ucb_error_clear(&err);
    }

    UCB_MEMTRACK_POP();
}

TEST_CASE("bytes - parse errors")
{
    UCB_MEMTRACK_PUSH();

    struct error_case
    {
        const char* in;
        ucb_ecode code;
    };

    const error_case cases[] = {
        {"", UCB_ERROR_INVALID_ARG},
        {"abc", UCB_ERROR_INVALID_ARG},
        {"B", UCB_ERROR_INVALID_ARG},
        {"1.2.3", UCB_ERROR_INVALID_ARG},
        {"-1", UCB_ERROR_INVALID_ARG},
        {"+1", UCB_ERROR_INVALID_ARG},
        {"1e3", UCB_ERROR_INVALID_ARG},
        {"1X", UCB_ERROR_INVALID_ARG},
        {"1KiBi", UCB_ERROR_INVALID_ARG},
        {"1.", UCB_ERROR_INVALID_ARG},
        {"1 KX", UCB_ERROR_INVALID_ARG},
        {"1Y", UCB_ERROR_INVALID_ARG},
        {"1B2", UCB_ERROR_INVALID_ARG},
        {"18446744073709551616", UCB_ERROR_OUT_OF_BOUNDS},
        {"9999999999999999999E", UCB_ERROR_OUT_OF_BOUNDS},
    };

    for (const error_case& c : cases)
    {
        CAPTURE(c.in);
        ucb_bytes_parsed r;
        ucb_error* err = UCB_NULL;
        CHECK(ucb_bytes_parse(c.in, &r, &err) == false);
        REQUIRE(UCB_IS_THROWN(err));
        CHECK(err->code == c.code);
        ucb_error_clear(&err);
    }

    UCB_MEMTRACK_POP();
}

TEST_CASE("bytes - str_buf")
{
    UCB_MEMTRACK_PUSH();

    char buf[UCB_BYTES_STRLEN];

    SUBCASE("auto si")
    {
        CHECK(ucb_bytes_str_buf(buf, sizeof buf, 1500, UCB_BYTES_AUTO_SI, 1) == buf);
        CHECK(std::string(buf) == "1.5 kB");
    }

    SUBCASE("auto iec")
    {
        CHECK(std::string(ucb_bytes_str_buf(buf, sizeof buf, 1536, UCB_BYTES_AUTO_IEC, 0)) ==
              "2 KiB");
    }

    SUBCASE("explicit unit")
    {
        CHECK(std::string(ucb_bytes_str_buf(buf, sizeof buf, 2000000, UCB_BYTES_MB, 2)) ==
              "2.00 MB");
    }

    SUBCASE("truncates and null-terminates")
    {
        char small[4];
        ucb_bytes_str_buf(small, sizeof small, 1500, UCB_BYTES_AUTO_SI, 1);
        CHECK(small[sizeof small - 1] == '\0');
    }

    UCB_MEMTRACK_POP();
}

TEST_CASE("bytes - UCB_BYTES_CSTR (C)")
{
    CHECK(test_bytes_c() == 0);
}

TEST_SUITE_END();
