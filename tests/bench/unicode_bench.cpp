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
#include "ucd_corpus.h"

#include "ucb/error.h"
#include "ucb/memory.h"
#include "ucb/unicode.h"
#include "ucb/unicode_private.h"

#include <cstddef>
#include <cstdint>
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

/// Load and decode the official NormalizationTest corpus once.
const std::vector<ucd_norm_case>& corpus()
{
    static const std::vector<ucd_norm_case> tests = [] {
        std::vector<ucd_norm_case> loaded;
        ucd_load_normalization_tests(NORM_TEST_FILE, loaded);
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
    for (const ucd_norm_case& test : corpus())
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