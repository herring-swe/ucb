/**
 * @file pqueue.cpp
 * This file is part of the UCB project
 * - SPDX-FileCopyrightText: © 2026 Åke Svedin <ake@svedin.org>
 * - SPDX-License-Identifier: MIT
 *
 * @brief PQueue tests
 */

#include "ucb/container/pqueue.h"

#include "ucb/memdbg.h"
#include "ucb/memory.h"

#include <doctest.h>

/* -------------------------------------------------------------------------- */
/*                                    Data                                    */
/* -------------------------------------------------------------------------- */

static void* int_clone(const void* data)
{
    int* copy = ucb_malloc_type(1, int);
    *copy = *reinterpret_cast<const int*>(data);
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
        pq_shared = ucb_pqueue_new(args);

        args.data_clone = int_clone;
        args.data_free = int_free;
        pq_owned = ucb_pqueue_new(args);
    }
    ~PQueueFixture()
    {
        ucb_pqueue_free(pq_owned);
        ucb_pqueue_free(pq_shared);
        pq_owned = nullptr;
        pq_shared = nullptr;
    }
};

/* -------------------------------------------------------------------------- */
/*                                    Tests                                   */
/* -------------------------------------------------------------------------- */

TEST_CASE_FIXTURE(PQueueFixture, "pqueue - general")
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

TEST_CASE("pqueue - owned clear")
{
    UCB_MEMTRACK_PUSH();

    ucb_pqueue_args args = {0};
    args.data_clone = int_clone;
    args.data_free = int_free;

    ucb_pqueue* pq = ucb_pqueue_new(args);
    REQUIRE(pq != nullptr);

    int vals[5] = {0, 1, 2, 3, 4};
    for (int i = 0; i < 5; i++)
        REQUIRE(ucb_pqueue_push(pq, &vals[i], i) != (size_t)-1);

    REQUIRE(ucb_pqueue_size(pq) == 5);

    // clear must free every cloned element
    ucb_pqueue_clear(pq);
    REQUIRE(ucb_pqueue_size(pq) == 0);
    REQUIRE(ucb_pqueue_empty(pq));

    ucb_pqueue_free(pq);

    UCB_MEMTRACK_POP();
}

TEST_CASE("pqueue - capacity and mt")
{
    UCB_MEMTRACK_PUSH();

    // Many distinct priorities grow the internal bucket list
    int vals[20];
    ucb_pqueue* pq = ucb_pqueue_new({0});
    REQUIRE(pq != nullptr);
    for (int i = 0; i < 20; i++)
    {
        vals[i] = i;
        REQUIRE(ucb_pqueue_push(pq, &vals[i], i) != (size_t)-1);
    }
    REQUIRE(ucb_pqueue_size(pq) == 20);

    // fit is a no-op on ordering and contents
    ucb_pqueue_fit(pq);
    REQUIRE(ucb_pqueue_size(pq) == 20);

    for (int i = 19; i >= 0; i--)
    {
        int* item = reinterpret_cast<int*>(ucb_pqueue_pop(pq));
        REQUIRE(item != nullptr);
        CHECK(*item == i);
    }
    REQUIRE(ucb_pqueue_size(pq) == 0);
    REQUIRE(ucb_pqueue_empty(pq));
    ucb_pqueue_free(pq);

    // Thread-safe constructor smoke test
    ucb_pqueue* mt = ucb_pqueue_new_mt({0});
    REQUIRE(mt != nullptr);
    int val = 7;
    REQUIRE(ucb_pqueue_push(mt, &val, 1) != (size_t)-1);
    REQUIRE(ucb_pqueue_size(mt) == 1);
    REQUIRE(ucb_pqueue_peek(mt) == &val);
    REQUIRE(reinterpret_cast<int*>(ucb_pqueue_pop(mt)) == &val);
    REQUIRE(ucb_pqueue_size(mt) == 0);
    ucb_pqueue_free(mt);

    // Empty queue operations
    ucb_pqueue* empty = ucb_pqueue_new({0});
    REQUIRE(empty != nullptr);
    CHECK(ucb_pqueue_pop(empty) == nullptr);
    CHECK(ucb_pqueue_peek(empty) == nullptr);
    CHECK(ucb_pqueue_size(empty) == 0);
    CHECK(ucb_pqueue_empty(empty));
    ucb_pqueue_free(empty);

    UCB_MEMTRACK_POP();
}
