/**
 * This file is part of the UCB project
 * SPDX-FileCopyrightText: © 2025 Åke Svedin <ake@svedin.org>
 * SPDX-License-Identifier: MIT
 */

#include "doctest.h"

#include <ucb/buffer.h>
#include <ucb/bufutil.h>
#include <ucb/memdbg.h>
#include <ucb/memory.h>

#include <array>
#include <cstring>
#include <string>

// Ignore some forced buffer manipulations in this testsuite
UCB_DIAG_IGN_UNSAFE_BUFFER_USAGE

/**
 * Test struct for buffer tests
 * Alignment = 4
 * Size = 12
 */
UCB_DIAG_PUSH
UCB_DIAG_IGN_PADDED
typedef struct test_struct
{
    char ch;
    int32_t i32;
    int16_t i16;
} test_struct;
UCB_DIAG_POP

TEST_SUITE_BEGIN("buffer");

// TEST_CASE("buffer basics")
// {
//     UCB_MEMTRACK_PUSH();

//     UCB_MEMTRACK_POP();
// }

TEST_CASE("buffer utils")
{
    UCB_MEMTRACK_PUSH();

    SUBCASE("bufcast aligned")
    {
        uint32_t test_data[4] = {0x11111111, 0x22222222, 0x33333333, 0x44444444};
        void* data            = reinterpret_cast<char*>(test_data);
        size_t data_size      = sizeof(test_data);
        size_t count;
        uint32_t* result = UCB_BUFCAST(uint32_t, data, data_size, &count);

        // Check that same buffer was returned
        REQUIRE(reinterpret_cast<void*>(result) == data);
        CHECK(count == 4);
        CHECK(result[0] == 0x11111111);
        CHECK(result[1] == 0x22222222);
        CHECK(result[2] == 0x33333333);
        CHECK(result[3] == 0x44444444);
    }

    SUBCASE("bufcast misaligned")
    {
        char unaligned_buf[17] = {0}; // 17 bytes (not aligned to 4)
        UCB_DIAG_PUSH
        UCB_DIAG_IGN_ALIGN_CAST
        *reinterpret_cast<uint32_t*>(unaligned_buf + 1) =
            0xAABBCCDD;                             // Write uint32_t at offset 1 (misaligned)
        ucb_buffer buf = {unaligned_buf + 1, 16}; // 16 bytes (4x uint32_t)
        size_t count;
        uint32_t* result = UCB_BUFCAST(uint32_t, buf.data, buf.used, &count);
        unaligned_buf[0] = 0xFFu; // First byte is 0xFF
        UCB_DIAG_POP

        // Check that we got a copy
        REQUIRE(reinterpret_cast<void*>(result) != reinterpret_cast<void*>(buf.data));
        REQUIRE(count == 4);
        CHECK(result[0] == 0xAABBCCDD);
        CHECK(result[1] == 0x00000000);
        CHECK(result[2] == 0x00000000);
        CHECK(result[3] == 0x00000000);
        ucb_free(result);
    }

    SUBCASE("empty buffer")
    {
        ucb_buffer buf = {nullptr, 0};
        size_t count;
        uint32_t* result = UCB_BUFCAST(uint32_t, buf.data, buf.used, &count);

        CHECK(result == nullptr);
        CHECK(count == 0);
    }

    SUBCASE("partial alignment")
    {
        char partial_buf[10] = {0}; // 10 bytes (2x uint32_t + 2 bytes)

        *reinterpret_cast<uint32_t*>(partial_buf + 0) = 0xAABBCCDD;
        *reinterpret_cast<uint32_t*>(partial_buf + 4) = 0x12345678;

        ucb_buffer buf = {partial_buf, sizeof(partial_buf)};
        size_t count;
        uint32_t* result = UCB_BUFCAST(uint32_t, buf.data, buf.used, &count);

        CHECK(count == 2); // Only 2 full uint32_t elements
        // Could be both cast or copied
        CHECK(result[0] == 0xAABBCCDD);
        CHECK(result[1] == 0x12345678);
        if (reinterpret_cast<uintptr_t>(result) != reinterpret_cast<uintptr_t>(buf.data))
            ucb_free(result);
    }

    SUBCASE("tiny buffer")
    {
        unsigned char tiny_buf[2] = {0xFF, 0xFF};
        char* data                = reinterpret_cast<char*>(tiny_buf);
        size_t data_size          = sizeof(tiny_buf);
        size_t count;
        uint32_t* result = UCB_BUFCAST(uint32_t, data, data_size, &count);

        REQUIRE(count == 0);
        REQUIRE(reinterpret_cast<void*>(result) != reinterpret_cast<void*>(data));
        REQUIRE(result == nullptr);
    }

    SUBCASE("packed struct data")
    {
        test_struct struct_data[3] = {
            {'a', 0x10000001, 0x1001},
            {'b', 0x10000002, 0x1002},
            {'c', 0x10000003, 0x1003},
        };
        void* data       = reinterpret_cast<void*>(struct_data);
        size_t data_size = sizeof(struct_data);

        size_t count;
        test_struct* result = UCB_BUFCAST(test_struct, data, data_size, &count);
        REQUIRE(count == 3);
        REQUIRE(reinterpret_cast<void*>(result) == data);
        REQUIRE(result[0].ch == 'a');
    }

    SUBCASE("misaligned packed struct data")
    {
        CHECK(alignof(test_struct) == 4);
        CHECK(sizeof(test_struct) == 12);

        constexpr size_t num_data  = 50;
        constexpr size_t misalign  = 3;
        constexpr size_t data_size = num_data * sizeof(test_struct) + misalign;

        char bufmem[data_size];
        test_struct* struct_data = reinterpret_cast<test_struct*>(bufmem + misalign);
        for (size_t i = 0; i < num_data; i++)
        {
            char iadd      = static_cast<char>(i);
            struct_data[i] = {
                static_cast<char>('A' + i % 26),
                static_cast<int32_t>(0x10000000 + iadd),
                static_cast<int16_t>(0x1000 + i),
            };
        }

        void* data = reinterpret_cast<void*>(bufmem + misalign);
        // size_t data_size = sizeof(struct_data) * num_data;

        size_t count;
        test_struct* result = UCB_BUFCAST(test_struct, data, data_size, &count);
        REQUIRE(count == 50);
        REQUIRE(reinterpret_cast<void*>(result) != data);

        for (size_t i = 0; i < count; i++)
        {
            REQUIRE(result[i].ch == struct_data[i].ch);
            REQUIRE(result[i].i32 == struct_data[i].i32);
            REQUIRE(result[i].i16 == struct_data[i].i16);
        }

        ucb_free(result);
    }

    UCB_MEMTRACK_POP();
}

TEST_SUITE_END();
