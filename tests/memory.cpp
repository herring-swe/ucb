/**
 * This file is part of the UCB project
 * SPDX-FileCopyrightText: © 2025 Åke Svedin <ake@svedin.org>
 * SPDX-License-Identifier: MIT
 */

#include "doctest.h"

#include <ucb/diag.h>
#include <ucb/memdbg.h>
#include <ucb/memory.h>

#include <climits>
#include <vector>
#include <mutex>

UCB_DIAG_PUSH()
UCB_DIAG_IGN_PADDED()
typedef struct memtrack_state
{
    uint32_t magic;
    size_t alloc;
    size_t size;
    size_t peak_size;
    size_t peak_alloc;
    size_t peak_alloc_block;
    size_t total_size;
    size_t total_alloc;
    bool reported;
    bool leaks;
} memtrack_state;
UCB_DIAG_POP()

static memtrack_state s_state = {0};

UCB_DIAG_PUSH()
UCB_DIAG_IGN_UNUSED_FUNCTION()
static void memtrack_func(const ucb_mem_report* const report)
{
    if (!report)
        return;

    s_state.reported         = true;
    s_state.alloc            = report->current_alloc;
    s_state.size             = report->current_size;
    s_state.leaks            = report->leaks;
    s_state.peak_alloc       = report->peak_alloc;
    s_state.peak_size        = report->peak_size;
    s_state.peak_alloc_block = report->peak_alloc_block;
    s_state.total_alloc      = report->total_alloc;
    s_state.total_size       = report->total_size;

    // Verify report
    ucb_mem_alloc* cur = report->allocs;
    size_t cur_alloc     = 0;
    size_t cur_size      = 0;
    while (cur)
    {
        cur_alloc++;
        cur_size += cur->size;

        cur = cur->next;
    }
    REQUIRE(cur_alloc == report->current_alloc);
    REQUIRE(cur_size == report->current_size);
}
UCB_DIAG_POP()

// Force test mutex
static std::mutex s_mutex;

TEST_SUITE_BEGIN("memory" * doctest::description("memory"));

TEST_CASE("basic memory")
{
    s_mutex.lock();

    void* mem  = nullptr;
    void* mem2 = nullptr;

    SUBCASE("ucb_malloc")
    {
        UCB_MEMTRACK_PUSH();

        CHECK(ucb_malloc(0) == nullptr);
        CHECK((mem = ucb_malloc(32)) != nullptr);
        CHECK((mem2 = ucb_malloc(64)) != nullptr);
        CHECK(mem != mem2);
        ucb_free_null(mem);
        ucb_free_null(mem2);

        UCB_MEMTRACK_POP();
    }

    SUBCASE("ucb_calloc")
    {
        UCB_MEMTRACK_PUSH();

        CHECK(ucb_calloc(0, 0) == nullptr);
        CHECK(ucb_calloc(0, 32) == nullptr);
        CHECK(ucb_calloc(32, 0) == nullptr);
        CHECK((mem = ucb_calloc(32, 32)) != nullptr);
        CHECK((mem2 = ucb_calloc(64, 64)) != nullptr);
        CHECK(mem != mem2);
        ucb_free_null(mem);
        ucb_free_null(mem2);

        // Test non-zero allocation (check zero-initialization)
        int* arr = ucb_calloc_type(10, int);
        CHECK(arr != nullptr);
        for (size_t i = 0; i < 10; ++i)
        {
            CHECK(arr[i] == 0);
            arr[i] = static_cast<int>(i);
        }
        ucb_free_null(arr);

        UCB_MEMTRACK_POP();
    }

    SUBCASE("ucb_realloc")
    {
        UCB_MEMTRACK_PUSH();

        CHECK(ucb_realloc(nullptr, 0) == nullptr);
        CHECK((mem = ucb_realloc(nullptr, 32)) != nullptr);
        CHECK((mem = ucb_realloc(mem, 0)) == nullptr);

        int* arr = ucb_realloc_type(nullptr, 10, int);
        for (size_t i = 0; i < 10; ++i)
        {
            arr[i] = static_cast<int>(i);
        }
        arr = ucb_realloc_type(arr, 20, int);
        for (size_t i = 0; i < 10; ++i)
        {
            CHECK(arr[i] == i);
            // CHECK(arr[i + 10] == 0);
        }
        ucb_free_null(arr);

        UCB_MEMTRACK_POP();
    }

    s_mutex.unlock();
}

/**
 * Ensures that we restore memory tracking function after test
 */
struct MemTrackFixture
{
    ucb_mem_report_func prev_func;
    MemTrackFixture() : prev_func(UCB_MEMTRACK_SET_FUNC(memtrack_func)) {}
    ~MemTrackFixture()
    {
        UCB_MEMTRACK_SET_FUNC(prev_func);
    }
};

TEST_CASE_FIXTURE(MemTrackFixture, "memory tracking")
{
    s_mutex.lock();

    if (UCB_MEMTRACK_IS_ENABLED())
    {
        s_state = {0};
        UCB_MEMTRACK_RESET();
        UCB_MEMTRACK_REPORT();
        CHECK(0 == UCB_MEMTRACK_LEVEL());

        REQUIRE(s_state.reported == true);
        REQUIRE(s_state.alloc == 0);
        REQUIRE(s_state.size == 0);
        REQUIRE(s_state.leaks == false);
        CHECK(s_state.total_alloc == 0);
        CHECK(s_state.total_size == 0);
        CHECK(s_state.peak_alloc == 0);
        CHECK(s_state.peak_alloc_block == 0);
        CHECK(s_state.peak_size == 0);

        std::vector<void*> ptrs;

        // Push new level
        s_state = {0};
        UCB_MEMTRACK_PUSH();
        CHECK(1 == UCB_MEMTRACK_LEVEL());

        // Allocate some memory
        ptrs.push_back(ucb_malloc(32));
        ptrs.push_back(ucb_calloc(2, 16));
        void* ptr = ucb_malloc(8);

        s_state = {0};
        UCB_MEMTRACK_POP();
        CHECK(0 == UCB_MEMTRACK_LEVEL());

        // A report of leaks should be generated
        REQUIRE(s_state.reported == true);
        REQUIRE(s_state.alloc == 3);
        REQUIRE(s_state.size == 72);
        REQUIRE(s_state.leaks == true);

        REQUIRE(0 == UCB_MEMTRACK_LEVEL());

        s_state = {0};
        UCB_MEMTRACK_REPORT();

        // All leaked memory must be propagated to previous level
        // Note that a requested report is not a leak report
        REQUIRE(s_state.reported == true);
        REQUIRE(s_state.alloc == 3);
        REQUIRE(s_state.size == 72);
        REQUIRE(s_state.leaks == false);

        ptrs.push_back(ucb_realloc(ptr, 16));

        s_state = {0};
        UCB_MEMTRACK_REPORT();

        // Check that we catched the realloc
        REQUIRE(s_state.reported == true);
        REQUIRE(s_state.alloc == 3);
        REQUIRE(s_state.size == 80);

        UCB_MEMTRACK_PUSH();
        CHECK(1 == UCB_MEMTRACK_LEVEL());

        // No mallocs by these calls
        ucb_malloc(0);
        ucb_calloc(0, 1);
        ucb_realloc(nullptr, 0);

        s_state = {0};
        CHECK(1 == UCB_MEMTRACK_LEVEL());
        UCB_MEMTRACK_REPORT();

        // New level must be empty
        REQUIRE(s_state.reported == true);
        REQUIRE(s_state.alloc == 0);
        REQUIRE(s_state.size == 0);

        for (int i = 0; i < 10; i++)
            ptrs.push_back(ucb_malloc(1024));

        s_state = {0};
        UCB_MEMTRACK_REPORT();

        REQUIRE(s_state.reported == true);
        REQUIRE(s_state.alloc == 10);
        REQUIRE(s_state.size == 10240);
        REQUIRE(s_state.leaks == false);
        CHECK(s_state.total_alloc == 10);
        CHECK(s_state.total_size == 10240);
        CHECK(s_state.peak_alloc == 10);
        CHECK(s_state.peak_alloc_block == 1024);
        CHECK(s_state.peak_size == 10240);

        s_state = {0};
        UCB_MEMTRACK_POP();
        CHECK(0 == UCB_MEMTRACK_LEVEL());

        // Generates a leak report of previous level
        REQUIRE(s_state.reported == true);
        REQUIRE(s_state.alloc == 10);
        REQUIRE(s_state.size == 10240);
        REQUIRE(s_state.leaks == true);
        CHECK(s_state.total_alloc == 10);
        CHECK(s_state.total_size == 10240);
        CHECK(s_state.peak_alloc == 10);
        CHECK(s_state.peak_alloc_block == 1024);
        CHECK(s_state.peak_size == 10240);

        s_state = {0};
        UCB_MEMTRACK_REPORT();
        REQUIRE(s_state.reported == true);
        REQUIRE(s_state.alloc == 10 + 3);
        REQUIRE(s_state.size == 10240 + 80);
        REQUIRE(s_state.leaks == false);
        // If realloc successfully grows a block,
        // it counts as +1 alloc and +size in total allocations
        CHECK(s_state.total_alloc == 14);
        CHECK(s_state.total_size == 10240 + 80 + 8);
        CHECK(s_state.peak_alloc == 13);
        CHECK(s_state.peak_alloc_block == 1024);
        CHECK(s_state.peak_size == 10240 + 80);

        for (int i = 0; i < 1000; i++)
        {
            UCB_MEMTRACK_PUSH();
        }
        CHECK(UCB_MEMTRACK_LEVEL() < 1000);
        for (int i = 0; i < 1000; i++)
        {
            UCB_MEMTRACK_POP();
        }
        CHECK(0 == UCB_MEMTRACK_LEVEL());
        UCB_MEMTRACK_POP();
        CHECK(0 == UCB_MEMTRACK_LEVEL());

        for (std::vector<void*>::iterator it = ptrs.begin(); it != ptrs.end(); ++it)
            ucb_free(*it);
        ptrs.clear();

        s_state = {0};
        UCB_MEMTRACK_REPORT();

        REQUIRE(s_state.reported == true);
        REQUIRE(s_state.alloc == 0);
        REQUIRE(s_state.size == 0);
        REQUIRE(s_state.leaks == false);
        // If realloc successfully grows a block,
        // it counts as +1 alloc and +size in total allocations
        CHECK(s_state.total_alloc == 14);
        CHECK(s_state.total_size == 10240 + 80 + 8);
        CHECK(s_state.peak_alloc == 13);
        CHECK(s_state.peak_alloc_block == 1024);
        CHECK(s_state.peak_size == 10240 + 80);
    }

    s_mutex.unlock();
}

TEST_CASE_FIXTURE(MemTrackFixture, "memory tracking stress")
{
    s_mutex.lock();

    int expected = -1;
    if (UCB_MEMTRACK_IS_ENABLED())
        expected = 0;

    for (int i = 0; i < 10000; i++)
    {
        UCB_MEMTRACK_PUSH();
        void* ptr = ucb_malloc(1);
        UCB_MEMTRACK_POP();
        CHECK(UCB_MEMTRACK_LEVEL() == expected);
        ucb_free(ptr);
    }
    REQUIRE(UCB_MEMTRACK_LEVEL() == expected);

    s_mutex.unlock();
}

TEST_SUITE_END();
