/**
 * @file vector_bench.h
 * This file is part of the UCB project
 * - SPDX-FileCopyrightText: © 2026 Åke Svedin <ake@svedin.org>
 * - SPDX-License-Identifier: MIT
 *
 * @brief Registration entry points for the vector benchmarks.
 */

#ifndef UCB_TEST_VECTOR_BENCH_H
#define UCB_TEST_VECTOR_BENCH_H

namespace ucb {
namespace microbench {
class benchmark;
}
} // namespace ucb

/// Number of elements per operation. All tests use the same count so the table
/// and the per-suite summary are directly comparable; the shifting operations
/// (front/middle insert/remove) are intentionally O(n^2) at this size.
constexpr int kBenchCount = 2048;

void register_vector_int_benchmarks(ucb::microbench::benchmark& bench);
void register_vector_ptr_benchmarks(ucb::microbench::benchmark& bench);

#endif // UCB_TEST_VECTOR_BENCH_H
