/**
 * @file vector.cpp
 * This file is part of the UCB project
 * - SPDX-FileCopyrightText: © 2026 Åke Svedin <ake@svedin.org>
 * - SPDX-License-Identifier: MIT
 *
 * @brief Vector tests
 */

#include "ucb/container/vector.h"

#include "my_type.h"

#include "ucb/container/vector_generic.h"
#include "ucb/memory.h"
#include "ucb/string.h"

#include <doctest.h>

#include <iostream>

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

static int int_cmp(const void* a, const void* b)
{
    int int_a = *reinterpret_cast<const int*>(a);
    int int_b = *reinterpret_cast<const int*>(b);
    return (int_a > int_b) - (int_a < int_b);
}

struct VectorFixture
{
    ucb_vector* vec_owned;
    ucb_vector* vec_shared;
    VectorFixture()
    {
        ucb_vector_args args = {0};
        vec_shared = ucb_vector_new(args);

        args.data_clone = int_clone;
        args.data_free = int_free;
        args.data_cmp = int_cmp;
        args.element_size = sizeof(void*);
        vec_owned = ucb_vector_new(args);
    }
    ~VectorFixture()
    {
        ucb_vector_free(vec_owned);
        ucb_vector_free(vec_shared);
        vec_owned = nullptr;
        vec_shared = nullptr;
    }
};

/* -------------------------------------------------------------------------- */
/*                                    Tests                                   */
/* -------------------------------------------------------------------------- */

TEST_CASE("vector - basics")
{
    // Basic lifetime. No fixture involved.

    ucb_vector* vec = UCB_NULL;
    ucb_vector* vec2 = UCB_NULL;

    int ival1 = 1001;
    int ival2 = 1002;
    int ival3 = 1003;
    int out;

    ucb_vector_args int_args = {0};
    // int_args.data_clone = int_clone;
    // int_args.data_free = int_free;
    int_args.data_cmp = int_cmp;
    int_args.element_size = sizeof(int);

    vec = ucb_vector_new({0});
    REQUIRE(vec != nullptr);
    REQUIRE(ucb_vector_is_empty(vec));
    ucb_vector_free(vec);

    vec = ucb_vector_new(int_args);

    REQUIRE(vec != nullptr);
    ucb_vector_push_back(vec, &ival1);
    ucb_vector_push_back(vec, &ival2);
    ucb_vector_push_front(vec, &ival3);

    REQUIRE(ucb_vector_size(vec) == 3);

    ucb_vector_get(vec, 0, &out);
    REQUIRE(out == ival3);
    ucb_vector_get(vec, 1, &out);
    REQUIRE(out == ival1);
    ucb_vector_get(vec, 2, &out);
    REQUIRE(out == ival2);

    vec2 = ucb_vector_clone(vec);
    REQUIRE(vec2 != nullptr);
    REQUIRE(ucb_vector_size(vec2) == 3);

    ucb_vector_clear(vec);
    REQUIRE(ucb_vector_is_empty(vec));
    REQUIRE(ucb_vector_size(vec2) == 3);

    ucb_vector_free(vec);

    ucb_vector_get(vec2, 0, &out);
    REQUIRE(out == ival3);
    ucb_vector_get(vec2, 1, &out);
    REQUIRE(out == ival1);
    ucb_vector_get(vec2, 2, &out);
    REQUIRE(out == ival2);

    ucb_vector_remove(vec2, 1, &out);
    REQUIRE(out == ival1);
    REQUIRE(ucb_vector_size(vec2) == 2);
    ucb_vector_get(vec2, 0, &out);
    REQUIRE(out == ival3);
    ucb_vector_get(vec2, 1, &out);
    REQUIRE(out == ival2);

    ucb_vector_free(vec2);
}

TEST_CASE("vector - custom element")
{
    MyType item1 = {1, "one"};
    MyType item2 = {2, "two"};
    MyType item3 = {3, "three"};
    MyType out = {0};

    ucb_vector_args args = {0};
    // args.data_clone = (ucb_clone_func)mytype_clone;
    // args.data_free = (ucb_free_func)mytype_free;
    args.data_cmp = (ucb_cmp_func)mytype_cmp;
    args.element_size = sizeof(MyType);
    args.initial_capacity = 2;

    ucb_vector* vec = ucb_vector_new(args);
    REQUIRE(vec != nullptr);
    ucb_vector_push_back(vec, &item2);
    ucb_vector_push_back(vec, &item3);
    ucb_vector_push_front(vec, &item1);

    REQUIRE(ucb_vector_size(vec) == 3);
    ucb_vector_get(vec, 0, &out);
    REQUIRE(mytype_cmp(&out, &item1) == 0);
    ucb_vector_get(vec, 1, &out);
    REQUIRE(mytype_cmp(&out, &item2) == 0);
    ucb_vector_get(vec, 2, &out);
    REQUIRE(mytype_cmp(&out, &item3) == 0);

    ucb_vector_remove(vec, 1, &out);
    REQUIRE(mytype_cmp(&out, &item2) == 0);
    REQUIRE(ucb_vector_size(vec) == 2);
    ucb_vector_get(vec, 0, &out);
    REQUIRE(mytype_cmp(&out, &item1) == 0);
    ucb_vector_get(vec, 1, &out);
    REQUIRE(mytype_cmp(&out, &item3) == 0);

    ucb_vector_free(vec);
}

TEST_CASE("vector - owning pointer")
{
    MyType item1 = {1, "one"};
    MyType item2 = {2, "two"};
    MyType item3 = {3, "three"};
    MyType* out = UCB_NULL;

    ucb_vector_args args = {0};
    args.data_clone = (ucb_clone_func)mytype_clone;
    args.data_free = (ucb_free_func)mytype_free;
    args.data_cmp = (ucb_cmp_func)mytype_cmp;
    // args.element_size = sizeof(MyType);
    args.initial_capacity = 2;

    ucb_vector* vec = ucb_vector_new(args);
    REQUIRE(vec != nullptr);
    ucb_vector_push_back(vec, &item2);
    ucb_vector_push_back(vec, &item3);
    ucb_vector_push_front(vec, &item1);

    REQUIRE(ucb_vector_size(vec) == 3);
    ucb_vector_get(vec, 0, &out);
    REQUIRE(mytype_cmp(out, &item1) == 0);
    ucb_vector_get(vec, 1, &out);
    REQUIRE(mytype_cmp(out, &item2) == 0);
    ucb_vector_get(vec, 2, &out);
    REQUIRE(mytype_cmp(out, &item3) == 0);

    // Free, with out_data. We are responsible for freeing this item.
    ucb_vector_remove(vec, 1, &out);
    REQUIRE(mytype_cmp(out, &item2) == 0);
    mytype_free(out);
    out = UCB_NULL;

    REQUIRE(ucb_vector_size(vec) == 2);
    ucb_vector_get(vec, 0, &out);
    REQUIRE(mytype_cmp(out, &item1) == 0);
    ucb_vector_get(vec, 1, &out);
    REQUIRE(mytype_cmp(out, &item3) == 0);

    ucb_vector_free(vec);
}

TEST_CASE("vector - non-owning pointer")
{
    MyType item1 = {1, "one"};
    MyType item2 = {2, "two"};
    MyType item3 = {3, "three"};
    MyType* out = UCB_NULL;

    ucb_vector_args args = {0};
    // No clone, no free: non-owning pointer mode (element_size == 0)
    args.data_cmp = (ucb_cmp_func)mytype_cmp;
    args.initial_capacity = 2;

    ucb_vector* vec = ucb_vector_new(args);
    REQUIRE(vec != nullptr);
    ucb_vector_push_back(vec, &item2);
    ucb_vector_push_back(vec, &item3);
    ucb_vector_push_front(vec, &item1);

    REQUIRE(ucb_vector_size(vec) == 3);

    // get returns the original pointer — no copy, no clone
    ucb_vector_get(vec, 0, &out);
    REQUIRE(out == &item1);
    ucb_vector_get(vec, 1, &out);
    REQUIRE(out == &item2);
    ucb_vector_get(vec, 2, &out);
    REQUIRE(out == &item3);

    // remove hands the pointer back; vector does not free anything
    ucb_vector_remove(vec, 1, &out);
    REQUIRE(out == &item2);
    REQUIRE(ucb_vector_size(vec) == 2);

    ucb_vector_get(vec, 0, &out);
    REQUIRE(out == &item1);
    ucb_vector_get(vec, 1, &out);
    REQUIRE(out == &item3);

    // free does not call any destructor on the stored pointers
    ucb_vector_free(vec);
}

TEST_CASE("vector - pop and peek")
{
    int ival1 = 10, ival2 = 20, ival3 = 30;
    int out = 0;
    bool ok;

    ucb_vector_args args = {0};
    args.element_size = sizeof(int);

    ucb_vector* vec = ucb_vector_new(args);
    REQUIRE(vec != nullptr);

    ucb_vector_push_back(vec, &ival1);
    ucb_vector_push_back(vec, &ival2);
    ucb_vector_push_back(vec, &ival3);

    // peek does not remove
    ok = ucb_vector_peek_back(vec, &out);
    REQUIRE(ok);
    REQUIRE(out == ival3);
    REQUIRE(ucb_vector_size(vec) == 3);

    ok = ucb_vector_peek_front(vec, &out);
    REQUIRE(ok);
    REQUIRE(out == ival1);
    REQUIRE(ucb_vector_size(vec) == 3);

    // pop removes
    ok = ucb_vector_pop_back(vec, &out);
    REQUIRE(ok);
    REQUIRE(out == ival3);
    REQUIRE(ucb_vector_size(vec) == 2);

    ok = ucb_vector_pop_front(vec, &out);
    REQUIRE(ok);
    REQUIRE(out == ival1);
    REQUIRE(ucb_vector_size(vec) == 1);

    // only ival2 remains
    ok = ucb_vector_peek_front(vec, &out);
    REQUIRE(ok);
    REQUIRE(out == ival2);

    ucb_vector_clear(vec);

    // pop/peek on empty return false
    ok = ucb_vector_pop_back(vec, &out);
    REQUIRE(!ok);
    ok = ucb_vector_peek_front(vec, &out);
    REQUIRE(!ok);

    ucb_vector_free(vec);
}

TEST_CASE("vector - set")
{
    MyType item1 = {1, "one"};
    MyType item2 = {2, "two"};
    MyType item3 = {3, "three"};
    MyType replacement = {99, "ninety-nine"};
    MyType out = {0};

    // Value mode: set overwrites in place
    ucb_vector_args args = {0};
    args.data_cmp = (ucb_cmp_func)mytype_cmp;
    args.element_size = sizeof(MyType);

    ucb_vector* vec = ucb_vector_new(args);
    REQUIRE(vec != nullptr);
    ucb_vector_push_back(vec, &item1);
    ucb_vector_push_back(vec, &item2);
    ucb_vector_push_back(vec, &item3);

    ucb_vector_set(vec, 1, &replacement);
    REQUIRE(ucb_vector_size(vec) == 3);

    ucb_vector_get(vec, 0, &out);
    REQUIRE(mytype_cmp(&out, &item1) == 0);
    ucb_vector_get(vec, 1, &out);
    REQUIRE(mytype_cmp(&out, &replacement) == 0);
    ucb_vector_get(vec, 2, &out);
    REQUIRE(mytype_cmp(&out, &item3) == 0);

    ucb_vector_free(vec);

    // Owned pointer mode: set clones the new item and frees the old one
    MyType* pout = UCB_NULL;
    ucb_vector_args oargs = {0};
    oargs.data_clone = (ucb_clone_func)mytype_clone;
    oargs.data_free = (ucb_free_func)mytype_free;
    oargs.data_cmp = (ucb_cmp_func)mytype_cmp;

    ucb_vector* ovec = ucb_vector_new(oargs);
    REQUIRE(ovec != nullptr);
    ucb_vector_push_back(ovec, &item1);
    ucb_vector_push_back(ovec, &item2);

    ucb_vector_set(ovec, 0, &replacement); // frees cloned item1, clones replacement
    REQUIRE(ucb_vector_size(ovec) == 2);

    ucb_vector_get(ovec, 0, &pout);
    REQUIRE(mytype_cmp(pout, &replacement) == 0);
    ucb_vector_get(ovec, 1, &pout);
    REQUIRE(mytype_cmp(pout, &item2) == 0);

    ucb_vector_free(ovec); // frees remaining cloned items
}

TEST_CASE("vector - copy and move")
{
    int ival1 = 1, ival2 = 2, ival3 = 3;
    int out = 0;

    ucb_vector_args args = {0};
    args.element_size = sizeof(int);

    ucb_vector* src = ucb_vector_new(args);
    REQUIRE(src != nullptr);
    ucb_vector_push_back(src, &ival1);
    ucb_vector_push_back(src, &ival2);
    ucb_vector_push_back(src, &ival3);

    // copy: dst is independent of src
    ucb_vector* dst = ucb_vector_new(args);
    REQUIRE(dst != nullptr);
    bool ok = ucb_vector_copy(dst, src);
    REQUIRE(ok);
    REQUIRE(ucb_vector_size(dst) == 3);

    ucb_vector_clear(src);
    REQUIRE(ucb_vector_is_empty(src));
    REQUIRE(ucb_vector_size(dst) == 3); // dst unaffected

    ucb_vector_get(dst, 0, &out);
    REQUIRE(out == ival1);
    ucb_vector_get(dst, 1, &out);
    REQUIRE(out == ival2);
    ucb_vector_get(dst, 2, &out);
    REQUIRE(out == ival3);

    // move: src becomes empty, dst takes the data
    ucb_vector_push_back(src, &ival1);
    ucb_vector_push_back(src, &ival2);
    REQUIRE(ucb_vector_size(src) == 2);

    ucb_vector_move(dst, src);
    REQUIRE(ucb_vector_is_empty(src));
    REQUIRE(ucb_vector_size(dst) == 2);

    ucb_vector_get(dst, 0, &out);
    REQUIRE(out == ival1);
    ucb_vector_get(dst, 1, &out);
    REQUIRE(out == ival2);

    ucb_vector_free(src);
    ucb_vector_free(dst);

    // copy in owned pointer mode (exercises the bug-fixed path)
    MyType item1 = {1, "one"};
    MyType item2 = {2, "two"};
    MyType* pout = UCB_NULL;

    ucb_vector_args oargs = {0};
    oargs.data_clone = (ucb_clone_func)mytype_clone;
    oargs.data_free = (ucb_free_func)mytype_free;
    oargs.data_cmp = (ucb_cmp_func)mytype_cmp;

    ucb_vector* osrc = ucb_vector_new(oargs);
    ucb_vector_push_back(osrc, &item1);
    ucb_vector_push_back(osrc, &item2);

    ucb_vector* odst = ucb_vector_clone(osrc); // also exercises copy internally
    REQUIRE(odst != nullptr);
    REQUIRE(ucb_vector_size(odst) == 2);

    // verify cloned items are deep copies (different pointers, same value)
    ucb_vector_get(osrc, 0, &pout);
    MyType* psrc0 = pout;
    ucb_vector_get(odst, 0, &pout);
    REQUIRE(pout != psrc0); // different allocations
    REQUIRE(mytype_cmp(pout, &item1) == 0);

    ucb_vector_free(osrc);
    ucb_vector_free(odst);
}

static inline bool check_sort_int(int* pval, size_t index, void* user_data)
{
    int* last = (int*)user_data;
    std::cout << index << ": " << *pval << ", last: " << *last << std::endl;
    if (index > 0)
    {
        if (*last > *pval)
            return false;
    }
    *last = *pval;
    return true;
}

static inline bool check_sort_str(ucb_str* pval, size_t index, void* user_data)
{
    ucb_str* last = (ucb_str*)user_data;
    std::cout << index << ": " << ucb_str_cstr(pval) << ", last: " << ucb_str_cstr(last)
              << std::endl;
    if (index > 0)
    {
        if (ucb_str_comp(last, pval) > 0)
            return false;
    }
    ucb_str_copy(last, pval);
    return true;
}

TEST_CASE("vector - sort and find")
{
    size_t pos;

    ucb_vector_int* vint = ucb_vector_int_new();
    ucb_vector_int_push_back(vint, 4);
    ucb_vector_int_push_back(vint, 1);
    ucb_vector_int_push_back(vint, 6);
    ucb_vector_int_push_back(vint, 2);
    ucb_vector_int_push_back(vint, 3);

    ucb_vector_int_sort(vint);

    int tint = -1;
    REQUIRE(ucb_vector_int_foreach(vint, check_sort_int, &tint) == 5);

    tint = 5;
    pos = ucb_vector_int_find(vint, &tint);
    REQUIRE(pos == -(4 + 1));

    pos = ucb_vector_int_insert_sorted(vint, 5);
    REQUIRE(pos == 4);

    tint = -1;
    REQUIRE(ucb_vector_int_foreach(vint, check_sort_int, &tint) == 6);

    tint = 4;
    pos = ucb_vector_int_find(vint, &tint);
    REQUIRE(pos == 3);

    ucb_vector_int_free(vint);

    // String

    ucb_vector_str* vstr = ucb_vector_str_new();
    ucb_vector_str_push_back(vstr, ucb_str_new_c("4"));
    ucb_vector_str_push_back(vstr, ucb_str_new_c("1"));
    ucb_vector_str_push_back(vstr, ucb_str_new_c("6"));
    ucb_vector_str_push_back(vstr, ucb_str_new_c("2"));
    ucb_vector_str_push_back(vstr, ucb_str_new_c("3"));

    ucb_vector_str_sort(vstr);

    ucb_str tstr = ucb_str_make();
    REQUIRE(ucb_vector_str_foreach(vstr, check_sort_str, &tstr) == 5);

    ucb_str_assign_c(&tstr, "5");
    pos = ucb_vector_str_find(vstr, &tstr);
    REQUIRE(pos == -(4 + 1));

    pos = ucb_vector_str_insert_sorted(vstr, ucb_str_new_c("5"));
    REQUIRE(pos == 4);

    ucb_str_assign_c(&tstr, "");
    REQUIRE(ucb_vector_str_foreach(vstr, check_sort_str, &tstr) == 6);

    ucb_str_assign_c(&tstr, "4");
    pos = ucb_vector_str_find(vstr, &tstr);
    REQUIRE(pos == 3);

    ucb_str_release(&tstr);
    ucb_vector_str_free_full(vstr);
}

struct ForeachStop
{
    size_t stop_at;
    size_t calls;
};

static bool foreach_stop_int(int* pval, size_t index, void* user_data)
{
    (void)pval;
    ForeachStop* state = static_cast<ForeachStop*>(user_data);
    state->calls++;
    return index != state->stop_at;
}

TEST_CASE("vector - typed")
{
    // float
    ucb_vector_flt* vflt = ucb_vector_flt_new();
    REQUIRE(vflt != nullptr);
    ucb_vector_flt_push_back(vflt, 4.0f);
    ucb_vector_flt_push_back(vflt, 1.5f);
    ucb_vector_flt_push_back(vflt, 6.0f);
    ucb_vector_flt_push_back(vflt, 2.0f);
    ucb_vector_flt_push_back(vflt, 3.0f);

    ucb_vector_flt_sort(vflt);
    REQUIRE(ucb_vector_flt_size(vflt) == 5);
    CHECK(ucb_vector_flt_get(vflt, 0) == 1.5f);
    CHECK(ucb_vector_flt_get(vflt, 1) == 2.0f);
    CHECK(ucb_vector_flt_get(vflt, 4) == 6.0f);

    float f5 = 5.0f;
    CHECK(ucb_vector_flt_find(vflt, &f5) == -(4 + 1));
    CHECK(ucb_vector_flt_insert_sorted(vflt, 5.0f) == 4);
    float f4 = 4.0f;
    CHECK(ucb_vector_flt_find(vflt, &f4) == 3);

    ucb_vector_flt_free(vflt);

    // double
    ucb_vector_dbl* vdbl = ucb_vector_dbl_new();
    REQUIRE(vdbl != nullptr);
    ucb_vector_dbl_push_back(vdbl, 4.0);
    ucb_vector_dbl_push_back(vdbl, 1.5);
    ucb_vector_dbl_push_back(vdbl, 6.0);
    ucb_vector_dbl_push_back(vdbl, 2.0);
    ucb_vector_dbl_push_back(vdbl, 3.0);

    ucb_vector_dbl_sort(vdbl);
    REQUIRE(ucb_vector_dbl_size(vdbl) == 5);
    CHECK(ucb_vector_dbl_get(vdbl, 0) == 1.5);
    CHECK(ucb_vector_dbl_get(vdbl, 4) == 6.0);

    double d5 = 5.0;
    CHECK(ucb_vector_dbl_find(vdbl, &d5) == -(4 + 1));
    CHECK(ucb_vector_dbl_insert_sorted(vdbl, 5.0) == 4);
    double d4 = 4.0;
    CHECK(ucb_vector_dbl_find(vdbl, &d4) == 3);

    ucb_vector_dbl_free(vdbl);

    // pointer (ordered by address via the default pointer comparator)
    int arr[5] = {0};
    ucb_vector_ptr* vptr = ucb_vector_ptr_new();
    REQUIRE(vptr != nullptr);
    for (int i = 4; i >= 0; i--)
        ucb_vector_ptr_push_back(vptr, &arr[i]);

    ucb_vector_ptr_sort(vptr);
    REQUIRE(ucb_vector_ptr_size(vptr) == 5);
    for (int i = 0; i < 5; i++)
        CHECK(ucb_vector_ptr_get(vptr, (size_t)i) == &arr[i]);

    CHECK(ucb_vector_ptr_find(vptr, &arr[2]) == 2);

    ucb_vector_ptr_free(vptr);
}

TEST_CASE("vector - capacity and iteration")
{
    ucb_vector_int* vec = ucb_vector_int_new();
    REQUIRE(vec != nullptr);
    CHECK(ucb_vector_int_is_empty(vec));
    CHECK(ucb_vector_int_capacity(vec) == 0);

    REQUIRE(ucb_vector_int_reserve(vec, 8));
    CHECK(ucb_vector_int_capacity(vec) == 8);

    // Reserve below current capacity is a no-op
    REQUIRE(ucb_vector_int_reserve(vec, 4));
    CHECK(ucb_vector_int_capacity(vec) == 8);

    ucb_vector_int_push_back(vec, 10);
    ucb_vector_int_push_back(vec, 20);
    ucb_vector_int_push_back(vec, 30);
    REQUIRE(ucb_vector_int_size(vec) == 3);

    // insert in the middle shifts existing elements
    ucb_vector_int_insert(vec, 1, 15);
    REQUIRE(ucb_vector_int_size(vec) == 4);
    CHECK(ucb_vector_int_get(vec, 0) == 10);
    CHECK(ucb_vector_int_get(vec, 1) == 15);
    CHECK(ucb_vector_int_get(vec, 2) == 20);
    CHECK(ucb_vector_int_get(vec, 3) == 30);

    // swap two elements
    ucb_vector_int_swap(vec, 0, 3);
    CHECK(ucb_vector_int_get(vec, 0) == 30);
    CHECK(ucb_vector_int_get(vec, 3) == 10);

    // fit shrinks capacity to the used size
    ucb_vector_int_fit(vec);
    CHECK(ucb_vector_int_capacity(vec) == 4);
    CHECK(ucb_vector_int_capacity(vec) == ucb_vector_int_size(vec));

    // foreach stops at the requested index and returns that index
    ForeachStop state = {2, 0};
    size_t processed = ucb_vector_int_foreach(vec, foreach_stop_int, &state);
    CHECK(processed == 2);
    CHECK(state.calls == 3);

    // foreach without stopping returns the number of elements
    state = {99, 0};
    processed = ucb_vector_int_foreach(vec, foreach_stop_int, &state);
    CHECK(processed == 4);
    CHECK(state.calls == 4);

    ucb_vector_int_free(vec);
}

TEST_CASE("vector - owned pop frees")
{
    MyType item1 = {1, "one"};
    MyType item2 = {2, "two"};
    MyType item3 = {3, "three"};

    ucb_vector_args args = {0};
    args.data_clone = (ucb_clone_func)mytype_clone;
    args.data_free = (ucb_free_func)mytype_free;
    args.data_cmp = (ucb_cmp_func)mytype_cmp;

    ucb_vector* vec = ucb_vector_new(args);
    REQUIRE(vec != nullptr);
    ucb_vector_push_back(vec, &item1);
    ucb_vector_push_back(vec, &item2);
    ucb_vector_push_back(vec, &item3);

    // Passing UCB_NULL as out_data makes the vector free the popped element
    REQUIRE(ucb_vector_pop_back(vec, UCB_NULL));
    REQUIRE(ucb_vector_pop_front(vec, UCB_NULL));
    REQUIRE(ucb_vector_size(vec) == 1);

    MyType* out = UCB_NULL;
    ucb_vector_get(vec, 0, &out);
    REQUIRE(mytype_cmp(out, &item2) == 0);

    ucb_vector_free(vec); // frees the remaining element
}
