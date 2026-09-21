/**
 * @file microbench_main.cpp
 * This file is part of the UCB project
 * - SPDX-FileCopyrightText: © 2026 Åke Svedin <ake@svedin.org>
 * - SPDX-License-Identifier: MIT
 *
 * @brief Usage example for the internal microbench library.
 */

#include "microbench.h"
#include "vector_bench.h"

#include <cstdint>
#include <exception>
#include <iostream>

extern "C" void microbench_c_function(void);

static void microbench_cpp_function()
{
    volatile std::uint32_t value = 1;
    value *= 3;
}

static void microbench_slow_function()
{
    volatile std::uint64_t value = 0;
    for (std::uint64_t i = 0; i < 1000; ++i)
        value += i * 3U;
}

static void microbench_alt_function()
{
    volatile std::uint32_t value = 7;
    value = value ^ 5U;
}

static void microbench_wrapped_c_function()
{
    microbench_c_function();
}

static void register_c_basics(ucb::microbench::benchmark& bench)
{
    ucb::microbench::suite& suite = bench.add_suite("c-basics", "Small callables: pure C and C++");

    ucb::microbench::test& c =
        suite.add_test("c-function", "Pure C function", microbench_c_function);
    c.add_ref(microbench_alt_function, "anon-alt");
    c.add_ref("slow-function", ucb::microbench::expectation::faster);

    ucb::microbench::test& cpp =
        suite.add_test("cpp-function", "C++ function", microbench_cpp_function);
    cpp.add_ref("c-function", ucb::microbench::expectation::none);

    suite.add_test("slow-function", "Deliberately slow", microbench_slow_function);
}

static void register_wrapper(ucb::microbench::benchmark& bench)
{
    ucb::microbench::suite& suite = bench.add_suite("wrapper", "Cross-suite references");

    ucb::microbench::test& wrapped =
        suite.add_test("wrapped-c", "C call through a wrapper", microbench_wrapped_c_function);
    wrapped.add_ref("c-basics::slow-function", ucb::microbench::expectation::faster);
    wrapped.add_ref("c-basics::c-function", ucb::microbench::expectation::none);
}

int main(int argc, char** argv)
{
    ucb::microbench::benchmark bench;
    try
    {
        register_c_basics(bench);
        register_wrapper(bench);
        register_vector_int_benchmarks(bench);
        register_vector_ptr_benchmarks(bench);
    }
    catch (const std::exception& error)
    {
        std::cerr << "error: " << error.what() << '\n';
        return 2;
    }

    return bench.run(argc, argv, std::cout, std::cerr);
}
