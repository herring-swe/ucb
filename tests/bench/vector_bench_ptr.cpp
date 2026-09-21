/**
 * @file vector_bench_ptr.cpp
 * This file is part of the UCB project
 * - SPDX-FileCopyrightText: © 2026 Åke Svedin <ake@svedin.org>
 * - SPDX-License-Identifier: MIT
 *
 * @brief Benchmarks for ucb_vector_ptr with std::vector, plain C arrays and the
 * generic ucb_vector (non-owning pointer mode) as references.
 */

#include "microbench.h"
#include "vector_bench.h"

#include "ucb/cast.h"
#include "ucb/container/impl/vector_ptr.h"
#include "ucb/container/vector_generic.h"

#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <vector>

extern "C" void ucb_bench_sink_ptr(const void* ptr);

namespace {

volatile std::int64_t g_sink = 0;

void sink(std::int64_t value)
{
    g_sink = g_sink + value;
}

bool ptr_iter_cb(void* value, std::size_t, void* user)
{
    *static_cast<std::int64_t*>(user) += ucb_ptr2int(value);
    return true;
}

/* -------------------------------------------------------------------------- */
/*                             Shared read sources                            */
/* -------------------------------------------------------------------------- */

struct PtrSource
{
    ucb_vector_ptr* vec;

    PtrSource() : vec(ucb_vector_ptr_new())
    {
        ucb_vector_ptr_reserve(vec, kBenchCount);
        for (int i = 0; i < kBenchCount; ++i)
            ucb_vector_ptr_push_back(vec, ucb_int2ptr(i));
    }

    ~PtrSource()
    {
        ucb_vector_ptr_free(vec);
    }
};

ucb_vector_ptr* ptr_source()
{
    static PtrSource source;
    return source.vec;
}

struct GenericPtrSource
{
    ucb_vector* vec;

    GenericPtrSource()
    {
        ucb_vector_args args = {0};
        args.initial_capacity = kBenchCount;
        vec = ucb_vector_new(args);
        for (int i = 0; i < kBenchCount; ++i)
            ucb_vector_push_back(vec, ucb_int2ptr(i));
    }

    ~GenericPtrSource()
    {
        ucb_vector_free(vec);
    }
};

ucb_vector* generic_ptr_source()
{
    static GenericPtrSource source;
    return source.vec;
}

const std::vector<void*>& ptr_std_source()
{
    static const std::vector<void*> source = [] {
        std::vector<void*> values;
        values.reserve(kBenchCount);
        for (int i = 0; i < kBenchCount; ++i)
            values.push_back(ucb_int2ptr(i));
        return values;
    }();
    return source;
}

struct PtrCArraySource
{
    void* arr[kBenchCount];

    PtrCArraySource()
    {
        for (int i = 0; i < kBenchCount; ++i)
            arr[i] = ucb_int2ptr(i);
    }
};

const void* const* ptr_carray_source()
{
    static PtrCArraySource source;
    return source.arr;
}

/* -------------------------------------------------------------------------- */
/*                                 push_back                                  */
/* -------------------------------------------------------------------------- */

void ptr_push_back()
{
    ucb_vector_ptr* vec = ucb_vector_ptr_new();
    ucb_vector_ptr_reserve(vec, kBenchCount);
    for (int i = 0; i < kBenchCount; ++i)
        ucb_vector_ptr_push_back(vec, ucb_int2ptr(i));
    ucb_bench_sink_ptr(vec->data);
    sink(static_cast<std::int64_t>(ucb_vector_ptr_size(vec)));
    ucb_vector_ptr_free(vec);
}

void ptr_std_push_back()
{
    std::vector<void*> vec;
    vec.reserve(kBenchCount);
    for (int i = 0; i < kBenchCount; ++i)
        vec.push_back(ucb_int2ptr(i));
    ucb_bench_sink_ptr(vec.data());
    sink(static_cast<std::int64_t>(vec.size()));
}

void ptr_carray_push_back()
{
    void* arr[kBenchCount];
    int size = 0;
    for (int i = 0; i < kBenchCount; ++i)
        arr[size++] = ucb_int2ptr(i);
    ucb_bench_sink_ptr(arr);
    sink(size);
}

void generic_ptr_push_back()
{
    ucb_vector_args args = {0};
    args.initial_capacity = kBenchCount;
    ucb_vector* vec = ucb_vector_new(args);
    for (int i = 0; i < kBenchCount; ++i)
        ucb_vector_push_back(vec, ucb_int2ptr(i));
    ucb_bench_sink_ptr(vec);
    sink(static_cast<std::int64_t>(ucb_vector_size(vec)));
    ucb_vector_free(vec);
}

/* -------------------------------------------------------------------------- */
/*                              push_back growth                              */
/* -------------------------------------------------------------------------- */

void ptr_push_back_grow()
{
    ucb_vector_ptr* vec = ucb_vector_ptr_new();
    for (int i = 0; i < kBenchCount; ++i)
        ucb_vector_ptr_push_back(vec, ucb_int2ptr(i));
    ucb_bench_sink_ptr(vec->data);
    sink(static_cast<std::int64_t>(ucb_vector_ptr_size(vec)));
    ucb_vector_ptr_free(vec);
}

void ptr_std_push_back_grow()
{
    std::vector<void*> vec;
    for (int i = 0; i < kBenchCount; ++i)
        vec.push_back(ucb_int2ptr(i));
    ucb_bench_sink_ptr(vec.data());
    sink(static_cast<std::int64_t>(vec.size()));
}

void ptr_carray_push_back_grow()
{
    void** arr = nullptr;
    std::size_t capacity = 0;
    std::size_t size = 0;
    for (int i = 0; i < kBenchCount; ++i)
    {
        if (size == capacity)
        {
            capacity = capacity == 0 ? 8 : capacity * 2;
            arr = static_cast<void**>(std::realloc(arr, capacity * sizeof(void*)));
        }
        arr[size++] = ucb_int2ptr(i);
    }
    ucb_bench_sink_ptr(arr);
    sink(static_cast<std::int64_t>(size));
    std::free(arr);
}

void generic_ptr_push_back_grow()
{
    ucb_vector_args args = {0};
    ucb_vector* vec = ucb_vector_new(args);
    for (int i = 0; i < kBenchCount; ++i)
        ucb_vector_push_back(vec, ucb_int2ptr(i));
    ucb_bench_sink_ptr(vec);
    sink(static_cast<std::int64_t>(ucb_vector_size(vec)));
    ucb_vector_free(vec);
}

/* -------------------------------------------------------------------------- */
/*                                  pop_back                                  */
/* -------------------------------------------------------------------------- */

void ptr_pop_back()
{
    ucb_vector_ptr* vec = ucb_vector_ptr_new();
    ucb_vector_ptr_reserve(vec, kBenchCount);
    for (int i = 0; i < kBenchCount; ++i)
        ucb_vector_ptr_push_back(vec, ucb_int2ptr(i));
    std::int64_t sum = 0;
    while (!ucb_vector_ptr_is_empty(vec))
        sum += ucb_ptr2int(ucb_vector_ptr_pop_back(vec));
    ucb_bench_sink_ptr(vec->data);
    sink(sum);
    ucb_vector_ptr_free(vec);
}

void ptr_std_pop_back()
{
    std::vector<void*> vec;
    vec.reserve(kBenchCount);
    for (int i = 0; i < kBenchCount; ++i)
        vec.push_back(ucb_int2ptr(i));
    std::int64_t sum = 0;
    while (!vec.empty())
    {
        sum += ucb_ptr2int(vec.back());
        vec.pop_back();
    }
    ucb_bench_sink_ptr(vec.data());
    sink(sum);
}

void ptr_carray_pop_back()
{
    void* arr[kBenchCount];
    int size = 0;
    for (int i = 0; i < kBenchCount; ++i)
        arr[size++] = ucb_int2ptr(i);
    std::int64_t sum = 0;
    while (size > 0)
        sum += ucb_ptr2int(arr[--size]);
    ucb_bench_sink_ptr(arr);
    sink(sum);
}

void generic_ptr_pop_back()
{
    ucb_vector_args args = {0};
    args.initial_capacity = kBenchCount;
    ucb_vector* vec = ucb_vector_new(args);
    for (int i = 0; i < kBenchCount; ++i)
        ucb_vector_push_back(vec, ucb_int2ptr(i));
    std::int64_t sum = 0;
    while (!ucb_vector_is_empty(vec))
    {
        void* value = nullptr;
        ucb_vector_pop_back(vec, &value);
        sum += ucb_ptr2int(value);
    }
    ucb_bench_sink_ptr(vec);
    sink(sum);
    ucb_vector_free(vec);
}

/* -------------------------------------------------------------------------- */
/*                                 push_front                                 */
/* -------------------------------------------------------------------------- */

void ptr_push_front()
{
    ucb_vector_ptr* vec = ucb_vector_ptr_new();
    ucb_vector_ptr_reserve(vec, kBenchCount);
    for (int i = 0; i < kBenchCount; ++i)
        ucb_vector_ptr_push_front(vec, ucb_int2ptr(i));
    ucb_bench_sink_ptr(vec->data);
    sink(static_cast<std::int64_t>(ucb_vector_ptr_size(vec)));
    ucb_vector_ptr_free(vec);
}

void ptr_std_push_front()
{
    std::vector<void*> vec;
    vec.reserve(kBenchCount);
    for (int i = 0; i < kBenchCount; ++i)
        vec.insert(vec.begin(), ucb_int2ptr(i));
    ucb_bench_sink_ptr(vec.data());
    sink(static_cast<std::int64_t>(vec.size()));
}

void ptr_carray_push_front()
{
    void* arr[kBenchCount];
    int size = 0;
    for (int i = 0; i < kBenchCount; ++i)
    {
        std::memmove(arr + 1, arr, static_cast<std::size_t>(size) * sizeof(void*));
        arr[0] = ucb_int2ptr(i);
        ++size;
    }
    ucb_bench_sink_ptr(arr);
    sink(size);
}

void generic_ptr_push_front()
{
    ucb_vector_args args = {0};
    args.initial_capacity = kBenchCount;
    ucb_vector* vec = ucb_vector_new(args);
    for (int i = 0; i < kBenchCount; ++i)
        ucb_vector_push_front(vec, ucb_int2ptr(i));
    ucb_bench_sink_ptr(vec);
    sink(static_cast<std::int64_t>(ucb_vector_size(vec)));
    ucb_vector_free(vec);
}

/* -------------------------------------------------------------------------- */
/*                                insert middle                               */
/* -------------------------------------------------------------------------- */

void ptr_insert_middle()
{
    ucb_vector_ptr* vec = ucb_vector_ptr_new();
    ucb_vector_ptr_reserve(vec, kBenchCount * 2);
    for (int i = 0; i < kBenchCount; ++i)
        ucb_vector_ptr_insert(vec, ucb_vector_ptr_size(vec) / 2, ucb_int2ptr(i));
    ucb_bench_sink_ptr(vec->data);
    sink(static_cast<std::int64_t>(ucb_vector_ptr_size(vec)));
    ucb_vector_ptr_free(vec);
}

void ptr_std_insert_middle()
{
    std::vector<void*> vec;
    vec.reserve(kBenchCount * 2);
    for (int i = 0; i < kBenchCount; ++i)
        vec.insert(vec.begin() + static_cast<std::ptrdiff_t>(vec.size() / 2), ucb_int2ptr(i));
    ucb_bench_sink_ptr(vec.data());
    sink(static_cast<std::int64_t>(vec.size()));
}

void ptr_carray_insert_middle()
{
    void* arr[kBenchCount * 2];
    int size = 0;
    for (int i = 0; i < kBenchCount; ++i)
    {
        const std::size_t pos = static_cast<std::size_t>(size) / 2;
        std::memmove(arr + pos + 1,
                     arr + pos,
                     (static_cast<std::size_t>(size) - pos) * sizeof(void*));
        arr[pos] = ucb_int2ptr(i);
        ++size;
    }
    ucb_bench_sink_ptr(arr);
    sink(size);
}

void generic_ptr_insert_middle()
{
    ucb_vector_args args = {0};
    args.initial_capacity = kBenchCount * 2;
    ucb_vector* vec = ucb_vector_new(args);
    for (int i = 0; i < kBenchCount; ++i)
        ucb_vector_insert(vec, ucb_vector_size(vec) / 2, ucb_int2ptr(i));
    ucb_bench_sink_ptr(vec);
    sink(static_cast<std::int64_t>(ucb_vector_size(vec)));
    ucb_vector_free(vec);
}

/* -------------------------------------------------------------------------- */
/*                                remove middle                               */
/* -------------------------------------------------------------------------- */

void ptr_remove_middle()
{
    ucb_vector_ptr* vec = ucb_vector_ptr_new();
    ucb_vector_ptr_reserve(vec, kBenchCount);
    for (int i = 0; i < kBenchCount; ++i)
        ucb_vector_ptr_push_back(vec, ucb_int2ptr(i));
    std::int64_t sum = 0;
    while (!ucb_vector_ptr_is_empty(vec))
        sum += ucb_ptr2int(ucb_vector_ptr_remove(vec, ucb_vector_ptr_size(vec) / 2));
    ucb_bench_sink_ptr(vec->data);
    sink(sum);
    ucb_vector_ptr_free(vec);
}

void ptr_std_remove_middle()
{
    std::vector<void*> vec;
    vec.reserve(kBenchCount);
    for (int i = 0; i < kBenchCount; ++i)
        vec.push_back(ucb_int2ptr(i));
    std::int64_t sum = 0;
    while (!vec.empty())
    {
        const std::size_t pos = vec.size() / 2;
        sum += ucb_ptr2int(vec[pos]);
        vec.erase(vec.begin() + static_cast<std::ptrdiff_t>(pos));
    }
    ucb_bench_sink_ptr(vec.data());
    sink(sum);
}

void ptr_carray_remove_middle()
{
    void* arr[kBenchCount];
    int size = 0;
    for (int i = 0; i < kBenchCount; ++i)
        arr[size++] = ucb_int2ptr(i);
    std::int64_t sum = 0;
    while (size > 0)
    {
        const std::size_t pos = static_cast<std::size_t>(size) / 2;
        sum += ucb_ptr2int(arr[pos]);
        std::memmove(arr + pos,
                     arr + pos + 1,
                     (static_cast<std::size_t>(size) - pos - 1) * sizeof(void*));
        --size;
    }
    ucb_bench_sink_ptr(arr);
    sink(sum);
}

void generic_ptr_remove_middle()
{
    ucb_vector_args args = {0};
    args.initial_capacity = kBenchCount;
    ucb_vector* vec = ucb_vector_new(args);
    for (int i = 0; i < kBenchCount; ++i)
        ucb_vector_push_back(vec, ucb_int2ptr(i));
    std::int64_t sum = 0;
    while (!ucb_vector_is_empty(vec))
    {
        void* value = nullptr;
        ucb_vector_remove(vec, ucb_vector_size(vec) / 2, &value);
        sum += ucb_ptr2int(value);
    }
    ucb_bench_sink_ptr(vec);
    sink(sum);
    ucb_vector_free(vec);
}

/* -------------------------------------------------------------------------- */
/*                              indexed read                                  */
/* -------------------------------------------------------------------------- */

void ptr_read()
{
    const ucb_vector_ptr* vec = ptr_source();
    const std::size_t size = ucb_vector_ptr_size(vec);
    std::int64_t sum = 0;
    for (std::size_t i = 0; i < size; ++i)
        sum += ucb_ptr2int(ucb_vector_ptr_get(vec, i));
    sink(sum);
}

void ptr_std_read()
{
    const std::vector<void*>& vec = ptr_std_source();
    std::int64_t sum = 0;
    for (std::size_t i = 0; i < vec.size(); ++i)
        sum += ucb_ptr2int(vec[i]);
    sink(sum);
}

void ptr_carray_read()
{
    const void* const* arr = ptr_carray_source();
    std::int64_t sum = 0;
    for (int i = 0; i < kBenchCount; ++i)
        sum += ucb_ptr2int(const_cast<void*>(arr[i]));
    sink(sum);
}

void generic_ptr_read()
{
    const ucb_vector* vec = generic_ptr_source();
    const std::size_t size = ucb_vector_size(vec);
    std::int64_t sum = 0;
    for (std::size_t i = 0; i < size; ++i)
    {
        void* value = nullptr;
        ucb_vector_get(vec, i, &value);
        sum += ucb_ptr2int(value);
    }
    sink(sum);
}

/* -------------------------------------------------------------------------- */
/*                                   foreach                                  */
/* -------------------------------------------------------------------------- */

void ptr_foreach()
{
    ucb_vector_ptr* vec = ptr_source();
    std::int64_t sum = 0;
    ucb_vector_ptr_foreach(vec, ptr_iter_cb, &sum);
    sink(sum);
}

void ptr_std_foreach()
{
    const std::vector<void*>& vec = ptr_std_source();
    std::int64_t sum = 0;
    for (void* value : vec)
        sum += ucb_ptr2int(value);
    sink(sum);
}

void ptr_carray_foreach()
{
    const void* const* arr = ptr_carray_source();
    std::int64_t sum = 0;
    for (const void* const* p = arr; p != arr + kBenchCount; ++p)
        sum += ucb_ptr2int(const_cast<void*>(*p));
    sink(sum);
}

void generic_ptr_foreach()
{
    ucb_vector* vec = generic_ptr_source();
    std::int64_t sum = 0;
    ucb_vector_foreach(vec, ptr_iter_cb, &sum);
    sink(sum);
}

/* -------------------------------------------------------------------------- */
/*                                    copy                                    */
/* -------------------------------------------------------------------------- */

void ptr_copy()
{
    ucb_vector_ptr* dst = ucb_vector_ptr_clone(ptr_source());
    ucb_bench_sink_ptr(dst->data);
    sink(static_cast<std::int64_t>(ucb_vector_ptr_size(dst)));
    ucb_vector_ptr_free(dst);
}

void ptr_std_copy()
{
    const std::vector<void*> dst = ptr_std_source();
    ucb_bench_sink_ptr(dst.data());
    sink(static_cast<std::int64_t>(dst.size()));
}

void ptr_carray_copy()
{
    void* dst[kBenchCount];
    std::memcpy(dst, ptr_carray_source(), sizeof(dst));
    ucb_bench_sink_ptr(dst);
    sink(static_cast<std::int64_t>(sizeof(dst) / sizeof(dst[0])));
}

void generic_ptr_copy()
{
    ucb_vector* dst = ucb_vector_clone(generic_ptr_source());
    ucb_bench_sink_ptr(dst);
    sink(static_cast<std::int64_t>(ucb_vector_size(dst)));
    ucb_vector_free(dst);
}

} // namespace

/* -------------------------------------------------------------------------- */
/*                                Registration                                */
/* -------------------------------------------------------------------------- */

void register_vector_ptr_benchmarks(ucb::microbench::benchmark& bench)
{
    ucb::microbench::suite& generic =
        bench.add_suite("vector-generic-ptr",
                        "Generic ucb_vector, non-owning pointer mode (2048 elements)");

    generic.add_test("push-back", "Reserved push_back", generic_ptr_push_back);
    generic.add_test("push-back-grow", "Growing push_back", generic_ptr_push_back_grow);
    generic.add_test("pop-back", "Push then pop_back", generic_ptr_pop_back);
    generic.add_test("push-front", "push_front with shifting", generic_ptr_push_front);
    generic.add_test("insert-middle", "Insert at the middle", generic_ptr_insert_middle);
    generic.add_test("remove-middle", "Remove from the middle", generic_ptr_remove_middle);
    generic.add_test("read", "Indexed read of all elements", generic_ptr_read);
    generic.add_test("foreach", "Iterate with foreach", generic_ptr_foreach);
    generic.add_test("copy", "Clone the vector", generic_ptr_copy);

    ucb::microbench::suite& suite =
        bench.add_suite("vector-ptr", "ucb_vector_ptr (2048 elements per op)");

    ucb::microbench::test& push_back =
        suite.add_test("push-back", "Reserved push_back", ptr_push_back);
    push_back.add_ref(ptr_std_push_back, "std-vector");
    push_back.add_ref(ptr_carray_push_back, "c-array");
    push_back.add_ref("vector-generic-ptr::push-back");

    ucb::microbench::test& push_back_grow =
        suite.add_test("push-back-grow", "Growing push_back", ptr_push_back_grow);
    push_back_grow.add_ref(ptr_std_push_back_grow, "std-vector");
    push_back_grow.add_ref(ptr_carray_push_back_grow, "c-array");
    push_back_grow.add_ref("vector-generic-ptr::push-back-grow");

    ucb::microbench::test& pop_back =
        suite.add_test("pop-back", "Push then pop_back", ptr_pop_back);
    pop_back.add_ref(ptr_std_pop_back, "std-vector");
    pop_back.add_ref(ptr_carray_pop_back, "c-array");
    pop_back.add_ref("vector-generic-ptr::pop-back");

    ucb::microbench::test& push_front =
        suite.add_test("push-front", "push_front with shifting", ptr_push_front);
    push_front.add_ref(ptr_std_push_front, "std-vector");
    push_front.add_ref(ptr_carray_push_front, "c-array");
    push_front.add_ref("vector-generic-ptr::push-front");

    ucb::microbench::test& insert_middle =
        suite.add_test("insert-middle", "Insert at the middle", ptr_insert_middle);
    insert_middle.add_ref(ptr_std_insert_middle, "std-vector");
    insert_middle.add_ref(ptr_carray_insert_middle, "c-array");
    insert_middle.add_ref("vector-generic-ptr::insert-middle");

    ucb::microbench::test& remove_middle =
        suite.add_test("remove-middle", "Remove from the middle", ptr_remove_middle);
    remove_middle.add_ref(ptr_std_remove_middle, "std-vector");
    remove_middle.add_ref(ptr_carray_remove_middle, "c-array");
    remove_middle.add_ref("vector-generic-ptr::remove-middle");

    ucb::microbench::test& read = suite.add_test("read", "Indexed read of all elements", ptr_read);
    read.add_ref(ptr_std_read, "std-vector");
    read.add_ref(ptr_carray_read, "c-array");
    read.add_ref("vector-generic-ptr::read");

    ucb::microbench::test& foreach = suite.add_test("foreach", "Iterate with foreach", ptr_foreach);
    foreach
        .add_ref(ptr_std_foreach, "std-vector");
    foreach
        .add_ref(ptr_carray_foreach, "c-array");
    foreach
        .add_ref("vector-generic-ptr::foreach");

    ucb::microbench::test& copy = suite.add_test("copy", "Clone the vector", ptr_copy);
    copy.add_ref(ptr_std_copy, "std-vector");
    copy.add_ref(ptr_carray_copy, "c-array");
    copy.add_ref("vector-generic-ptr::copy");
}
