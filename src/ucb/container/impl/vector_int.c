#include <ucb/container/impl/vector_int.h>
#include <ucb/error.h>
#include <ucb/memory.h>

#include <stdlib.h>
#include <string.h>

ucb_vector_int* ucb_vector_int_new(void)
{
    return ucb_calloc_type(1, ucb_vector_int);
}

void ucb_vector_int_free(ucb_vector_int* vec)
{
    if (vec)
    {
        ucb_vector_int_clear(vec);
        ucb_free(vec->data);
        ucb_free(vec);
    }
}

bool ucb_vector_int_copy(ucb_vector_int* dst, const ucb_vector_int* src)
{
    UCB_VERIFY_ARGS(dst && src && dst != src);

    ucb_vector_int_clear(dst);

    if (!ucb_vector_int_reserve(dst, src->capacity))
    {
        ucb_vector_int_clear(dst);
        return false;
    }

    dst->size = src->size;

    memcpy(dst->data, src->data, src->size * sizeof(int));

    return true;
}

ucb_vector_int* ucb_vector_int_clone(const ucb_vector_int* src)
{
    UCB_VERIFY_ARGS(src);

    ucb_vector_int* dst = ucb_calloc_type(1, ucb_vector_int);
    if (dst)
    {
        if (!ucb_vector_int_copy(dst, src))
        {
            ucb_free(dst);
            return UCB_NULL;
        }
    }
    return dst;
}

void ucb_vector_int_move(ucb_vector_int* dst, ucb_vector_int* src)
{
    UCB_VERIFY_ARGS(dst && src && dst != src);

    ucb_vector_int_clear(dst);
    ucb_free(dst->data);

    dst->data = src->data;
    dst->size = src->size;
    dst->capacity = src->capacity;

    src->data = UCB_NULL;
    src->size = 0;
    src->capacity = 0;
}

bool ucb_vector_int_reserve(ucb_vector_int* vec, size_t new_capacity)
{
    UCB_VERIFY_ARGS(vec);
    if (new_capacity == 0)
        new_capacity = 8;
    if (new_capacity <= vec->capacity)
        return true;

    int* new_data = ucb_realloc_type(vec->data, new_capacity, int);
    if (!new_data)
        return false;

    vec->data = new_data;
    vec->capacity = new_capacity;
    return true;
}

void ucb_vector_int_fit(ucb_vector_int* vec)
{
    UCB_VERIFY_ARGS(vec);
    if (vec->capacity > vec->size)
    {
        int* new_data = ucb_realloc_type(vec->data, vec->size, int);
        if (new_data)
        {
            vec->data = new_data;
            vec->capacity = vec->size;
        }
    }
}

void ucb_vector_int_insert(ucb_vector_int* vec, size_t index, int data)
{
    UCB_VERIFY_ARGS(vec && index <= vec->size);
    if (vec->size == vec->capacity)
    {
        if (!ucb_vector_int_reserve(vec, vec->capacity * 2))
            return;
    }
    if (index < vec->size)
    {
        memmove(vec->data + (index + 1), vec->data + index, (vec->size - index) * sizeof(int));
    }
    vec->data[index] = data;
    vec->size++;
}

int ucb_vector_int_remove(ucb_vector_int* vec, size_t index)
{
    UCB_VERIFY_ARGS(vec && index < vec->size);
    UCB_VERIFY_MSG(vec->size > 0, UCB_ERROR_INVALID_ARG, "Cannot remove from empty container");
    int data = vec->data[index];
    if (index < vec->size - 1)
    {
        memmove(vec->data + index, vec->data + index + 1, (vec->size - index - 1) * sizeof(int));
    }
    vec->size--;
    return data;
}

void ucb_vector_int_push_back(ucb_vector_int* vec, int data)
{
    UCB_VERIFY_ARGS(vec);
    if (vec->size < vec->capacity)
    {
        vec->data[vec->size] = data;
        vec->size++;
        return;
    }
    ucb_vector_int_insert(vec, vec->size, data);
}

void ucb_vector_int_push_front(ucb_vector_int* vec, int data)
{
    UCB_VERIFY_ARGS(vec);
    if (vec->size == 0 && vec->capacity > 0)
    {
        vec->data[0] = data;
        vec->size++;
        return;
    }
    ucb_vector_int_insert(vec, 0, data);
}

int ucb_vector_int_pop_back(ucb_vector_int* vec)
{
    UCB_VERIFY_ARGS(vec && vec->size);
    UCB_VERIFY_MSG(vec->size > 0, UCB_ERROR_INVALID_ARG, "Cannot remove from empty container");
    vec->size--;
    return vec->data[vec->size];
}

int ucb_vector_int_pop_front(ucb_vector_int* vec)
{
    UCB_VERIFY_ARGS(vec && vec->size);
    UCB_VERIFY_MSG(vec->size > 0, UCB_ERROR_INVALID_ARG, "Cannot remove from empty container");
    vec->size--;
    int ret = vec->data[0];
    if (vec->size > 0)
    {
        memmove(vec->data, vec->data + 1, vec->size * sizeof(int));
    }
    return ret;
}

size_t ucb_vector_int_foreach(ucb_vector_int* vec, ucb_vector_int_iter_func func, void* user_data)
{
    UCB_VERIFY_ARGS(vec && func);
    size_t i = 0;
    for (; i < vec->size; ++i)
    {
        if (!func(&vec->data[i], i, user_data))
        {
            break;
        }
    }
    return i;
}

UCB_API void ucb_vector_int_sort(ucb_vector_int* vec)
{
    ucb_vector_int_sort_with(vec, ucb_comp_func_int);
}

UCB_API void ucb_vector_int_sort_with(ucb_vector_int* vec, ucb_cmp_func func)
{
    UCB_VERIFY_ARGS(vec && func);
    qsort(vec->data, vec->size, sizeof(int), func);
}

UCB_API size_t ucb_vector_int_insert_sorted(ucb_vector_int* vec, int val)
{
    return ucb_vector_int_insert_sorted_with(vec, val, ucb_comp_func_int);
}

UCB_API size_t ucb_vector_int_insert_sorted_with(ucb_vector_int* vec, int val, ucb_cmp_func func)
{
    UCB_VERIFY_ARGS(vec && func);
    ucb_ssize pos = ucb_vector_int_find_with(vec, &val, func);
    size_t ins_pos;
    if (pos >= 0)
    {
        ins_pos = pos + 1;
    }
    else
    {
        ins_pos = -pos - 1;
    }
    ucb_vector_int_insert(vec, ins_pos, val);
    return ins_pos;
}

UCB_API ucb_ssize ucb_vector_int_find(const ucb_vector_int* vec, const int* val)
{
    return ucb_vector_int_find_with(vec, val, ucb_comp_func_int);
}

UCB_API ucb_ssize ucb_vector_int_find_with(const ucb_vector_int* vec,
                                           const int* val,
                                           ucb_cmp_func func)
{
    UCB_VERIFY_ARGS(vec && func);

    ucb_ssize low = 0;
    ucb_ssize high = ucb_vector_int_size(vec) - 1;
    while (low <= high)
    {
        ucb_ssize mid = low + (high - low) / 2;
        int cmp = func(val, &vec->data[mid]);
        if (cmp == 0)
        {
            while (mid < high && func(val, &vec->data[mid + 1]) == 0)
            {
                mid++;
            }
            return mid;
        }
        else if (cmp < 0)
        {
            high = mid - 1;
        }
        else
        {
            low = mid + 1;
        }
    }
    return -(low + 1);
}
