/**
 * @file pqueue_bench.cpp
 * This file is part of the UCB project
 * - SPDX-FileCopyrightText: © 2026 Åke Svedin <ake@svedin.org>
 * - SPDX-License-Identifier: MIT
 *
 * @brief Benchmarks for ucb_pqueue with std::priority_queue as a reference.
 */

#include "pqueue_bench.h"

#include "microbench.h"

#include "ucb/container/pqueue.h"

#include <cstdint>
#include <queue>
#include <random>
#include <vector>

extern "C" void ucb_bench_sink_ptr(const void* ptr);

namespace {

constexpr int kPqueueCount = 16384;

volatile std::int64_t g_sink = 0;

void sink(std::int64_t value)
{
    g_sink = g_sink + value;
}

struct PrioritySource
{
    std::vector<int> values;

    PrioritySource() : values(kPqueueCount)
    {
        std::mt19937 rng;
        std::uniform_int_distribution<int> randval(1, 10);
        for (int& value : values)
            value = randval(rng);
    }
};

std::vector<int>& priority_source()
{
    static PrioritySource source;
    return source.values;
}

struct PqueueSource
{
    ucb_pqueue* pq;

    PqueueSource()
    {
        ucb_pqueue_args args = {0};
        pq = ucb_pqueue_new(args);
    }

    ~PqueueSource()
    {
        ucb_pqueue_free(pq);
    }
};

ucb_pqueue* shared_pqueue()
{
    static PqueueSource source;
    return source.pq;
}

void pqueue_push()
{
    std::vector<int>& priorities = priority_source();
    ucb_pqueue* pq = shared_pqueue();
    for (int i = 0; i < kPqueueCount; ++i)
        ucb_pqueue_push(pq, &priorities[i], priorities[i]);
    ucb_bench_sink_ptr(pq);
    sink(static_cast<std::int64_t>(ucb_pqueue_size(pq)));
    ucb_pqueue_clear(pq);
}

void pqueue_pop()
{
    std::vector<int>& priorities = priority_source();
    ucb_pqueue* pq = shared_pqueue();
    for (int i = 0; i < kPqueueCount; ++i)
        ucb_pqueue_push(pq, &priorities[i], priorities[i]);

    std::int64_t count = 0;
    for (int i = 0; i < kPqueueCount; ++i)
    {
        if (ucb_pqueue_pop(pq) != nullptr)
            ++count;
    }
    sink(count);
}

void std_pqueue_push()
{
    std::vector<int>& priorities = priority_source();
    std::priority_queue<int> pq;
    for (int i = 0; i < kPqueueCount; ++i)
        pq.push(priorities[i]);
    sink(static_cast<std::int64_t>(pq.size()));
}

void std_pqueue_pop()
{
    std::vector<int>& priorities = priority_source();
    std::priority_queue<int> pq;
    for (int i = 0; i < kPqueueCount; ++i)
        pq.push(priorities[i]);

    std::int64_t count = 0;
    while (!pq.empty())
    {
        pq.pop();
        ++count;
    }
    sink(count);
}

} // namespace

void register_pqueue_benchmarks(ucb::microbench::benchmark& bench)
{
    ucb::microbench::suite& suite =
        bench.add_suite("pqueue", "Priority queue with 16384 items and 10 priority levels");

    ucb::microbench::test& push = suite.add_test("push", "Push all items", pqueue_push);
    push.add_ref(std_pqueue_push, "std-priority-queue");

    ucb::microbench::test& pop =
        suite.add_test("pop", "Push all items, then pop them all", pqueue_pop);
    pop.add_ref(std_pqueue_pop, "std-priority-queue");
}
