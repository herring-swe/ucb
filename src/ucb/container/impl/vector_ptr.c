#include <ucb/algorithm.h>
#include <ucb/container/impl/vector_ptr.h>
#include <ucb/error.h>
#include <ucb/memory.h>

#include <stdlib.h>
#include <string.h>

ucb_vector_ptr* ucb_vector_ptr_new(void)
{
    return ucb_calloc_type(1, ucb_vector_ptr);
}

void ucb_vector_ptr_free(ucb_vector_ptr* vec)
{
    if (vec)
    {
        ucb_vector_ptr_clear(vec);
        ucb_free(vec->data);
        ucb_free(vec);
    }
}

bool ucb_vector_ptr_copy(ucb_vector_ptr* dst, const ucb_vector_ptr* src)
{
    UCB_VERIFY_ARGS(dst && src && dst != src);

    ucb_vector_ptr_clear(dst);

    if (!ucb_vector_ptr_reserve(dst, src->capacity))
    {
        ucb_vector_ptr_clear(dst);
        return false;
    }

    dst->size = src->size;

    memcpy(dst->data, src->data, src->size * sizeof(void*));

    return true;
}

ucb_vector_ptr* ucb_vector_ptr_clone(const ucb_vector_ptr* src)
{
    UCB_VERIFY_ARGS(src);

    ucb_vector_ptr* dst = ucb_calloc_type(1, ucb_vector_ptr);
    if (dst)
    {
        if (!ucb_vector_ptr_copy(dst, src))
        {
            ucb_free(dst);
            return UCB_NULL;
        }
    }
    return dst;
}

void ucb_vector_ptr_move(ucb_vector_ptr* dst, ucb_vector_ptr* src)
{
    UCB_VERIFY_ARGS(dst && src && dst != src);

    ucb_vector_ptr_clear(dst);
    ucb_free(dst->data);

    dst->data = src->data;
    dst->size = src->size;
    dst->capacity = src->capacity;

    src->data = UCB_NULL;
    src->size = 0;
    src->capacity = 0;
}

bool ucb_vector_ptr_reserve(ucb_vector_ptr* vec, size_t new_capacity)
{
    UCB_VERIFY_ARGS(vec);
    if (new_capacity == 0)
        new_capacity = 8;
    if (new_capacity <= vec->capacity)
        return true;

    void** new_data = ucb_realloc_type(vec->data, new_capacity, void*);
    if (!new_data)
        return false;

    vec->data = new_data;
    vec->capacity = new_capacity;
    return true;
}

void ucb_vector_ptr_fit(ucb_vector_ptr* vec)
{
    UCB_VERIFY_ARGS(vec);
    if (vec->capacity > vec->size)
    {
        void** new_data = ucb_realloc_type(vec->data, vec->size, void*);
        if (new_data)
        {
            vec->data = new_data;
            vec->capacity = vec->size;
        }
    }
}

void ucb_vector_ptr_insert(ucb_vector_ptr* vec, size_t index, void* data)
{
    UCB_VERIFY_ARGS(vec && index <= vec->size);
    if (vec->size == vec->capacity)
    {
        if (!ucb_vector_ptr_reserve(vec, vec->capacity * 2))
            return;
    }
    if (index < vec->size)
    {
        memmove(vec->data + (index + 1), vec->data + index, (vec->size - index) * sizeof(void*));
    }
    vec->data[index] = data;
    vec->size++;
}

void* ucb_vector_ptr_remove(ucb_vector_ptr* vec, size_t index)
{
    UCB_VERIFY_ARGS(vec && index < vec->size);
    UCB_VERIFY_MSG(vec->size > 0, UCB_ERROR_INVALID_ARG, "Cannot remove from empty container");
    void* data = vec->data[index];
    if (index < vec->size - 1)
    {
        memmove(vec->data + index, vec->data + index + 1, (vec->size - index - 1) * sizeof(void*));
    }
    vec->size--;
    return data;
}

void ucb_vector_ptr_push_back(ucb_vector_ptr* vec, void* data)
{
    UCB_VERIFY_ARGS(vec);
    if (vec->size < vec->capacity)
    {
        vec->data[vec->size] = data;
        vec->size++;
        return;
    }
    ucb_vector_ptr_insert(vec, vec->size, data);
}

void ucb_vector_ptr_push_front(ucb_vector_ptr* vec, void* data)
{
    UCB_VERIFY_ARGS(vec);
    if (vec->size == 0 && vec->capacity > 0)
    {
        vec->data[0] = data;
        vec->size++;
        return;
    }
    ucb_vector_ptr_insert(vec, 0, data);
}

void* ucb_vector_ptr_pop_back(ucb_vector_ptr* vec)
{
    UCB_VERIFY_ARGS(vec && vec->size);
    UCB_VERIFY_MSG(vec->size > 0, UCB_ERROR_INVALID_ARG, "Cannot remove from empty container");
    vec->size--;
    return vec->data[vec->size];
}

void* ucb_vector_ptr_pop_front(ucb_vector_ptr* vec)
{
    UCB_VERIFY_ARGS(vec && vec->size);
    UCB_VERIFY_MSG(vec->size > 0, UCB_ERROR_INVALID_ARG, "Cannot remove from empty container");
    vec->size--;
    void* ret = vec->data[0];
    if (vec->size > 0)
    {
        memmove(vec->data, vec->data + 1, vec->size * sizeof(void*));
    }
    return ret;
}

size_t ucb_vector_ptr_foreach(ucb_vector_ptr* vec, ucb_vector_ptr_iter_func func, void* user_data)
{
    UCB_VERIFY_ARGS(vec && func);
    size_t i = 0;
    for (; i < vec->size; ++i)
    {
        if (!func(vec->data[i], i, user_data))
        {
            break;
        }
    }
    return i;
}

void ucb_vector_ptr_sort(ucb_vector_ptr* vec)
{
    ucb_vector_ptr_sort_with(vec, ucb_comp_func_ptr);
}

int ucb_vector_ptr_cmp_wrapper(const void* a, const void* b, void* ctx)
{
    ucb_cmp_func func = (ucb_cmp_func)ctx;
    const void* a_ptr = *(void**)a;
    const void* b_ptr = *(void**)b;
    return func(a_ptr, b_ptr);
}

void ucb_vector_ptr_sort_with(ucb_vector_ptr* vec, ucb_cmp_func func)
{
    UCB_VERIFY_ARGS(vec && func);

    ucb_qsort_ctx(vec->data,
                  vec->size,
                  sizeof(void*),
                  ucb_vector_ptr_cmp_wrapper,
                  (void*)(uintptr_t)func);
}

size_t ucb_vector_ptr_insert_sorted(ucb_vector_ptr* vec, void* val)
{
    return ucb_vector_ptr_insert_sorted_with(vec, val, ucb_comp_func_ptr);
}

size_t ucb_vector_ptr_insert_sorted_with(ucb_vector_ptr* vec, void* val, ucb_cmp_func func)
{
    UCB_VERIFY_ARGS(vec && func);
    ucb_ssize pos = ucb_vector_ptr_find_with(vec, val, func);
    size_t ins_pos;
    if (pos >= 0)
    {
        ins_pos = pos + 1;
    }
    else
    {
        ins_pos = -pos - 1;
    }
    ucb_vector_ptr_insert(vec, ins_pos, val);
    return ins_pos;
}

ucb_ssize ucb_vector_ptr_find(const ucb_vector_ptr* vec, const void* val)
{
    return ucb_vector_ptr_find_with(vec, val, ucb_comp_func_ptr);
}

ucb_ssize ucb_vector_ptr_find_with(const ucb_vector_ptr* vec, const void* val, ucb_cmp_func func)
{
    UCB_VERIFY_ARGS(vec && func);

    ucb_ssize low = 0;
    ucb_ssize high = ucb_vector_ptr_size(vec) - 1;
    while (low <= high)
    {
        ucb_ssize mid = low + (high - low) / 2;
        int cmp = func(val, vec->data[mid]);
        if (cmp == 0)
        {
            while (mid < high && func(val, vec->data[mid + 1]) == 0)
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
