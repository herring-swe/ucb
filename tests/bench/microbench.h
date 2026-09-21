/**
 * @file bench.h
 * This file is part of the UCB project
 * - SPDX-FileCopyrightText: © 2026 Åke Svedin <ake@svedin.org>
 * - SPDX-License-Identifier: MIT
 *
 * @brief Minimal internal C++ benchmark library.
 *
 * A `benchmark` owns named suites, a suite owns named tests, and a test owns
 * references. A reference either points at another test by name (bare within
 * the same suite, or `suite::test` across suites) or wraps an inline function
 * with a label. Every reference carries its own expectation. Tests are warmed
 * up and sampled per call (nanoseconds plus a human readable unit) and reported
 * as calls per second. See `benchmark::run` for the CLI.
 */

#ifndef UCB_TEST_MICROBENCH_H
#define UCB_TEST_MICROBENCH_H

#include <cstddef>
#include <functional>
#include <iosfwd>
#include <memory>
#include <string>
#include <vector>

namespace ucb {
namespace microbench {

/// Any callable invocable with no arguments, e.g. a pure C `void(void)` pointer.
using function = std::function<void()>;

/// Comparison assumed between a test and one of its references.
enum class expectation
{
    none,
    faster,
    slower,
};

struct test;

/// A comparison point attached to a test. Not a test on its own.
struct reference
{
    enum class kind
    {
        named,
        function_ref,
    };

    kind type = kind::named;
    /// Authored name: bare/qualified test name, or label for a function reference.
    std::string name;
    /// Inline callable, only for kind::function_ref.
    function fn;
    expectation expected = expectation::none;

    /// Resolved at run start, only for kind::named.
    const test* target = nullptr;
    /// Resolved display name (bare for same-suite targets, qualified otherwise).
    std::string display;
    /// Measurement, only for kind::function_ref.
    double mean_ns = 0.0;
    double median_ns = 0.0;
    double standard_deviation_ns = 0.0;
};

/// A named test with an ordered list of references.
struct test
{
    std::string name;
    std::string description;
    function fn;
    std::vector<reference> refs;

    bool measured = false;
    double mean_ns = 0.0;
    double median_ns = 0.0;
    double standard_deviation_ns = 0.0;

    /// Reference an existing test by bare `name` (same suite) or `suite::name`.
    test& add_ref(std::string name, expectation expected = expectation::none);
    /// Reference an inline callable; `label` is required for presentation.
    test& add_ref(function fn, std::string label, expectation expected = expectation::none);
};

/// A named suite with an insertion-ordered list of tests.
struct suite
{
    std::string name;
    std::string description;
    std::vector<std::unique_ptr<test>> tests;

    test& add_test(std::string name, std::string description, function fn);
};

/// Top-level registry. Create one, pass it to registration functions, then run.
class benchmark
{
public:
    suite& add_suite(std::string name, std::string description);

    /// Parse @p argv, run/present the selected tests, and return an exit code:
    /// 0 on success, 1 if any presented test has a FAIL verdict, 2 on usage or
    /// validation error. `out` receives reports, `err` receives errors/progress.
    int run(int argc, char** argv, std::ostream& out, std::ostream& err);

private:
    std::vector<std::unique_ptr<suite>> suites_;
};

} // namespace microbench
} // namespace ucb

#endif // UCB_TEST_MICROBENCH_H
