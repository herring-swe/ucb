#include "test_vector.h"

#include "ucb/cast.h"
#include "ucb/container/vector.h"
#include "ucb/container/vector_generic.h"

#include <stdlib.h>

static bool test_ucb_vector1_iter_func(void* pval, size_t idx, void* user_data)
{
    UCB_UNUSED(idx);
    long long* sum = (long long*)user_data;
    *sum += *(int*)pval;
    return true;
}

static bool test_ucb_vector_ptr_func(void* pval, size_t idx, void* user_data)
{
    UCB_UNUSED(idx);
    long long* sum = (long long*)user_data;
    *sum += ucb_ptr2int(pval);
    return true;
}

static bool test_ucb_vector_int_iter_func(int* pval, size_t idx, void* user_data)
{
    UCB_UNUSED(idx);
    long long* sum = (long long*)user_data;
    *sum += *pval;
    return true;
};

int64_t test_carr_direct(int iterations, int n_elem)
{
    int* arr = malloc(sizeof(int) * n_elem);
    int arr_size = 0;
    int64_t sum = 0;

    for (int it = 0; it < iterations; ++it)
    {
        for (int i = 0; i < n_elem; ++i)
            arr[arr_size++] = i;
        for (int i = 0; i < arr_size; ++i)
            sum += arr[i];
        arr_size = 0;
    }
    free(arr);
    return sum;
}

int64_t test_carr_iter(int iterations, int n_elem)
{
    int* arr = malloc(sizeof(int) * n_elem);
    int arr_size = 0;
    int64_t sum = 0;

    for (int it = 0; it < iterations; ++it)
    {
        for (int i = 0; i < n_elem; ++i)
            arr[arr_size++] = i;
        for (const int* p = arr; p < arr + arr_size; ++p)
            sum += *p;
        arr_size = 0;
    }
    free(arr);
    return sum;
}

int64_t test_vector1_direct(int iterations, int n_elem)
{
    ucb_vector_args args = {0};
    args.element_size = sizeof(int);
    args.initial_capacity = n_elem;
    ucb_vector* vec = ucb_vector_new(args);
    int64_t sum = 0;

    for (int it = 0; it < iterations; ++it)
    {
        for (int i = 0; i < n_elem; ++i)
            ucb_vector_push_back(vec, &i);
        for (size_t i = 0; i < ucb_vector_size(vec); ++i)
        {
            int val = 0;
            ucb_vector_get(vec, i, &val);
            sum += val;
        }
        ucb_vector_clear(vec);
    }
    ucb_vector_free(vec);
    return sum;
}

int64_t test_vector1_foreach(int iterations, int n_elem)
{
    ucb_vector_args args = {0};
    args.element_size = sizeof(int);
    args.initial_capacity = n_elem;
    ucb_vector* vec = ucb_vector_new(args);
    int64_t sum = 0;

    for (int it = 0; it < iterations; ++it)
    {
        for (int i = 0; i < n_elem; ++i)
            ucb_vector_push_back(vec, &i);
        ucb_vector_foreach(vec, test_ucb_vector1_iter_func, &sum);
        ucb_vector_clear(vec);
    }
    ucb_vector_free(vec);
    return sum;
}

int64_t test_vector_ptr_direct(int iterations, int n_elem)
{
    ucb_vector_ptr* vec = ucb_vector_ptr_new();
    ucb_vector_ptr_reserve(vec, n_elem);
    int64_t sum = 0;

    for (int it = 0; it < iterations; ++it)
    {
        for (int i = 0; i < n_elem; ++i)
            ucb_vector_ptr_push_back(vec, ucb_int2ptr(i));
        for (size_t i = 0; i < ucb_vector_ptr_size(vec); ++i)
        {
            int val = 0;
            val = ucb_ptr2int(ucb_vector_ptr_get(vec, i));
            sum += val;
        }
        ucb_vector_ptr_clear(vec);
    }
    ucb_vector_ptr_free(vec);
    return sum;
}

int64_t test_vector_ptr_foreach(int iterations, int n_elem)
{
    ucb_vector_ptr* vec = ucb_vector_ptr_new();
    ucb_vector_ptr_reserve(vec, n_elem);
    int64_t sum = 0;

    for (int it = 0; it < iterations; ++it)
    {
        for (int i = 0; i < n_elem; ++i)
            ucb_vector_ptr_push_back(vec, ucb_int2ptr(i));
        ucb_vector_ptr_foreach(vec, test_ucb_vector_ptr_func, &sum);
        ucb_vector_ptr_clear(vec);
    }
    ucb_vector_ptr_free(vec);
    return sum;
}

int64_t test_vector_int_direct(int iterations, int n_elem)
{
    ucb_vector_int* vec = ucb_vector_int_new();
    ucb_vector_int_reserve(vec, n_elem);
    int64_t sum = 0;

    for (int it = 0; it < iterations; ++it)
    {
        for (int i = 0; i < n_elem; ++i)
            ucb_vector_int_push_back(vec, i);
        for (size_t i = 0; i < ucb_vector_int_size(vec); ++i)
        {
            int val = 0;
            val = ucb_vector_int_get(vec, i);
            sum += val;
        }
        ucb_vector_int_clear(vec);
    }
    ucb_vector_int_free(vec);
    return sum;
}

int64_t test_vector_int_foreach(int iterations, int n_elem)
{
    ucb_vector_int* vec = ucb_vector_int_new();
    ucb_vector_int_reserve(vec, n_elem);
    int64_t sum = 0;

    for (int it = 0; it < iterations; ++it)
    {
        for (int i = 0; i < n_elem; ++i)
            ucb_vector_int_push_back(vec, i);
        ucb_vector_int_foreach(vec, test_ucb_vector_int_iter_func, &sum);
        ucb_vector_int_clear(vec);
    }
    ucb_vector_int_free(vec);
    return sum;
}
