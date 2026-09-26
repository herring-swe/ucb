/**
 * @file buffer.cpp
 *
 * This file is part of the UCB project
 * - SPDX-FileCopyrightText: © 2026 Åke Svedin <ake@svedin.org>
 * - SPDX-License-Identifier: MIT
 *
 * @brief buffer tests
 */

#include <ucb/buffer.h>
#include <ucb/bufutil.h>
#include <ucb/memdbg.h>
#include <ucb/memory.h>

#include <doctest.h>

#include <array>
#include <cstdint>
#include <cstring>
#include <string>

// Ignore some forced buffer manipulations in this testsuite
UCB_DIAG_IGN_UNSAFE_BUFFER_USAGE()

/**
 * Test struct for buffer tests
 * Alignment = 4
 * Size = 12
 */
UCB_DIAG_PUSH()
UCB_DIAG_IGN_PADDED()
typedef struct test_struct
{
    char ch;
    int32_t i32;
    int16_t i16;
} test_struct;
UCB_DIAG_POP()

static size_t grow_half(ucb_buffer* buf, size_t size_needed)
{
    (void)size_needed;
    return buf->alloc / 2;
}

TEST_SUITE_BEGIN("buffer");

TEST_CASE("buffer - general")
{
    UCB_MEMTRACK_PUSH();

    SUBCASE("bufcast aligned")
    {
        uint32_t test_data[4] = {0x11111111, 0x22222222, 0x33333333, 0x44444444};
        void* data = reinterpret_cast<char*>(test_data);
        size_t data_size = sizeof(test_data);
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
        UCB_DIAG_PUSH()
        UCB_DIAG_IGN_CAST_ALIGN()
        *reinterpret_cast<uint32_t*>(unaligned_buf + 1) =
            0xAABBCCDD;                           // Write uint32_t at offset 1 (misaligned)
        ucb_buffer buf = {unaligned_buf + 1, 16}; // 16 bytes (4x uint32_t)
        size_t count;
        uint32_t* result = UCB_BUFCAST(uint32_t, buf.data, buf.size, &count);
        unaligned_buf[0] = 0xFFu; // First byte is 0xFF
        UCB_DIAG_POP()

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
        uint32_t* result = UCB_BUFCAST(uint32_t, buf.data, buf.size, &count);

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
        uint32_t* result = UCB_BUFCAST(uint32_t, buf.data, buf.size, &count);

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
        char* data = reinterpret_cast<char*>(tiny_buf);
        size_t data_size = sizeof(tiny_buf);
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
        void* data = reinterpret_cast<void*>(struct_data);
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

        constexpr size_t num_data = 50;
        constexpr size_t misalign = 3;
        constexpr size_t data_size = num_data * sizeof(test_struct) + misalign;

        char bufmem[data_size];
        test_struct* struct_data = reinterpret_cast<test_struct*>(bufmem + misalign);
        for (size_t i = 0; i < num_data; i++)
        {
            char iadd = static_cast<char>(i);
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

TEST_CASE("buffer - heap")
{
    UCB_MEMTRACK_PUSH();

    const size_t initial_capacity = 16;
    ucb_buffer* buf = ucb_buffer_new_heap(initial_capacity);
    REQUIRE(buf != nullptr);
    REQUIRE(buf->data != nullptr);
    CHECK(buf->size == 0);
    CHECK(buf->alloc == initial_capacity);
    CHECK(ucb_buffer_can_resize(buf));
    CHECK(ucb_buffer_can_transfer(buf));

    SUBCASE("push read pop")
    {
        const char payload[] = "hello";
        REQUIRE(ucb_buffer_push(buf, payload, sizeof(payload)));
        CHECK(buf->size == sizeof(payload));
        CHECK(buf->size <= buf->alloc);

        char out[16] = {0};
        ucb_buffer_read(buf, out, sizeof(payload), 0);
        CHECK(memcmp(out, payload, sizeof(payload)) == 0);

        // Pushing beyond the capacity grows the buffer
        char big[64];
        memset(big, 'A', sizeof(big));
        REQUIRE(ucb_buffer_push(buf, big, sizeof(big)));
        CHECK(buf->size == sizeof(payload) + sizeof(big));
        CHECK(buf->alloc >= buf->size);

        // pop returns the last bytes and reduces the used size
        char pop_out[8];
        ucb_buffer_pop(buf, pop_out, sizeof(pop_out));
        CHECK(buf->size == sizeof(payload) + sizeof(big) - sizeof(pop_out));
        CHECK(memcmp(pop_out, big + sizeof(big) - sizeof(pop_out), sizeof(pop_out)) == 0);

        // clear resets the used size but keeps the capacity
        const size_t cap_before_clear = buf->alloc;
        ucb_buffer_clear(buf);
        CHECK(buf->size == 0);
        CHECK(buf->alloc == cap_before_clear);
    }

    SUBCASE("push_format")
    {
        int written = ucb_buffer_push_format(buf, "value=%d", 42);
        CHECK(written == 8);
        CHECK(buf->size == 9); // formatted text plus null terminator
        CHECK(std::string(buf->data) == "value=42");

        // Empty format still appends a null terminator
        written = ucb_buffer_push_format(buf, "%s", "");
        CHECK(written == 0);
        CHECK(buf->size == 10);
        CHECK(buf->data[9] == '\0');

        // Empty format on a buffer that is exactly full must still grow and
        // append the terminator
        ucb_buffer* exact = ucb_buffer_new_heap(4);
        REQUIRE(exact != nullptr);
        REQUIRE(ucb_buffer_push_format(exact, "%s", "abc") == 3);
        CHECK(exact->size == 4);
        CHECK(exact->alloc == 4);
        REQUIRE(ucb_buffer_push_format(exact, "%s", "") == 0);
        CHECK(exact->size == 5);
        CHECK(exact->alloc >= 5);
        CHECK(exact->data[3] == '\0');
        CHECK(exact->data[4] == '\0');
        ucb_buffer_free(exact);
    }

    ucb_buffer_free(buf);

    UCB_MEMTRACK_POP();
}

TEST_CASE("buffer - static")
{
    UCB_MEMTRACK_PUSH();

    char storage[8];
    memset(storage, 0, sizeof(storage));

    ucb_buffer buf;
    REQUIRE(ucb_buffer_init_static(&buf, storage, sizeof(storage)));
    CHECK(buf.data == storage);
    CHECK(buf.size == 0);
    CHECK(buf.alloc == sizeof(storage));
    CHECK(!ucb_buffer_can_resize(&buf));
    CHECK(!ucb_buffer_can_transfer(&buf));

    const char text[] = "abc";
    REQUIRE(ucb_buffer_push(&buf, text, sizeof(text)));
    CHECK(buf.size == sizeof(text));
    CHECK(memcmp(storage, text, sizeof(text)) == 0);

    // read a subrange
    char out[8] = {0};
    ucb_buffer_read(&buf, out, 3, 1);
    CHECK(memcmp(out, "bc\0", 3) == 0);

    // zero scrubs used bytes only and keeps the borrowed storage
    ucb_buffer_zero(&buf);
    CHECK(buf.data == storage);
    CHECK(buf.size == sizeof(text));
    for (size_t i = 0; i < sizeof(text); i++)
        CHECK(storage[i] == 0);

    ucb_buffer_clear(&buf);
    CHECK(buf.size == 0);
    CHECK(buf.alloc == sizeof(storage));

    // Static buffer has no owned data; release is a no-op
    ucb_buffer_release(&buf);

    // Heap allocated buffer struct wrapping static storage
    ucb_buffer* heap_static = ucb_buffer_new_static(storage, sizeof(storage));
    REQUIRE(heap_static != nullptr);
    CHECK(heap_static->data == storage);
    CHECK(!ucb_buffer_can_transfer(heap_static));
    ucb_buffer_free(heap_static); // frees the struct only, not storage

    UCB_MEMTRACK_POP();
}

TEST_CASE("buffer - grow")
{
    UCB_MEMTRACK_PUSH();

    char data[32];
    memset(data, 'x', sizeof(data));

    ucb_buffer* buf = ucb_buffer_new_heap(8);
    REQUIRE(buf != nullptr);
    CHECK(buf->alloc == 8);

    // Explicit grow adds to the current capacity
    REQUIRE(ucb_buffer_grow(buf, 8));
    CHECK(buf->alloc == 16);

    // ensure is a no-op when enough free space is already available
    size_t cap = buf->alloc;
    REQUIRE(ucb_buffer_ensure(buf, 4));
    CHECK(buf->alloc == cap);

    // Fill part of the buffer, then ensure forces a grow
    REQUIRE(ucb_buffer_push(buf, data, 12));
    REQUIRE(buf->size == 12);
    REQUIRE(ucb_buffer_ensure(buf, 8));
    CHECK(buf->alloc >= buf->size + 8);

    // Shrinking below the used size clamps the used size
    REQUIRE(ucb_buffer_resize(buf, 10));
    CHECK(buf->alloc == 10);
    CHECK(buf->size == 10);

    // Growing again
    REQUIRE(ucb_buffer_resize(buf, 20));
    CHECK(buf->alloc == 20);

    // fit shrinks capacity to the used size
    REQUIRE(ucb_buffer_fit(buf));
    CHECK(buf->alloc == buf->size);

    ucb_buffer_free(buf);

    SUBCASE("grow_double helper")
    {
        ucb_buffer* dbl = ucb_buffer_new_heap(8);
        REQUIRE(dbl != nullptr);
        REQUIRE(ucb_buffer_push(dbl, data, 8)); // fill to capacity

        const size_t expected = ucb_buffer_grow_double(dbl, 1);
        CHECK(expected >= dbl->alloc * 2);

        dbl->grow_func = ucb_buffer_grow_double;
        REQUIRE(ucb_buffer_grow(dbl, 1));
        CHECK(dbl->alloc == expected);
        CHECK(dbl->alloc >= dbl->size + 1);

        ucb_buffer_free(dbl);
    }

    SUBCASE("grow_double zero capacity")
    {
        // A zero-capacity buffer must not spin forever and must still grow.
        ucb_buffer* dbl = ucb_buffer_new_heap(8);
        REQUIRE(dbl != nullptr);
        REQUIRE(ucb_buffer_resize(dbl, 0));
        CHECK(dbl->alloc == 0);
        CHECK(dbl->data == nullptr);

        CHECK(ucb_buffer_grow_double(dbl, 1) > 0);
        CHECK(ucb_buffer_grow_double(dbl, 32) >= 32);

        dbl->grow_func = ucb_buffer_grow_double;
        REQUIRE(ucb_buffer_grow(dbl, 1));
        CHECK(dbl->alloc >= 1);
        CHECK(dbl->data != nullptr);

        ucb_buffer_free(dbl);
    }

    SUBCASE("grow_double overflow clamp")
    {
        // Pure math: must terminate and clamp instead of overflowing.
        ucb_buffer fake;
        memset(&fake, 0, sizeof(fake));
        fake.alloc = SIZE_MAX / 2 + 1;
        fake.size = 0;
        CHECK(ucb_buffer_grow_double(&fake, SIZE_MAX) == SIZE_MAX);
    }

    SUBCASE("grow_func below capacity does not shrink")
    {
        ucb_buffer* bad = ucb_buffer_new_heap(16);
        REQUIRE(bad != nullptr);
        const size_t before = bad->alloc;

        bad->grow_func = grow_half;
        REQUIRE(ucb_buffer_grow(bad, 8));
        CHECK(bad->alloc == before);

        ucb_buffer_free(bad);
    }

    UCB_MEMTRACK_POP();
}

TEST_CASE("buffer - can_ensure")
{
    UCB_MEMTRACK_PUSH();

    SUBCASE("null")
    {
        CHECK(!ucb_buffer_can_ensure(nullptr, 1));
        CHECK(!ucb_buffer_can_ensure(nullptr, 0));
    }

    SUBCASE("static")
    {
        char storage[8];
        ucb_buffer buf;
        REQUIRE(ucb_buffer_init_static(&buf, storage, sizeof(storage)));

        CHECK(ucb_buffer_can_ensure(&buf, 8));
        CHECK(!ucb_buffer_can_ensure(&buf, 9));

        ucb_buffer_release(&buf);
    }

    SUBCASE("heap")
    {
        ucb_buffer* buf = ucb_buffer_new_heap(8);
        REQUIRE(buf != nullptr);
        CHECK(ucb_buffer_can_ensure(buf, 8));

        // One used byte means SIZE_MAX additional bytes would overflow.
        REQUIRE(ucb_buffer_push(buf, "x", 1));
        CHECK(!ucb_buffer_can_ensure(buf, SIZE_MAX));

        ucb_buffer_free(buf);
    }

    UCB_MEMTRACK_POP();
}

TEST_CASE("buffer - capacity zero")
{
    UCB_MEMTRACK_PUSH();

    SUBCASE("resize to zero")
    {
        ucb_buffer* buf = ucb_buffer_new_heap(16);
        REQUIRE(buf != nullptr);
        REQUIRE(ucb_buffer_push(buf, "abcdef", 6));

        REQUIRE(ucb_buffer_resize(buf, 0));
        CHECK(buf->alloc == 0);
        CHECK(buf->size == 0);
        CHECK(buf->data == nullptr);
        CHECK(ucb_buffer_can_resize(buf));

        // Can grow again from zero
        REQUIRE(ucb_buffer_push(buf, "xyz", 3));
        CHECK(buf->size == 3);
        CHECK(buf->alloc >= 3);
        CHECK(memcmp(buf->data, "xyz", 3) == 0);

        ucb_buffer_free(buf);
    }

    SUBCASE("fit empty buffer")
    {
        ucb_buffer* buf = ucb_buffer_new_heap(16);
        REQUIRE(buf != nullptr);
        REQUIRE(ucb_buffer_fit(buf));
        CHECK(buf->alloc == 0);
        CHECK(buf->data == nullptr);

        ucb_buffer_free(buf);
    }

    SUBCASE("grow from zero with double")
    {
        ucb_buffer* buf = ucb_buffer_new_heap(16);
        REQUIRE(buf != nullptr);
        REQUIRE(ucb_buffer_resize(buf, 0));

        buf->grow_func = ucb_buffer_grow_double;
        REQUIRE(ucb_buffer_push(buf, "abcd", 4));
        CHECK(buf->size == 4);
        CHECK(buf->alloc >= 4);

        ucb_buffer_free(buf);
    }

    UCB_MEMTRACK_POP();
}

TEST_CASE("buffer - zero")
{
    UCB_MEMTRACK_PUSH();

    SUBCASE("scrubs used bytes only")
    {
        ucb_buffer* buf = ucb_buffer_new_heap(16);
        REQUIRE(buf != nullptr);
        REQUIRE(ucb_buffer_push(buf, "secret", 6));
        const size_t cap = buf->alloc;
        char* data = buf->data;

        ucb_buffer_zero(buf);
        CHECK(buf->data == data);
        CHECK(buf->size == 6);
        CHECK(buf->alloc == cap);
        for (size_t i = 0; i < 6; i++)
            CHECK(buf->data[i] == 0);

        ucb_buffer_free(buf);
    }

    SUBCASE("no-op when empty")
    {
        ucb_buffer* buf = ucb_buffer_new_heap(16);
        REQUIRE(buf != nullptr);
        ucb_buffer_zero(buf);
        CHECK(buf->size == 0);
        CHECK(buf->alloc == 16);

        ucb_buffer* null_buf = nullptr;
        ucb_buffer_zero(null_buf); // must not crash

        ucb_buffer_free(buf);
    }

    UCB_MEMTRACK_POP();
}

TEST_SUITE_END();
