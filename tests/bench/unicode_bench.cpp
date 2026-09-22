/**
 * @file unicode_bench.cpp
 * This file is part of the UCB project
 * - SPDX-FileCopyrightText: © 2026 Åke Svedin <ake@svedin.org>
 * - SPDX-License-Identifier: MIT
 *
 * @brief Benchmarks for unicode normalization with and without the quick check
 * fast path, over the official UCD NormalizationTest corpus.
 */

#include "unicode_bench.h"

#include "microbench.h"

#include "ucb/error.h"
#include "ucb/memory.h"
#include "ucb/unicode.h"
#include "ucb/unicode_private.h"

#include <cctype>
#include <cstddef>
#include <cstdint>
#include <fstream>
#include <string>
#include <vector>

#ifdef __INTELLISENSE__
#ifndef NORM_TEST_FILE
#define NORM_TEST_FILE "dummy"
#endif
#endif

namespace {

volatile std::uint64_t g_norm_sink = 0;

using norm_func = ucb_uc_result (*)(const char*, size_t, ucb_norm_form, ucb_error**);

struct norm_test
{
    std::string input;
    std::string nfc;
    std::string nfd;
    std::string nfkc;
    std::string nfkd;
};

void append_utf8(std::string& out, std::uint32_t cp)
{
    if (cp <= 0x7F)
    {
        out.push_back(static_cast<char>(cp));
    }
    else if (cp <= 0x7FF)
    {
        out.push_back(static_cast<char>(0xC0 | (cp >> 6)));
        out.push_back(static_cast<char>(0x80 | (cp & 0x3F)));
    }
    else if (cp <= 0xFFFF)
    {
        out.push_back(static_cast<char>(0xE0 | (cp >> 12)));
        out.push_back(static_cast<char>(0x80 | ((cp >> 6) & 0x3F)));
        out.push_back(static_cast<char>(0x80 | (cp & 0x3F)));
    }
    else
    {
        out.push_back(static_cast<char>(0xF0 | (cp >> 18)));
        out.push_back(static_cast<char>(0x80 | ((cp >> 12) & 0x3F)));
        out.push_back(static_cast<char>(0x80 | ((cp >> 6) & 0x3F)));
        out.push_back(static_cast<char>(0x80 | (cp & 0x3F)));
    }
}

/// Decode a whitespace separated list of hex code points into UTF-8.
std::string hex_to_utf8(const std::string& hex)
{
    std::string out;
    std::size_t index = 0;
    while (index < hex.size())
    {
        while (index < hex.size() && std::isspace(static_cast<unsigned char>(hex[index])))
            ++index;
        if (index >= hex.size())
            break;

        std::uint32_t cp = 0;
        int digits = 0;
        while (index < hex.size() && digits < 6 &&
               std::isxdigit(static_cast<unsigned char>(hex[index])))
        {
            const char c = hex[index];
            const std::uint32_t nibble =
                c >= '0' && c <= '9'   ? static_cast<std::uint32_t>(c - '0')
                : c >= 'a' && c <= 'f' ? static_cast<std::uint32_t>(c - 'a' + 10)
                : c >= 'A' && c <= 'F' ? static_cast<std::uint32_t>(c - 'A' + 10)
                                       : 0;
            cp = (cp << 4) | nibble;
            ++index;
            ++digits;
        }
        append_utf8(out, cp);
    }
    return out;
}

std::vector<std::string> split(const std::string& line, char delimiter, std::size_t max)
{
    std::vector<std::string> parts;
    std::size_t start = 0;
    for (std::size_t count = 0; count < max; ++count)
    {
        const std::size_t end = line.find(delimiter, start);
        if (end == std::string::npos)
            break;
        parts.push_back(line.substr(start, end - start));
        start = end + 1;
    }
    parts.push_back(line.substr(start));
    return parts;
}

/// Load and decode the official NormalizationTest corpus once.
const std::vector<norm_test>& corpus()
{
    static const std::vector<norm_test> tests = [] {
        std::vector<norm_test> loaded;
        std::ifstream stream(NORM_TEST_FILE, std::ios::in);
        if (!stream.is_open())
            return loaded;

        std::string line;
        while (std::getline(stream, line))
        {
            if (line.empty() || line.at(0) == '#' || line.at(0) == '@')
                continue;

            const std::vector<std::string> parts = split(line, ';', 5);
            if (parts.size() < 5)
                continue;

            norm_test test;
            test.input = hex_to_utf8(parts[0]);
            test.nfc = hex_to_utf8(parts[1]);
            test.nfd = hex_to_utf8(parts[2]);
            test.nfkc = hex_to_utf8(parts[3]);
            test.nfkd = hex_to_utf8(parts[4]);
            loaded.push_back(std::move(test));
        }
        return loaded;
    }();
    return tests;
}

/// One call converts every corpus entry from every source form into all four
/// normalization forms, mirroring the official normalization test.
void normalize_corpus(norm_func fn)
{
    static const ucb_norm_form forms[4] = {UCB_NORM_NFD,
                                           UCB_NORM_NFC,
                                           UCB_NORM_NFKD,
                                           UCB_NORM_NFKC};

    ucb_error* err = nullptr;
    std::uint64_t sink = 0;
    for (const norm_test& test : corpus())
    {
        const std::string* sources[5] = {&test.input, &test.nfc, &test.nfd, &test.nfkc, &test.nfkd};
        for (const std::string* source : sources)
        {
            for (const ucb_norm_form form : forms)
            {
                const ucb_uc_result result = fn(source->c_str(), source->size(), form, &err);
                if (!UCB_IS_THROWN(err) && result.data != nullptr)
                {
                    sink += result.size;
                    ucb_free(result.data);
                }
                ucb_error_clear(&err);
            }
        }
    }
    g_norm_sink = g_norm_sink + sink;
}

void normalize_quick()
{
    normalize_corpus(ucb_uc_normalize);
}

void normalize_without_quickpath()
{
    normalize_corpus(ucb_uc_normalize_full);
}

} // namespace

void register_unicode_benchmarks(ucb::microbench::benchmark& bench)
{
    ucb::microbench::suite& suite = bench.add_suite(
        "unicode",
        "Unicode normalization over the official UCD NormalizationTest corpus. A call normalizes "
        "the whole corpus, so compare per-corpus time; characters per second is not meaningful.");

    ucb::microbench::test& quick =
        suite.add_test("normalization",
                       "Convert every corpus entry from every source form into NFD, NFC, NFKD and "
                       "NFKC using the quick check fast path",
                       normalize_quick);
    suite.add_test("normalization-without-quickpath",
                   "Same conversions with the quick check disabled, always running the full "
                   "decomposition/composition algorithm",
                   normalize_without_quickpath);

    quick.add_ref("normalization-without-quickpath", ucb::microbench::expectation::faster);
}