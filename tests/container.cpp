/**
 * @file container.cpp
 * This file is part of the UCB project
 * - SPDX-FileCopyrightText: © 2026 Åke Svedin <ake@svedin.org>
 * - SPDX-License-Identifier: MIT
 *
 * @brief Container tests
 */

#include "doctest.h"

#include "ucb/container/pqueue.h"
#include "ucb/memdbg.h"
#include "ucb/memory.h"

#include <chrono>
#include <iostream>
#include <random>

/* -------------------------------------------------------------------------- */
/*                                    Data                                    */
/* -------------------------------------------------------------------------- */

static void* int_clone(const void* data)
{
    int* copy = ucb_malloc_type(1, int);
    *copy     = *reinterpret_cast<const int*>(data);
    return copy;
}

static void int_free(void* data)
{
    ucb_free(data);
}

struct PQueueFixture
{
    ucb_pqueue* pq_owned;
    ucb_pqueue* pq_shared;
    PQueueFixture()
    {
        ucb_pqueue_args args = {0};
        pq_shared            = ucb_pqueue_new(args);

        args.data_clone = int_clone;
        args.data_free  = int_free;
        pq_owned        = ucb_pqueue_new(args);
    }
    ~PQueueFixture()
    {
        ucb_pqueue_free(pq_owned);
        ucb_pqueue_free(pq_shared);
        pq_owned  = nullptr;
        pq_shared = nullptr;
    }
};

/* -------------------------------------------------------------------------- */
/*                                    Tests                                   */
/* -------------------------------------------------------------------------- */

TEST_CASE_FIXTURE(PQueueFixture, "container pqueue")
{
    int val1 = 1;
    int val2 = 2;
    int val3 = 3;

    SUBCASE("Basic functions")
    {
        ucb_pqueue_push(pq_shared, &val2, val2);
        REQUIRE(ucb_pqueue_peek(pq_shared) == &val2);
        REQUIRE(ucb_pqueue_size(pq_shared) == 1);
        ucb_pqueue_clear(pq_shared);
        REQUIRE(ucb_pqueue_size(pq_shared) == 0);

        ucb_pqueue_push(pq_shared, &val2, val2);
        REQUIRE(ucb_pqueue_size(pq_shared) == 1);
        ucb_pqueue_pop(pq_shared);
        REQUIRE(ucb_pqueue_size(pq_shared) == 0);
    }

    SUBCASE("FIFO for equal priority")
    {
        ucb_pqueue_push(pq_shared, &val1, val1);
        ucb_pqueue_push(pq_shared, &val1, val1); // Same priority
        REQUIRE(reinterpret_cast<int*>(ucb_pqueue_pop(pq_shared)) == &val1);
        REQUIRE(reinterpret_cast<int*>(ucb_pqueue_pop(pq_shared)) == &val1);
    }

    SUBCASE("Priority ordering")
    {
        REQUIRE(ucb_pqueue_push(pq_shared, &val1, val1) == 0);
        REQUIRE(ucb_pqueue_push(pq_shared, &val3, val3) == 0);
        REQUIRE(ucb_pqueue_push(pq_shared, &val2, val2) == 1);
        REQUIRE(reinterpret_cast<int*>(ucb_pqueue_pop(pq_shared)) == &val3); // Highest first
        REQUIRE(reinterpret_cast<int*>(ucb_pqueue_pop(pq_shared)) == &val2);
        REQUIRE(reinterpret_cast<int*>(ucb_pqueue_pop(pq_shared)) == &val1);
    }

    SUBCASE("Empty queue")
    {
        REQUIRE(ucb_pqueue_pop(pq_shared) == nullptr);
        REQUIRE(ucb_pqueue_peek(pq_shared) == nullptr);
        REQUIRE(ucb_pqueue_size(pq_shared) == 0);
    }

    SUBCASE("Ownership")
    {
        int* item;
        ucb_pqueue_push(pq_owned, &val1, val1);
        ucb_pqueue_push(pq_owned, &val2, val2);
        ucb_pqueue_push(pq_owned, &val3, val3);
        REQUIRE(ucb_pqueue_size(pq_owned) == 3);
        item = reinterpret_cast<int*>(ucb_pqueue_pop(pq_owned));
        REQUIRE(item != &val3);
        REQUIRE(*item == val3);
        ucb_free(item);
        REQUIRE(ucb_pqueue_size(pq_owned) == 2);
        ucb_pqueue_clear(pq_owned);
        REQUIRE(ucb_pqueue_size(pq_owned) == 0);
    }
}

TEST_CASE_FIXTURE(PQueueFixture, "container pqueue benchmark")
{
    // Use fixed default seed.
    std::mt19937 rng;
    std::uniform_int_distribution<std::mt19937::result_type> randval(1, 10);

    constexpr int num_items = 100000;
    std::vector<int> items(num_items);
    for (int i = 0; i < num_items; ++i)
    {
        items[i] = randval(rng);
    }

    auto start = std::chrono::high_resolution_clock::now();

    for (int i = 0; i < num_items; ++i)
    {
        ucb_pqueue_push(pq_shared, &items[i], items[i]);
    }

    auto end         = std::chrono::high_resolution_clock::now();
    uint64_t push_ms = std::chrono::duration_cast<std::chrono::milliseconds>(end - start).count();

    start = std::chrono::high_resolution_clock::now();

    for (int i = 0; i < num_items; ++i)
    {
        ucb_pqueue_pop(pq_shared);
    }

    end = std::chrono::high_resolution_clock::now();

    uint64_t pop_ms = std::chrono::duration_cast<std::chrono::milliseconds>(end - start).count();

    std::cout << "Num items: " << num_items << std::endl;
    std::cout << "Total time: " << push_ms + pop_ms << " ms" << std::endl;
    std::cout << "Push: " << push_ms << " ms";
    std::cout << ", " << num_items / static_cast<double>(push_ms) << " items/ms" << std::endl;
    std::cout << "Pop: " << pop_ms << "ms";
    std::cout << ", " << num_items / static_cast<double>(pop_ms) << " items/ms" << std::endl;
}
