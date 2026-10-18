/**
 * @file unicode_bench.cpp
 * This file is part of the UCB project
 * - SPDX-FileCopyrightText: © 2026 Åke Svedin <ake@svedin.org>
 * - SPDX-License-Identifier: MIT
 *
 * @brief Benchmarks for unicode normalization with and without the quick check
 * fast path, over the official UCD NormalizationTest corpus. When
 * UCB_BENCH_COMPARE is enabled the same workload is also run through utf8proc
 * and ICU for comparison.
 */

#include "unicode_bench.h"

#include "microbench.h"
#include "ucd_corpus.h"

#include "ucb/error.h"
#include "ucb/memory.h"
#include "ucb/unicode.h"
#include "ucb/unicode_private.h"

#ifdef UCB_BENCH_UTF8PROC
#include <utf8proc.h>
#endif

#ifdef UCB_BENCH_ICU
#if defined(_WIN32)
// The in-box Windows ICU only ships the merged C header.
#include <icu.h>
#else
#include <unicode/unorm2.h>
#include <unicode/ustring.h>
#endif
#endif

#include <array>
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

/// Run every corpus entry from every source form through all four
/// normalization forms, mirroring the official normalization test. `convert`
/// performs one conversion and returns the number of result bytes, which is
/// folded into a shared sink so the work cannot be optimized away.
template <typename Convert>
void normalize_corpus_with(Convert convert)
{
    static const ucb_norm_form forms[4] = {UCB_NORM_NFD,
                                           UCB_NORM_NFC,
                                           UCB_NORM_NFKD,
                                           UCB_NORM_NFKC};

    std::uint64_t sink = 0;
    for (const ucd_norm_case& test : corpus())
    {
        const std::string* sources[5] = {&test.input, &test.nfc, &test.nfd, &test.nfkc, &test.nfkd};
        for (const std::string* source : sources)
        {
            for (const ucb_norm_form form : forms)
                sink += static_cast<std::uint64_t>(convert(*source, form));
        }
    }
    g_norm_sink = g_norm_sink + sink;
}

using norm_func = ucb_uc_result (*)(const char*, size_t, ucb_norm_form, ucb_error**);

void normalize_corpus(norm_func fn)
{
    normalize_corpus_with([fn](const std::string& source, ucb_norm_form form) -> std::size_t {
        ucb_error* err = nullptr;
        const ucb_uc_result result = fn(source.c_str(), source.size(), form, &err);
        std::size_t size = 0;
        if (!UCB_IS_THROWN(err) && result.data != nullptr)
        {
            size = result.size;
            ucb_free(result.data);
        }
        ucb_error_clear(&err);
        return size;
    });
}

void normalize_quick()
{
    normalize_corpus(ucb_uc_normalize);
}

void normalize_without_quickpath()
{
    normalize_corpus(ucb_uc_normalize_full);
}

#ifdef UCB_BENCH_UTF8PROC
/// Map a UCB normalization form to the equivalent utf8proc option flags.
unsigned int utf8proc_options(ucb_norm_form form)
{
    switch (form)
    {
    case UCB_NORM_NFD:
        return UTF8PROC_STABLE | UTF8PROC_DECOMPOSE;
    case UCB_NORM_NFC:
        return UTF8PROC_STABLE | UTF8PROC_COMPOSE;
    case UCB_NORM_NFKD:
        return UTF8PROC_STABLE | UTF8PROC_DECOMPOSE | UTF8PROC_COMPAT;
    case UCB_NORM_NFKC:
        return UTF8PROC_STABLE | UTF8PROC_COMPOSE | UTF8PROC_COMPAT;
    default:
        return UTF8PROC_STABLE;
    }
}

void normalize_corpus_utf8proc()
{
    normalize_corpus_with([](const std::string& source, ucb_norm_form form) -> std::size_t {
        utf8proc_uint8_t* dest = nullptr;
        const utf8proc_ssize_t result =
            utf8proc_map(reinterpret_cast<const utf8proc_uint8_t*>(source.data()),
                         static_cast<utf8proc_ssize_t>(source.size()),
                         &dest,
                         utf8proc_options(form));
        std::size_t size = 0;
        if (result >= 0)
            size = static_cast<std::size_t>(result);
        utf8proc_free(dest);
        return size;
    });
}
#endif // UCB_BENCH_UTF8PROC

#ifdef UCB_BENCH_ICU
/// Resolve (and cache) the ICU normalizer for a UCB normalization form.
const UNormalizer2* icu_normalizer_for(ucb_norm_form form)
{
    static const std::array<const UNormalizer2*, 4> instances = [] {
        UErrorCode status = U_ZERO_ERROR;
        return std::array<const UNormalizer2*, 4>{
            unorm2_getNFDInstance(&status),
            unorm2_getNFCInstance(&status),
            unorm2_getNFKDInstance(&status),
            unorm2_getNFKCInstance(&status),
        };
    }();

    switch (form)
    {
    case UCB_NORM_NFD:
        return instances[0];
    case UCB_NORM_NFC:
        return instances[1];
    case UCB_NORM_NFKD:
        return instances[2];
    case UCB_NORM_NFKC:
        return instances[3];
    default:
        return nullptr;
    }
}

/// Convert UTF-8 to UTF-16 using the ICU size-first pattern: the first call
/// only asks for the required length. Leaves a NUL terminator in `dest`.
bool icu_utf8_to_utf16(const std::string& source, int32_t& utf16_len, std::vector<UChar>& dest)
{
    const int32_t src_len = static_cast<int32_t>(source.size());
    UErrorCode status = U_ZERO_ERROR;
    u_strFromUTF8(nullptr, 0, &utf16_len, source.data(), src_len, &status);
    if (utf16_len < 0)
        return false;

    dest.resize(static_cast<std::size_t>(utf16_len) + 1);
    status = U_ZERO_ERROR;
    u_strFromUTF8(dest.data(), utf16_len + 1, &utf16_len, source.data(), src_len, &status);
    return U_SUCCESS(status);
}

void normalize_corpus_icu()
{
    normalize_corpus_with([](const std::string& source, ucb_norm_form form) -> std::size_t {
        const UNormalizer2* const normalizer = icu_normalizer_for(form);
        if (normalizer == nullptr)
            return 0;

        std::vector<UChar> utf16;
        int32_t utf16_len = 0;
        if (!icu_utf8_to_utf16(source, utf16_len, utf16))
            return 0;

        UErrorCode status = U_ZERO_ERROR;
        const int32_t norm_len = unorm2_normalize(
            normalizer, utf16.data(), utf16_len, nullptr, 0, &status);
        if (norm_len < 0)
            return 0;

        std::vector<UChar> normalized(static_cast<std::size_t>(norm_len) + 1);
        status = U_ZERO_ERROR;
        unorm2_normalize(normalizer,
                         utf16.data(),
                         utf16_len,
                         normalized.data(),
                         norm_len + 1,
                         &status);
        if (U_FAILURE(status))
            return 0;

        int32_t utf8_len = 0;
        status = U_ZERO_ERROR;
        u_strToUTF8(nullptr, 0, &utf8_len, normalized.data(), norm_len, &status);
        return utf8_len > 0 ? static_cast<std::size_t>(utf8_len) : 0;
    });
}
#endif // UCB_BENCH_ICU

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

#ifdef UCB_BENCH_UTF8PROC
    suite.add_test("normalization-utf8proc",
                   "Same conversions using utf8proc (utf8proc_map over the whole corpus)",
                   normalize_corpus_utf8proc);
    quick.add_ref("normalization-utf8proc", ucb::microbench::expectation::none);
#endif

#ifdef UCB_BENCH_ICU
    suite.add_test("normalization-icu",
                   "Same conversions using ICU (UTF-8 to UTF-16, normalize, back to UTF-8)",
                   normalize_corpus_icu);
    quick.add_ref("normalization-icu", ucb::microbench::expectation::none);
#endif
}
