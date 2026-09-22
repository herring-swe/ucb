/**
 * @file unicode_bench.h
 * This file is part of the UCB project
 * - SPDX-FileCopyrightText: © 2026 Åke Svedin <ake@svedin.org>
 * - SPDX-License-Identifier: MIT
 *
 * @brief Registration entry point for the unicode benchmarks.
 */

#ifndef UCB_TEST_UNICODE_BENCH_H
#define UCB_TEST_UNICODE_BENCH_H

namespace ucb {
namespace microbench {
class benchmark;
}
} // namespace ucb

void register_unicode_benchmarks(ucb::microbench::benchmark& bench);

#endif // UCB_TEST_UNICODE_BENCH_H