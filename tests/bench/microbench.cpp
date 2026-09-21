/**
 * @file microbench.cpp
 * This file is part of the UCB project
 * - SPDX-FileCopyrightText: © 2026 Åke Svedin <ake@svedin.org>
 * - SPDX-License-Identifier: MIT
 *
 * @brief Minimal internal C++ benchmark library.
 */

#include "microbench.h"

#include <unordered_map>
#include <unordered_set>

#include <cmath>

#include <algorithm>
#include <chrono>
#include <cstdio>
#include <iomanip>
#include <numeric>
#include <ostream>
#include <sstream>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

#if defined(_WIN32)
#include <io.h>
#define NOMINMAX
#include <Windows.h>
#else
#include <unistd.h>
#endif

namespace ucb {
namespace microbench {
namespace {

using clock_type = std::chrono::steady_clock;

constexpr std::size_t max_batch = static_cast<std::size_t>(1) << 20;
constexpr double sample_target_ns = 1000.0; // Enough to be meaningful for a clock tick.
constexpr std::size_t max_id_length = 64;
constexpr std::size_t max_description_length = 1024;

/* -------------------------------------------------------------------------- */
/*                                 Validation                                 */
/* -------------------------------------------------------------------------- */

bool is_valid_id(const std::string& value)
{
    if (value.empty() || value.size() > max_id_length)
        return false;
    if (value[0] < 'a' || value[0] > 'z')
        return false;

    for (std::size_t index = 1; index < value.size(); ++index)
    {
        const char c = value[index];
        const bool allowed = (c >= 'a' && c <= 'z') || (c >= '0' && c <= '9') || c == '.' ||
                             c == '_' || c == '+' || c == '-';
        if (!allowed)
            return false;
    }
    return true;
}

bool split_qualified(const std::string& value, std::string& suite_part, std::string& test_part)
{
    const std::size_t pos = value.find("::");
    if (pos == std::string::npos)
        return false;
    suite_part = value.substr(0, pos);
    test_part = value.substr(pos + 2);
    return true;
}

bool is_valid_reference_name(const std::string& value)
{
    const std::size_t pos = value.find("::");
    if (pos == std::string::npos)
        return is_valid_id(value);
    if (value.find("::", pos + 2) != std::string::npos)
        return false;
    return is_valid_id(value.substr(0, pos)) && is_valid_id(value.substr(pos + 2));
}

void validate_description(const std::string& value)
{
    if (value.size() > max_description_length)
        throw std::invalid_argument("description must not exceed 1024 characters");
}

/* -------------------------------------------------------------------------- */
/*                                 Measurement                                */
/* -------------------------------------------------------------------------- */

double elapsed_ns(clock_type::time_point start, clock_type::time_point end)
{
    return std::chrono::duration<double, std::nano>(end - start).count();
}

/// Run `test` once per batch element and return the per-call time in nanoseconds.
double measure_batch(const function& test, std::size_t batch)
{
    const clock_type::time_point start = clock_type::now();
    for (std::size_t call = 0; call < batch; ++call)
        test();
    const clock_type::time_point end = clock_type::now();
    return elapsed_ns(start, end) / static_cast<double>(batch);
}

/// Short callables can be below the clock resolution; grow the batch until one
/// sample is measurable, then report per-call time.
std::size_t calibrate(const function& test)
{
    std::size_t batch = 1;
    while (batch < max_batch)
    {
        if (measure_batch(test, batch) * static_cast<double>(batch) >= sample_target_ns)
            break;
        batch *= 2;
    }
    return batch;
}

/// Warm up a callable without recording the result, for at most `warmup_ms`.
void warmup(const function& test, double warmup_ms)
{
    if (warmup_ms <= 0.0)
        return;

    const double budget_ns = warmup_ms * 1000000.0;
    double elapsed = 0.0;
    std::size_t batch = 1;
    while (elapsed < budget_ns && batch < max_batch)
    {
        const clock_type::time_point start = clock_type::now();
        for (std::size_t call = 0; call < batch; ++call)
            test();
        const clock_type::time_point end = clock_type::now();
        elapsed += elapsed_ns(start, end);
        if (elapsed < budget_ns)
            batch *= 2;
    }
}

struct sample_stats
{
    double mean = 0.0;
    double median = 0.0;
    double standard_deviation = 0.0;
};

/// `trim_fraction` discards that fraction of samples from each tail before
/// computing the mean and standard deviation, so rare scheduler or allocator
/// spikes do not dominate the result. The median always uses all samples.
sample_stats summarize(const std::vector<double>& values, double trim_fraction)
{
    sample_stats stats;
    if (values.empty())
        return stats;

    std::vector<double> sorted = values;
    std::sort(sorted.begin(), sorted.end());

    const std::size_t middle = sorted.size() / 2;
    if (sorted.size() % 2 != 0)
        stats.median = sorted[middle];
    else
        stats.median = (sorted[middle - 1] + sorted[middle]) / 2.0;

    std::size_t trim = static_cast<std::size_t>(static_cast<double>(sorted.size()) * trim_fraction);
    if (trim * 2 >= sorted.size())
        trim = 0;

    const std::size_t first = trim;
    const std::size_t last = sorted.size() - trim;
    const std::size_t count = last - first;
    const double count_value = static_cast<double>(count);

    double sum = 0.0;
    for (std::size_t index = first; index < last; ++index)
        sum += sorted[index];
    stats.mean = sum / count_value;

    double variance = 0.0;
    for (std::size_t index = first; index < last; ++index)
        variance += (sorted[index] - stats.mean) * (sorted[index] - stats.mean);
    variance /= count > 1 ? static_cast<double>(count - 1) : 1.0;
    stats.standard_deviation = std::sqrt(variance);
    return stats;
}

sample_stats measure(const function& test,
                     std::size_t samples,
                     double warmup_ms,
                     double trim_fraction)
{
    warmup(test, warmup_ms);
    const std::size_t batch = calibrate(test);

    std::vector<double> values;
    values.reserve(samples);
    for (std::size_t index = 0; index < samples; ++index)
        values.push_back(measure_batch(test, batch));

    return summarize(values, trim_fraction);
}

/* -------------------------------------------------------------------------- */
/*                                  Formatting                                */
/* -------------------------------------------------------------------------- */

std::string format_value(double value, const char* unit)
{
    int precision = 3;
    if (value >= 100.0)
        precision = 1;
    else if (value >= 10.0)
        precision = 2;

    std::ostringstream output;
    output << std::fixed << std::setprecision(precision) << value << ' ' << unit;
    return output.str();
}

/// Pick the most readable unit while keeping the value meaningful.
std::string format_time(double nanoseconds)
{
    if (nanoseconds >= 1000000000.0)
        return format_value(nanoseconds / 1000000000.0, "s");
    if (nanoseconds >= 1000000.0)
        return format_value(nanoseconds / 1000000.0, "ms");
    if (nanoseconds >= 1000.0)
        return format_value(nanoseconds / 1000.0, "us");
    return format_value(nanoseconds, "ns");
}

std::string format_rate(double calls_per_second)
{
    if (calls_per_second >= 1000000000.0)
        return format_value(calls_per_second / 1000000000.0, "G/s");
    if (calls_per_second >= 1000000.0)
        return format_value(calls_per_second / 1000000.0, "M/s");
    if (calls_per_second >= 1000.0)
        return format_value(calls_per_second / 1000.0, "k/s");
    return format_value(calls_per_second, "/s");
}

bool meets_expectation(expectation expected, double value, double reference)
{
    if (expected == expectation::faster)
        return value < reference;
    if (expected == expectation::slower)
        return value > reference;
    return true;
}

std::string join_qualified(const std::string& suite_name, const std::string& test_name)
{
    return suite_name + "::" + test_name;
}

bool stderr_is_tty()
{
#if defined(_WIN32)
    return _isatty(_fileno(stderr)) != 0;
#else
    return isatty(fileno(stderr)) != 0;
#endif
}

bool stdout_is_tty()
{
#if defined(_WIN32)
    return _isatty(_fileno(stdout)) != 0;
#else
    return isatty(fileno(stdout)) != 0;
#endif
}

/// Enable ANSI escape sequences on legacy Windows consoles.
void enable_virtual_terminal()
{
#if defined(_WIN32)
    const HANDLE handle = GetStdHandle(STD_OUTPUT_HANDLE);
    if (handle == nullptr || handle == INVALID_HANDLE_VALUE)
        return;
    DWORD mode = 0;
    if (!GetConsoleMode(handle, &mode))
        return;
    SetConsoleMode(handle, mode | ENABLE_VIRTUAL_TERMINAL_PROCESSING);
#endif
}

enum class color_mode
{
    automatic,
    always,
    never,
};

struct colors
{
    const char* pass = "";
    const char* fail = "";
    const char* reference = "";
    const char* reset = "";
};

colors make_colors(bool enabled)
{
    colors palette;
    if (!enabled)
        return palette;
    palette.pass = "\x1b[32m";
    palette.fail = "\x1b[31m";
    palette.reference = "\x1b[90m";
    palette.reset = "\x1b[0m";
    return palette;
}

const char* status_color(const colors& palette, const std::string& status)
{
    if (status == "[PASS]")
        return palette.pass;
    if (status == "[FAIL]")
        return palette.fail;
    return "";
}

/* -------------------------------------------------------------------------- */
/*                                    CLI                                     */
/* -------------------------------------------------------------------------- */

struct run_options
{
    bool list = false;
    bool quiet = false;
    bool verbose = false;
    bool progress = false;
    bool status_only = false;
    bool no_refs = false;
    bool help = false;
    color_mode color = color_mode::automatic;
    std::size_t samples = 500;
    double warmup_ms = 50.0;
    double trim_percent = 5.0;
    std::vector<std::string> suite_selectors;
    std::vector<std::string> test_selectors;
};

void print_usage(std::ostream& out)
{
    out << "Usage: ucb_microbenchmarks [options]\n"
        << "\n"
        << "  -l, --list             list selected tests and exit\n"
        << "  -q, --quiet            print only result rows\n"
        << "  -v, --verbose          show descriptions and reference rows\n"
        << "  -t, --test <sel>       select a test (repeatable); name | suite::name | :all\n"
        << "  -s, --suite <sel>      select a suite (repeatable); name | :all\n"
        << "  -p, --progress         show progress on stderr while running\n"
        << "  -S, --status           present only tests with a graded reference\n"
        << "  -n, --samples <count>  samples per test (default 500)\n"
        << "      --warmup <ms>      warmup budget per callable (default 50)\n"
        << "      --trim <percent>   discard this percent from each sample tail (default 5)\n"
        << "      --no-refs          disregard all references\n"
        << "      --color            force colored output\n"
        << "      --no-color         disable colored output\n"
        << "  -h, --help             show this help\n";
}

bool parse_size(const std::string& value, std::size_t& result)
{
    if (value.empty())
        return false;
    std::size_t parsed = 0;
    for (char c : value)
    {
        if (c < '0' || c > '9')
            return false;
        parsed = parsed * 10 + static_cast<std::size_t>(c - '0');
    }
    result = parsed;
    return true;
}

bool parse_double(const std::string& value, double& result)
{
    try
    {
        std::size_t consumed = 0;
        const double parsed = std::stod(value, &consumed);
        if (consumed != value.size() || !std::isfinite(parsed))
            return false;
        result = parsed;
        return true;
    }
    catch (const std::exception&)
    {
        return false;
    }
}

bool parse_options(int argc, char** argv, run_options& options, std::string& error)
{
    for (int index = 1; index < argc; ++index)
    {
        std::string argument = argv[index];
        std::string value;
        bool has_value = false;

        const std::size_t equals = argument.find('=');
        if (argument.rfind("--", 0) == 0 && equals != std::string::npos)
        {
            value = argument.substr(equals + 1);
            argument = argument.substr(0, equals);
            has_value = true;
        }

        auto take_value = [&](void) -> bool {
            if (has_value)
                return true;
            if (index + 1 >= argc)
                return false;
            value = argv[++index];
            return true;
        };

        if (argument == "-l" || argument == "--list")
            options.list = true;
        else if (argument == "-q" || argument == "--quiet")
            options.quiet = true;
        else if (argument == "-v" || argument == "--verbose")
            options.verbose = true;
        else if (argument == "-p" || argument == "--progress")
            options.progress = true;
        else if (argument == "-S" || argument == "--status")
            options.status_only = true;
        else if (argument == "--no-refs")
            options.no_refs = true;
        else if (argument == "--color")
            options.color = color_mode::always;
        else if (argument == "--no-color")
            options.color = color_mode::never;
        else if (argument == "-h" || argument == "--help")
            options.help = true;
        else if (argument == "-n" || argument == "--samples")
        {
            if (!take_value() || !parse_size(value, options.samples) || options.samples == 0)
            {
                error = "invalid sample count for " + argument;
                return false;
            }
        }
        else if (argument == "--warmup")
        {
            if (!take_value() || !parse_double(value, options.warmup_ms) || options.warmup_ms < 0.0)
            {
                error = "invalid warmup value for " + argument;
                return false;
            }
        }
        else if (argument == "--trim")
        {
            if (!take_value() || !parse_double(value, options.trim_percent) ||
                options.trim_percent < 0.0 || options.trim_percent >= 50.0)
            {
                error = "invalid trim percent for " + argument;
                return false;
            }
        }
        else if (argument == "-t" || argument == "--test")
        {
            if (!take_value())
            {
                error = "missing value for " + argument;
                return false;
            }
            options.test_selectors.push_back(value);
        }
        else if (argument == "-s" || argument == "--suite")
        {
            if (!take_value())
            {
                error = "missing value for " + argument;
                return false;
            }
            options.suite_selectors.push_back(value);
        }
        else if (!argument.empty() && argument[0] == '-')
        {
            error = "unknown option: " + argument;
            return false;
        }
        else
        {
            error = "unexpected argument: " + argument;
            return false;
        }
    }
    return true;
}

/* -------------------------------------------------------------------------- */
/*                                 Reporting                                  */
/* -------------------------------------------------------------------------- */

struct entry
{
    suite* suite_ptr = nullptr;
    test* test_ptr = nullptr;
};

struct output_row
{
    std::string name;
    std::string mean;
    std::string median;
    std::string standard_deviation;
    std::string rate;
    std::string status;
    std::string description;
    bool is_test = false;
    bool is_reference = false;
};

struct suite_section
{
    const suite* suite_ptr = nullptr;
    std::vector<output_row> rows;
    std::vector<double> test_means;
    sample_stats summary;
    std::string accumulated;
};

std::vector<std::string> split_lines(const std::string& text)
{
    std::vector<std::string> lines;
    std::size_t start = 0;
    for (;;)
    {
        const std::size_t newline = text.find('\n', start);
        if (newline == std::string::npos)
        {
            lines.push_back(text.substr(start));
            break;
        }
        lines.push_back(text.substr(start, newline - start));
        start = newline + 1;
    }
    return lines;
}

} // namespace

/* -------------------------------------------------------------------------- */
/*                                  Model                                     */
/* -------------------------------------------------------------------------- */

test& test::add_ref(std::string reference_name, expectation expected)
{
    if (!is_valid_reference_name(reference_name))
        throw std::invalid_argument("invalid reference name: " + reference_name);

    reference ref;
    ref.type = reference::kind::named;
    ref.name = std::move(reference_name);
    ref.expected = expected;
    refs.push_back(std::move(ref));
    return *this;
}

test& test::add_ref(function reference_function, std::string label, expectation expected)
{
    if (!reference_function)
        throw std::invalid_argument("reference function must not be empty");
    if (!is_valid_id(label))
        throw std::invalid_argument("invalid reference label: " + label);

    reference ref;
    ref.type = reference::kind::function_ref;
    ref.name = std::move(label);
    ref.fn = std::move(reference_function);
    ref.expected = expected;
    refs.push_back(std::move(ref));
    return *this;
}

test& suite::add_test(std::string test_name, std::string test_description, function test_function)
{
    if (!is_valid_id(test_name))
        throw std::invalid_argument("invalid test name: " + test_name);
    validate_description(test_description);
    if (!test_function)
        throw std::invalid_argument("test function must not be empty");

    for (const std::unique_ptr<test>& existing : tests)
    {
        if (existing->name == test_name)
            throw std::invalid_argument("duplicate test name in suite: " + test_name);
    }

    tests.push_back(std::make_unique<test>());
    test& created = *tests.back();
    created.name = std::move(test_name);
    created.description = std::move(test_description);
    created.fn = std::move(test_function);
    return created;
}

suite& benchmark::add_suite(std::string suite_name, std::string suite_description)
{
    if (!is_valid_id(suite_name))
        throw std::invalid_argument("invalid suite name: " + suite_name);
    validate_description(suite_description);

    for (const std::unique_ptr<suite>& existing : suites_)
    {
        if (existing->name == suite_name)
            throw std::invalid_argument("duplicate suite name: " + suite_name);
    }

    suites_.push_back(std::make_unique<suite>());
    suite& created = *suites_.back();
    created.name = std::move(suite_name);
    created.description = std::move(suite_description);
    return created;
}

/* -------------------------------------------------------------------------- */
/*                                   Run                                      */
/* -------------------------------------------------------------------------- */

int benchmark::run(int argc, char** argv, std::ostream& out, std::ostream& err)
{
    run_options options;
    std::string parse_error;
    if (!parse_options(argc, argv, options, parse_error))
    {
        err << "error: " << parse_error << '\n';
        print_usage(err);
        return 2;
    }
    if (options.help)
    {
        print_usage(out);
        return 0;
    }

    // Flatten the model in insertion order.
    std::vector<entry> all;
    std::unordered_map<std::string, std::size_t> by_qualified;
    for (const std::unique_ptr<suite>& suite_ptr : suites_)
    {
        for (const std::unique_ptr<test>& test_ptr : suite_ptr->tests)
        {
            const std::size_t index = all.size();
            all.push_back({suite_ptr.get(), test_ptr.get()});
            by_qualified.emplace(join_qualified(suite_ptr->name, test_ptr->name), index);
        }
    }

    // Selection: union of all selectors; no selectors means everything.
    const bool any_selector = !options.suite_selectors.empty() || !options.test_selectors.empty();
    std::vector<char> selector_hit(options.suite_selectors.size() + options.test_selectors.size(),
                                   0);
    std::vector<char> selected(all.size(), 0);
    for (std::size_t index = 0; index < all.size(); ++index)
    {
        bool is_selected = !any_selector;
        for (std::size_t k = 0; k < options.suite_selectors.size(); ++k)
        {
            const std::string& selector = options.suite_selectors[k];
            if (selector == ":all" || selector == all[index].suite_ptr->name)
            {
                is_selected = true;
                selector_hit[k] = 1;
            }
        }
        for (std::size_t k = 0; k < options.test_selectors.size(); ++k)
        {
            const std::string& selector = options.test_selectors[k];
            const std::size_t hit = options.suite_selectors.size() + k;
            if (selector == ":all")
            {
                is_selected = true;
                selector_hit[hit] = 1;
                continue;
            }

            std::string suite_part;
            std::string test_part;
            if (split_qualified(selector, suite_part, test_part))
            {
                if (all[index].suite_ptr->name == suite_part &&
                    all[index].test_ptr->name == test_part)
                {
                    is_selected = true;
                    selector_hit[hit] = 1;
                }
            }
            else if (all[index].test_ptr->name == selector)
            {
                is_selected = true;
                selector_hit[hit] = 1;
            }
        }
        selected[index] = is_selected ? 1 : 0;
    }

    for (std::size_t k = 0; k < selector_hit.size(); ++k)
    {
        if (selector_hit[k])
            continue;
        const std::string& bad = k < options.suite_selectors.size()
                                     ? options.suite_selectors[k]
                                     : options.test_selectors[k - options.suite_selectors.size()];
        err << "error: selector matched nothing: " << bad << '\n';
        return 2;
    }

    if (options.list)
    {
        const suite* current = nullptr;
        for (std::size_t index = 0; index < all.size(); ++index)
        {
            if (!selected[index])
                continue;
            if (all[index].suite_ptr != current)
            {
                current = all[index].suite_ptr;
                out << current->name << '\n';
                if (options.verbose && !current->description.empty())
                    out << "  " << current->description << '\n';
            }
            out << "  " << all[index].test_ptr->name << '\n';
            if (options.verbose && !all[index].test_ptr->description.empty())
                out << "    " << all[index].test_ptr->description << '\n';
        }
        return 0;
    }

    // Status filter narrows the presented set only.
    std::vector<char> present = selected;
    if (options.status_only)
    {
        for (std::size_t index = 0; index < all.size(); ++index)
        {
            if (!present[index])
                continue;
            bool graded = false;
            for (const reference& ref : all[index].test_ptr->refs)
            {
                if (ref.expected != expectation::none)
                {
                    graded = true;
                    break;
                }
            }
            if (!graded)
                present[index] = 0;
        }
    }

    // Resolve references of presented tests and build the run set (one level).
    std::vector<char> in_run = present;
    if (!options.no_refs)
    {
        for (std::size_t index = 0; index < all.size(); ++index)
        {
            if (!present[index])
                continue;

            test* current = all[index].test_ptr;
            suite* owner = all[index].suite_ptr;
            std::unordered_set<std::string> seen;

            for (reference& ref : current->refs)
            {
                if (ref.type == reference::kind::function_ref)
                {
                    if (!seen.insert(ref.name).second)
                    {
                        err << "error: duplicate reference on " << owner->name
                            << "::" << current->name << ": " << ref.name << '\n';
                        return 2;
                    }
                    ref.display = ref.name;
                    continue;
                }

                std::string suite_part;
                std::string test_part;
                const std::string qualified = split_qualified(ref.name, suite_part, test_part)
                                                  ? ref.name
                                                  : join_qualified(owner->name, ref.name);

                const auto found = by_qualified.find(qualified);
                if (found == by_qualified.end())
                {
                    err << "error: unknown reference " << qualified << " on " << owner->name
                        << "::" << current->name << '\n';
                    return 2;
                }
                if (!seen.insert(qualified).second)
                {
                    err << "error: duplicate reference on " << owner->name << "::" << current->name
                        << ": " << qualified << '\n';
                    return 2;
                }

                entry& target = all[found->second];
                if (target.test_ptr == current)
                {
                    err << "error: self reference " << qualified << " on " << owner->name
                        << "::" << current->name << '\n';
                    return 2;
                }

                ref.target = target.test_ptr;
                ref.display = target.suite_ptr == owner ? target.test_ptr->name : qualified;
                in_run[found->second] = 1;
            }
        }
    }

    // Banner before running, unless quiet.
    if (!options.quiet)
    {
        std::size_t test_count = 0;
        for (char value : present)
            test_count += value ? 1 : 0;

        std::size_t suite_count = 0;
        for (const std::unique_ptr<suite>& suite_ptr : suites_)
        {
            bool has_present = false;
            for (const std::unique_ptr<test>& test_ptr : suite_ptr->tests)
            {
                for (std::size_t index = 0; index < all.size(); ++index)
                {
                    if (all[index].test_ptr == test_ptr.get() && present[index])
                    {
                        has_present = true;
                        break;
                    }
                }
                if (has_present)
                    break;
            }
            if (has_present)
                ++suite_count;
        }

        out << "Running " << test_count << " tests in " << suite_count
            << " suites (samples=" << options.samples << ", trim=" << options.trim_percent
            << "%)\n\n";
    }

    // Measure every test in the run set once, in insertion order.
    const bool tty = stderr_is_tty();
    std::size_t progress_width = 0;
    std::size_t completed = 0;
    std::size_t total = 0;
    for (char value : in_run)
        total += value ? 1 : 0;

    for (std::size_t index = 0; index < all.size(); ++index)
    {
        if (!in_run[index])
            continue;

        test* current = all[index].test_ptr;
        const sample_stats stats =
            measure(current->fn, options.samples, options.warmup_ms, options.trim_percent / 100.0);
        current->mean_ns = stats.mean;
        current->median_ns = stats.median;
        current->standard_deviation_ns = stats.standard_deviation;
        current->measured = true;

        if (present[index] && !options.no_refs)
        {
            for (reference& ref : current->refs)
            {
                if (ref.type != reference::kind::function_ref)
                    continue;
                const sample_stats ref_stats = measure(ref.fn,
                                                       options.samples,
                                                       options.warmup_ms,
                                                       options.trim_percent / 100.0);
                ref.mean_ns = ref_stats.mean;
                ref.median_ns = ref_stats.median;
                ref.standard_deviation_ns = ref_stats.standard_deviation;
            }
        }

        ++completed;
        if (options.progress && !options.quiet)
        {
            const std::string label = join_qualified(all[index].suite_ptr->name, current->name);
            std::ostringstream line;
            line << '[' << completed << '/' << total << "] " << label;
            const std::string text = line.str();
            if (tty)
            {
                err << '\r' << text;
                if (text.size() < progress_width)
                    err << std::string(progress_width - text.size(), ' ');
                progress_width = text.size();
                err.flush();
            }
            else
            {
                err << text << '\n';
            }
        }
    }
    if (options.progress && !options.quiet && tty && progress_width > 0)
        err << '\n';

    // Build one output section per suite in insertion order.
    struct ref_view
    {
        const reference* ref = nullptr;
        double mean = 0.0;
        double median = 0.0;
        double standard_deviation = 0.0;
        bool graded = false;
        bool ok = true;
        char sign = '*';
    };

    std::vector<suite_section> sections;
    bool any_fail = false;

    for (const std::unique_ptr<suite>& suite_ptr : suites_)
    {
        suite_section section;
        section.suite_ptr = suite_ptr.get();

        for (std::size_t index = 0; index < all.size(); ++index)
        {
            if (all[index].suite_ptr != section.suite_ptr || !present[index])
                continue;

            test* current = all[index].test_ptr;

            std::vector<ref_view> views;
            bool graded = false;
            bool passed = true;
            if (!options.no_refs)
            {
                for (const reference& ref : current->refs)
                {
                    ref_view view;
                    view.ref = &ref;
                    if (ref.type == reference::kind::named)
                    {
                        view.mean = ref.target->mean_ns;
                        view.median = ref.target->median_ns;
                        view.standard_deviation = ref.target->standard_deviation_ns;
                    }
                    else
                    {
                        view.mean = ref.mean_ns;
                        view.median = ref.median_ns;
                        view.standard_deviation = ref.standard_deviation_ns;
                    }

                    view.graded = ref.expected != expectation::none;
                    if (view.graded)
                    {
                        // '*' marks no comparison; otherwise the sign is the
                        // observed relation of the test to the reference.
                        view.sign = current->mean_ns < view.mean
                                        ? '<'
                                        : (current->mean_ns > view.mean ? '>' : '=');
                        view.ok = meets_expectation(ref.expected, current->mean_ns, view.mean);
                        graded = true;
                        if (!view.ok)
                            passed = false;
                    }
                    views.push_back(view);
                }
            }
            if (graded && !passed)
                any_fail = true;

            output_row test_row;
            test_row.name = options.quiet ? join_qualified(section.suite_ptr->name, current->name)
                                          : current->name;
            test_row.mean = format_time(current->mean_ns);
            test_row.median = format_time(current->median_ns);
            test_row.standard_deviation = format_time(current->standard_deviation_ns);
            test_row.rate = format_rate(1000000000.0 / current->mean_ns);
            test_row.description = current->description;
            test_row.is_test = true;
            // Reference rows are only shown with --verbose, so otherwise the
            // aggregate verdict is reflected on the test row itself.
            if (!options.verbose && graded)
                test_row.status = passed ? "[PASS]" : "[FAIL]";
            section.rows.push_back(std::move(test_row));
            section.test_means.push_back(current->mean_ns);

            if (options.verbose && !options.no_refs)
            {
                for (const ref_view& view : views)
                {
                    output_row ref_row;
                    ref_row.name = std::string("  ") + view.sign + ' ' + view.ref->display;
                    ref_row.mean = format_time(view.mean);
                    ref_row.median = format_time(view.median);
                    ref_row.standard_deviation = format_time(view.standard_deviation);
                    ref_row.rate = format_rate(1000000000.0 / view.mean);
                    ref_row.is_reference = true;
                    if (view.graded)
                        ref_row.status = view.ok ? "[PASS]" : "[FAIL]";
                    section.rows.push_back(std::move(ref_row));
                }
            }
        }

        if (section.rows.empty())
            continue;

        section.summary = summarize(section.test_means, 0.0);
        section.accumulated =
            format_time(section.summary.mean * static_cast<double>(section.test_means.size()));
        sections.push_back(std::move(section));
    }

    // One set of column widths for every row and the suite summary so all fields
    // stay aligned across suites.
    std::size_t name_width = std::max<std::size_t>(4, std::string("suite:").size());
    std::size_t mean_width = 4;
    std::size_t median_width = 6;
    std::size_t stddev_width = 7;
    std::size_t rate_width = 7;
    std::size_t status_width = 6;
    for (const suite_section& section : sections)
    {
        mean_width = std::max(mean_width, section.accumulated.size());
        for (const output_row& row : section.rows)
        {
            name_width = std::max(name_width, row.name.size());
            mean_width = std::max(mean_width, row.mean.size());
            median_width = std::max(median_width, row.median.size());
            stddev_width = std::max(stddev_width, row.standard_deviation.size());
            rate_width = std::max(rate_width, row.rate.size());
            status_width = std::max(status_width, row.status.size());
        }
    }

    std::ostringstream header_stream;
    header_stream << std::left << std::setw(static_cast<int>(name_width)) << "name" << "  "
                  << std::right << std::setw(static_cast<int>(mean_width)) << "mean" << "  "
                  << std::setw(static_cast<int>(median_width)) << "median" << "  "
                  << std::setw(static_cast<int>(stddev_width)) << "std dev" << "  "
                  << std::setw(static_cast<int>(rate_width)) << "calls/s" << "  status";
    const std::string header = header_stream.str();
    const std::string rule(header.size(), '-');

    bool use_color = options.color == color_mode::always;
    if (options.color == color_mode::automatic)
        use_color = stdout_is_tty();
    if (use_color)
        enable_virtual_terminal();
    const colors palette = make_colors(use_color);

    auto print_row = [&](const output_row& row) {
        out << std::left << std::setw(static_cast<int>(name_width)) << row.name << "  ";
        if (row.is_reference && palette.reference[0] != '\0')
            out << palette.reference;
        out << std::right << std::setw(static_cast<int>(mean_width)) << row.mean << "  "
            << std::setw(static_cast<int>(median_width)) << row.median << "  "
            << std::setw(static_cast<int>(stddev_width)) << row.standard_deviation << "  "
            << std::setw(static_cast<int>(rate_width)) << row.rate;
        if (row.is_reference && palette.reference[0] != '\0')
            out << palette.reset;
        if (!row.status.empty())
        {
            out << "  ";
            const char* color = status_color(palette, row.status);
            if (color[0] != '\0')
                out << color;
            out << std::left << std::setw(static_cast<int>(status_width)) << row.status;
            if (color[0] != '\0')
                out << palette.reset;
        }
        out << '\n';
    };

    if (options.quiet)
    {
        for (const suite_section& section : sections)
            for (const output_row& row : section.rows)
                print_row(row);
        return any_fail ? 1 : 0;
    }

    for (std::size_t section_index = 0; section_index < sections.size(); ++section_index)
    {
        if (section_index > 0)
            out << '\n';

        const suite_section& section = sections[section_index];
        const std::string title = "suite " + section.suite_ptr->name;
        const std::string title_rule(title.size(), '-');
        out << title_rule << '\n' << title << '\n' << title_rule << '\n';
        if (options.verbose && !section.suite_ptr->description.empty())
        {
            for (const std::string& line : split_lines(section.suite_ptr->description))
                out << "  " << line << '\n';
        }

        out << '\n' << header << '\n' << rule << '\n';
        for (const output_row& row : section.rows)
        {
            print_row(row);
            if (options.verbose && row.is_test && !row.description.empty())
            {
                for (const std::string& line : split_lines(row.description))
                    out << "  " << line << '\n';
            }
        }
        out << rule << '\n';

        // Summary row: accumulated, average, median and std dev share the four
        // numeric columns so every field stays aligned with the table.
        out << std::left << std::setw(static_cast<int>(name_width)) << "suite:" << "  "
            << std::right << std::setw(static_cast<int>(mean_width)) << section.accumulated << "  "
            << std::setw(static_cast<int>(median_width)) << format_time(section.summary.mean)
            << "  " << std::setw(static_cast<int>(stddev_width))
            << format_time(section.summary.median) << "  "
            << std::setw(static_cast<int>(rate_width))
            << format_time(section.summary.standard_deviation) << '\n';
    }

    return any_fail ? 1 : 0;
}

} // namespace microbench
} // namespace ucb
