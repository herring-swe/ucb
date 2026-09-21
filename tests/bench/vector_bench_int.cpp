/**
 * @file vector_bench_int.cpp
 * This file is part of the UCB project
 * - SPDX-FileCopyrightText: © 2026 Åke Svedin <ake@svedin.org>
 * - SPDX-License-Identifier: MIT
 *
 * @brief Benchmarks for ucb_vector_int with std::vector, plain C arrays and the
 * generic ucb_vector as references.
 */

#include "microbench.h"
#include "vector_bench.h"

#include "ucb/container/impl/vector_int.h"
#include "ucb/container/vector_generic.h"

#include <algorithm>
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

int cmp_int(const void* a, const void* b)
{
    const int lhs = *static_cast<const int*>(a);
    const int rhs = *static_cast<const int*>(b);
    return (lhs > rhs) - (lhs < rhs);
}

bool int_iter_cb(int* value, std::size_t, void* user)
{
    *static_cast<std::int64_t*>(user) += *value;
    return true;
}

bool generic_int_iter_cb(void* value, std::size_t, void* user)
{
    *static_cast<std::int64_t*>(user) += *static_cast<int*>(value);
    return true;
}

/* -------------------------------------------------------------------------- */
/*                             Shared read sources                            */
/* -------------------------------------------------------------------------- */

struct IntSource
{
    ucb_vector_int* vec;

    IntSource() : vec(ucb_vector_int_new())
    {
        ucb_vector_int_reserve(vec, kBenchCount);
        for (int i = 0; i < kBenchCount; ++i)
            ucb_vector_int_push_back(vec, i);
    }

    ~IntSource()
    {
        ucb_vector_int_free(vec);
    }
};

ucb_vector_int* int_source()
{
    static IntSource source;
    return source.vec;
}

struct GenericIntSource
{
    ucb_vector* vec;

    GenericIntSource()
    {
        ucb_vector_args args = {0};
        args.element_size = sizeof(int);
        args.initial_capacity = kBenchCount;
        vec = ucb_vector_new(args);
        for (int i = 0; i < kBenchCount; ++i)
            ucb_vector_push_back(vec, &i);
    }

    ~GenericIntSource()
    {
        ucb_vector_free(vec);
    }
};

ucb_vector* generic_int_source()
{
    static GenericIntSource source;
    return source.vec;
}

const std::vector<int>& int_std_source()
{
    static const std::vector<int> source = [] {
        std::vector<int> values;
        values.reserve(kBenchCount);
        for (int i = 0; i < kBenchCount; ++i)
            values.push_back(i);
        return values;
    }();
    return source;
}

struct IntCArraySource
{
    int arr[kBenchCount];

    IntCArraySource()
    {
        for (int i = 0; i < kBenchCount; ++i)
            arr[i] = i;
    }
};

const int* int_carray_source()
{
    static IntCArraySource source;
    return source.arr;
}

/* -------------------------------------------------------------------------- */
/*                                 push_back                                  */
/* -------------------------------------------------------------------------- */

void int_push_back()
{
    ucb_vector_int* vec = ucb_vector_int_new();
    ucb_vector_int_reserve(vec, kBenchCount);
    for (int i = 0; i < kBenchCount; ++i)
        ucb_vector_int_push_back(vec, i);
    ucb_bench_sink_ptr(vec->data);
    sink(static_cast<std::int64_t>(ucb_vector_int_size(vec)));
    ucb_vector_int_free(vec);
}

void int_std_push_back()
{
    std::vector<int> vec;
    vec.reserve(kBenchCount);
    for (int i = 0; i < kBenchCount; ++i)
        vec.push_back(i);
    ucb_bench_sink_ptr(vec.data());
    sink(static_cast<std::int64_t>(vec.size()));
}

void int_carray_push_back()
{
    int arr[kBenchCount];
    int size = 0;
    for (int i = 0; i < kBenchCount; ++i)
        arr[size++] = i;
    ucb_bench_sink_ptr(arr);
    sink(size);
}

void generic_int_push_back()
{
    ucb_vector_args args = {0};
    args.element_size = sizeof(int);
    args.initial_capacity = kBenchCount;
    ucb_vector* vec = ucb_vector_new(args);
    for (int i = 0; i < kBenchCount; ++i)
        ucb_vector_push_back(vec, &i);
    ucb_bench_sink_ptr(vec);
    sink(static_cast<std::int64_t>(ucb_vector_size(vec)));
    ucb_vector_free(vec);
}

/* -------------------------------------------------------------------------- */
/*                              push_back growth                              */
/* -------------------------------------------------------------------------- */

void int_push_back_grow()
{
    ucb_vector_int* vec = ucb_vector_int_new();
    for (int i = 0; i < kBenchCount; ++i)
        ucb_vector_int_push_back(vec, i);
    ucb_bench_sink_ptr(vec->data);
    sink(static_cast<std::int64_t>(ucb_vector_int_size(vec)));
    ucb_vector_int_free(vec);
}

void int_std_push_back_grow()
{
    std::vector<int> vec;
    for (int i = 0; i < kBenchCount; ++i)
        vec.push_back(i);
    ucb_bench_sink_ptr(vec.data());
    sink(static_cast<std::int64_t>(vec.size()));
}

void int_carray_push_back_grow()
{
    int* arr = nullptr;
    std::size_t capacity = 0;
    std::size_t size = 0;
    for (int i = 0; i < kBenchCount; ++i)
    {
        if (size == capacity)
        {
            capacity = capacity == 0 ? 8 : capacity * 2;
            arr = static_cast<int*>(std::realloc(arr, capacity * sizeof(int)));
        }
        arr[size++] = i;
    }
    ucb_bench_sink_ptr(arr);
    sink(static_cast<std::int64_t>(size));
    std::free(arr);
}

void generic_int_push_back_grow()
{
    ucb_vector_args args = {0};
    args.element_size = sizeof(int);
    ucb_vector* vec = ucb_vector_new(args);
    for (int i = 0; i < kBenchCount; ++i)
        ucb_vector_push_back(vec, &i);
    ucb_bench_sink_ptr(vec);
    sink(static_cast<std::int64_t>(ucb_vector_size(vec)));
    ucb_vector_free(vec);
}

/* -------------------------------------------------------------------------- */
/*                                  pop_back                                  */
/* -------------------------------------------------------------------------- */

void int_pop_back()
{
    ucb_vector_int* vec = ucb_vector_int_new();
    ucb_vector_int_reserve(vec, kBenchCount);
    for (int i = 0; i < kBenchCount; ++i)
        ucb_vector_int_push_back(vec, i);
    std::int64_t sum = 0;
    while (!ucb_vector_int_is_empty(vec))
        sum += ucb_vector_int_pop_back(vec);
    ucb_bench_sink_ptr(vec->data);
    sink(sum);
    ucb_vector_int_free(vec);
}

void int_std_pop_back()
{
    std::vector<int> vec;
    vec.reserve(kBenchCount);
    for (int i = 0; i < kBenchCount; ++i)
        vec.push_back(i);
    std::int64_t sum = 0;
    while (!vec.empty())
    {
        sum += vec.back();
        vec.pop_back();
    }
    ucb_bench_sink_ptr(vec.data());
    sink(sum);
}

void int_carray_pop_back()
{
    int arr[kBenchCount];
    int size = 0;
    for (int i = 0; i < kBenchCount; ++i)
        arr[size++] = i;
    std::int64_t sum = 0;
    while (size > 0)
        sum += arr[--size];
    ucb_bench_sink_ptr(arr);
    sink(sum);
}

void generic_int_pop_back()
{
    ucb_vector_args args = {0};
    args.element_size = sizeof(int);
    args.initial_capacity = kBenchCount;
    ucb_vector* vec = ucb_vector_new(args);
    for (int i = 0; i < kBenchCount; ++i)
        ucb_vector_push_back(vec, &i);
    std::int64_t sum = 0;
    while (!ucb_vector_is_empty(vec))
    {
        int value = 0;
        ucb_vector_pop_back(vec, &value);
        sum += value;
    }
    ucb_bench_sink_ptr(vec);
    sink(sum);
    ucb_vector_free(vec);
}

/* -------------------------------------------------------------------------- */
/*                                 push_front                                 */
/* -------------------------------------------------------------------------- */

void int_push_front()
{
    ucb_vector_int* vec = ucb_vector_int_new();
    ucb_vector_int_reserve(vec, kBenchCount);
    for (int i = 0; i < kBenchCount; ++i)
        ucb_vector_int_push_front(vec, i);
    ucb_bench_sink_ptr(vec->data);
    sink(static_cast<std::int64_t>(ucb_vector_int_size(vec)));
    ucb_vector_int_free(vec);
}

void int_std_push_front()
{
    std::vector<int> vec;
    vec.reserve(kBenchCount);
    for (int i = 0; i < kBenchCount; ++i)
        vec.insert(vec.begin(), i);
    ucb_bench_sink_ptr(vec.data());
    sink(static_cast<std::int64_t>(vec.size()));
}

void int_carray_push_front()
{
    int arr[kBenchCount];
    int size = 0;
    for (int i = 0; i < kBenchCount; ++i)
    {
        std::memmove(arr + 1, arr, static_cast<std::size_t>(size) * sizeof(int));
        arr[0] = i;
        ++size;
    }
    ucb_bench_sink_ptr(arr);
    sink(size);
}

void generic_int_push_front()
{
    ucb_vector_args args = {0};
    args.element_size = sizeof(int);
    args.initial_capacity = kBenchCount;
    ucb_vector* vec = ucb_vector_new(args);
    for (int i = 0; i < kBenchCount; ++i)
        ucb_vector_push_front(vec, &i);
    ucb_bench_sink_ptr(vec);
    sink(static_cast<std::int64_t>(ucb_vector_size(vec)));
    ucb_vector_free(vec);
}

/* -------------------------------------------------------------------------- */
/*                                insert middle                               */
/* -------------------------------------------------------------------------- */

void int_insert_middle()
{
    ucb_vector_int* vec = ucb_vector_int_new();
    ucb_vector_int_reserve(vec, kBenchCount * 2);
    for (int i = 0; i < kBenchCount; ++i)
        ucb_vector_int_insert(vec, ucb_vector_int_size(vec) / 2, i);
    ucb_bench_sink_ptr(vec->data);
    sink(static_cast<std::int64_t>(ucb_vector_int_size(vec)));
    ucb_vector_int_free(vec);
}

void int_std_insert_middle()
{
    std::vector<int> vec;
    vec.reserve(kBenchCount * 2);
    for (int i = 0; i < kBenchCount; ++i)
        vec.insert(vec.begin() + static_cast<std::ptrdiff_t>(vec.size() / 2), i);
    ucb_bench_sink_ptr(vec.data());
    sink(static_cast<std::int64_t>(vec.size()));
}

void int_carray_insert_middle()
{
    int arr[kBenchCount * 2];
    int size = 0;
    for (int i = 0; i < kBenchCount; ++i)
    {
        const std::size_t pos = static_cast<std::size_t>(size) / 2;
        std::memmove(arr + pos + 1,
                     arr + pos,
                     (static_cast<std::size_t>(size) - pos) * sizeof(int));
        arr[pos] = i;
        ++size;
    }
    ucb_bench_sink_ptr(arr);
    sink(size);
}

void generic_int_insert_middle()
{
    ucb_vector_args args = {0};
    args.element_size = sizeof(int);
    args.initial_capacity = kBenchCount * 2;
    ucb_vector* vec = ucb_vector_new(args);
    for (int i = 0; i < kBenchCount; ++i)
        ucb_vector_insert(vec, ucb_vector_size(vec) / 2, &i);
    ucb_bench_sink_ptr(vec);
    sink(static_cast<std::int64_t>(ucb_vector_size(vec)));
    ucb_vector_free(vec);
}

/* -------------------------------------------------------------------------- */
/*                                remove middle                               */
/* -------------------------------------------------------------------------- */

void int_remove_middle()
{
    ucb_vector_int* vec = ucb_vector_int_new();
    ucb_vector_int_reserve(vec, kBenchCount);
    for (int i = 0; i < kBenchCount; ++i)
        ucb_vector_int_push_back(vec, i);
    std::int64_t sum = 0;
    while (!ucb_vector_int_is_empty(vec))
        sum += ucb_vector_int_remove(vec, ucb_vector_int_size(vec) / 2);
    ucb_bench_sink_ptr(vec->data);
    sink(sum);
    ucb_vector_int_free(vec);
}

void int_std_remove_middle()
{
    std::vector<int> vec;
    vec.reserve(kBenchCount);
    for (int i = 0; i < kBenchCount; ++i)
        vec.push_back(i);
    std::int64_t sum = 0;
    while (!vec.empty())
    {
        const std::size_t pos = vec.size() / 2;
        sum += vec[pos];
        vec.erase(vec.begin() + static_cast<std::ptrdiff_t>(pos));
    }
    ucb_bench_sink_ptr(vec.data());
    sink(sum);
}

void int_carray_remove_middle()
{
    int arr[kBenchCount];
    int size = 0;
    for (int i = 0; i < kBenchCount; ++i)
        arr[size++] = i;
    std::int64_t sum = 0;
    while (size > 0)
    {
        const std::size_t pos = static_cast<std::size_t>(size) / 2;
        sum += arr[pos];
        std::memmove(arr + pos,
                     arr + pos + 1,
                     (static_cast<std::size_t>(size) - pos - 1) * sizeof(int));
        --size;
    }
    ucb_bench_sink_ptr(arr);
    sink(sum);
}

void generic_int_remove_middle()
{
    ucb_vector_args args = {0};
    args.element_size = sizeof(int);
    args.initial_capacity = kBenchCount;
    ucb_vector* vec = ucb_vector_new(args);
    for (int i = 0; i < kBenchCount; ++i)
        ucb_vector_push_back(vec, &i);
    std::int64_t sum = 0;
    while (!ucb_vector_is_empty(vec))
    {
        int value = 0;
        ucb_vector_remove(vec, ucb_vector_size(vec) / 2, &value);
        sum += value;
    }
    ucb_bench_sink_ptr(vec);
    sink(sum);
    ucb_vector_free(vec);
}

/* -------------------------------------------------------------------------- */
/*                              indexed read                                  */
/* -------------------------------------------------------------------------- */

void int_read()
{
    const ucb_vector_int* vec = int_source();
    const std::size_t size = ucb_vector_int_size(vec);
    std::int64_t sum = 0;
    for (std::size_t i = 0; i < size; ++i)
        sum += ucb_vector_int_get(vec, i);
    sink(sum);
}

void int_std_read()
{
    const std::vector<int>& vec = int_std_source();
    std::int64_t sum = 0;
    for (std::size_t i = 0; i < vec.size(); ++i)
        sum += vec[i];
    sink(sum);
}

void int_carray_read()
{
    const int* arr = int_carray_source();
    std::int64_t sum = 0;
    for (int i = 0; i < kBenchCount; ++i)
        sum += arr[i];
    sink(sum);
}

void generic_int_read()
{
    const ucb_vector* vec = generic_int_source();
    const std::size_t size = ucb_vector_size(vec);
    std::int64_t sum = 0;
    for (std::size_t i = 0; i < size; ++i)
    {
        int value = 0;
        ucb_vector_get(vec, i, &value);
        sum += value;
    }
    sink(sum);
}

/* -------------------------------------------------------------------------- */
/*                                   foreach                                  */
/* -------------------------------------------------------------------------- */

void int_foreach()
{
    ucb_vector_int* vec = int_source();
    std::int64_t sum = 0;
    ucb_vector_int_foreach(vec, int_iter_cb, &sum);
    sink(sum);
}

void int_std_foreach()
{
    const std::vector<int>& vec = int_std_source();
    std::int64_t sum = 0;
    for (int value : vec)
        sum += value;
    sink(sum);
}

void int_carray_foreach()
{
    const int* arr = int_carray_source();
    std::int64_t sum = 0;
    for (const int* p = arr; p != arr + kBenchCount; ++p)
        sum += *p;
    sink(sum);
}

void generic_int_foreach()
{
    ucb_vector* vec = generic_int_source();
    std::int64_t sum = 0;
    ucb_vector_foreach(vec, generic_int_iter_cb, &sum);
    sink(sum);
}

/* -------------------------------------------------------------------------- */
/*                                    copy                                    */
/* -------------------------------------------------------------------------- */

void int_copy()
{
    ucb_vector_int* dst = ucb_vector_int_clone(int_source());
    ucb_bench_sink_ptr(dst->data);
    sink(static_cast<std::int64_t>(ucb_vector_int_size(dst)));
    ucb_vector_int_free(dst);
}

void int_std_copy()
{
    const std::vector<int> dst = int_std_source();
    ucb_bench_sink_ptr(dst.data());
    sink(static_cast<std::int64_t>(dst.size()));
}

void int_carray_copy()
{
    int dst[kBenchCount];
    std::memcpy(dst, int_carray_source(), sizeof(dst));
    ucb_bench_sink_ptr(dst);
    sink(static_cast<std::int64_t>(sizeof(dst) / sizeof(dst[0])));
}

void generic_int_copy()
{
    ucb_vector* dst = ucb_vector_clone(generic_int_source());
    ucb_bench_sink_ptr(dst);
    sink(static_cast<std::int64_t>(ucb_vector_size(dst)));
    ucb_vector_free(dst);
}

/* -------------------------------------------------------------------------- */
/*                                    sort                                    */
/* -------------------------------------------------------------------------- */

void int_sort()
{
    ucb_vector_int* vec = ucb_vector_int_new();
    ucb_vector_int_reserve(vec, kBenchCount);
    for (int i = kBenchCount - 1; i >= 0; --i)
        ucb_vector_int_push_back(vec, i);
    ucb_vector_int_sort(vec);
    ucb_bench_sink_ptr(vec->data);
    sink(ucb_vector_int_get(vec, kBenchCount / 2));
    ucb_vector_int_free(vec);
}

void int_std_sort()
{
    std::vector<int> vec;
    vec.reserve(kBenchCount);
    for (int i = kBenchCount - 1; i >= 0; --i)
        vec.push_back(i);
    std::sort(vec.begin(), vec.end());
    ucb_bench_sink_ptr(vec.data());
    sink(vec[kBenchCount / 2]);
}

void int_carray_sort()
{
    int arr[kBenchCount];
    for (int i = 0; i < kBenchCount; ++i)
        arr[i] = kBenchCount - 1 - i;
    std::qsort(arr, kBenchCount, sizeof(int), cmp_int);
    ucb_bench_sink_ptr(arr);
    sink(arr[kBenchCount / 2]);
}

} // namespace

/* -------------------------------------------------------------------------- */
/*                                Registration                                */
/* -------------------------------------------------------------------------- */

void register_vector_int_benchmarks(ucb::microbench::benchmark& bench)
{
    ucb::microbench::suite& generic =
        bench.add_suite("vector-generic-int", "Generic ucb_vector, int value mode (2048 elements)");

    generic.add_test("push-back", "Reserved push_back", generic_int_push_back);
    generic.add_test("push-back-grow", "Growing push_back", generic_int_push_back_grow);
    generic.add_test("pop-back", "Push then pop_back", generic_int_pop_back);
    generic.add_test("push-front", "push_front with shifting", generic_int_push_front);
    generic.add_test("insert-middle", "Insert at the middle", generic_int_insert_middle);
    generic.add_test("remove-middle", "Remove from the middle", generic_int_remove_middle);
    generic.add_test("read", "Indexed read of all elements", generic_int_read);
    generic.add_test("foreach", "Iterate with foreach", generic_int_foreach);
    generic.add_test("copy", "Clone the vector", generic_int_copy);

    ucb::microbench::suite& suite =
        bench.add_suite("vector-int", "ucb_vector_int (2048 elements per op)");

    ucb::microbench::test& push_back =
        suite.add_test("push-back", "Reserved push_back", int_push_back);
    push_back.add_ref(int_std_push_back, "std-vector");
    push_back.add_ref(int_carray_push_back, "c-array");
    push_back.add_ref("vector-generic-int::push-back");

    ucb::microbench::test& push_back_grow =
        suite.add_test("push-back-grow", "Growing push_back", int_push_back_grow);
    push_back_grow.add_ref(int_std_push_back_grow, "std-vector");
    push_back_grow.add_ref(int_carray_push_back_grow, "c-array");
    push_back_grow.add_ref("vector-generic-int::push-back-grow");

    ucb::microbench::test& pop_back =
        suite.add_test("pop-back", "Push then pop_back", int_pop_back);
    pop_back.add_ref(int_std_pop_back, "std-vector");
    pop_back.add_ref(int_carray_pop_back, "c-array");
    pop_back.add_ref("vector-generic-int::pop-back");

    ucb::microbench::test& push_front =
        suite.add_test("push-front", "push_front with shifting", int_push_front);
    push_front.add_ref(int_std_push_front, "std-vector");
    push_front.add_ref(int_carray_push_front, "c-array");
    push_front.add_ref("vector-generic-int::push-front");

    ucb::microbench::test& insert_middle =
        suite.add_test("insert-middle", "Insert at the middle", int_insert_middle);
    insert_middle.add_ref(int_std_insert_middle, "std-vector");
    insert_middle.add_ref(int_carray_insert_middle, "c-array");
    insert_middle.add_ref("vector-generic-int::insert-middle");

    ucb::microbench::test& remove_middle =
        suite.add_test("remove-middle", "Remove from the middle", int_remove_middle);
    remove_middle.add_ref(int_std_remove_middle, "std-vector");
    remove_middle.add_ref(int_carray_remove_middle, "c-array");
    remove_middle.add_ref("vector-generic-int::remove-middle");

    ucb::microbench::test& read = suite.add_test("read", "Indexed read of all elements", int_read);
    read.add_ref(int_std_read, "std-vector");
    read.add_ref(int_carray_read, "c-array");
    read.add_ref("vector-generic-int::read");

    ucb::microbench::test& foreach = suite.add_test("foreach", "Iterate with foreach", int_foreach);
    foreach
        .add_ref(int_std_foreach, "std-vector");
    foreach
        .add_ref(int_carray_foreach, "c-array");
    foreach
        .add_ref("vector-generic-int::foreach");

    ucb::microbench::test& copy = suite.add_test("copy", "Clone the vector", int_copy);
    copy.add_ref(int_std_copy, "std-vector");
    copy.add_ref(int_carray_copy, "c-array");
    copy.add_ref("vector-generic-int::copy");

    ucb::microbench::test& sort = suite.add_test("sort", "Sort the vector", int_sort);
    sort.add_ref(int_std_sort, "std-vector");
    sort.add_ref(int_carray_sort, "c-array");
}
