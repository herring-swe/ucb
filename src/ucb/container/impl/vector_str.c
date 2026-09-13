#include <ucb/algorithm.h>
#include <ucb/container/impl/vector_str.h>
#include <ucb/error.h>
#include <ucb/memory.h>

#include <stdlib.h>
#include <string.h>

ucb_vector_str* ucb_vector_str_new(void)
{
    return ucb_calloc_type(1, ucb_vector_str);
}

void ucb_vector_str_free(ucb_vector_str* vec)
{
    if (vec)
    {
        ucb_vector_str_clear(vec);
        ucb_free(vec->data);
        ucb_free(vec);
    }
}

void ucb_vector_str_free_full(ucb_vector_str* vec)
{
    if (vec)
    {
        ucb_vector_str_clear_deep(vec);
        ucb_free(vec->data);
        ucb_free(vec);
    }
}

bool ucb_vector_str_copy(ucb_vector_str* dst, const ucb_vector_str* src)
{
    UCB_VERIFY_ARGS(dst && src && dst != src);

    ucb_vector_str_clear(dst);

    if (!ucb_vector_str_reserve(dst, src->capacity))
    {
        ucb_vector_str_clear(dst);
        return false;
    }

    dst->size = src->size;

    memcpy(dst->data, src->data, src->size * sizeof(struct ucb_str*));

    return true;
}

ucb_vector_str* ucb_vector_str_clone(const ucb_vector_str* src)
{
    UCB_VERIFY_ARGS(src);

    ucb_vector_str* dst = ucb_calloc_type(1, ucb_vector_str);
    if (dst)
    {
        if (!ucb_vector_str_copy(dst, src))
        {
            ucb_free(dst);
            return UCB_NULL;
        }
    }
    return dst;
}

void ucb_vector_str_move(ucb_vector_str* dst, ucb_vector_str* src)
{
    UCB_VERIFY_ARGS(dst && src && dst != src);

    ucb_vector_str_clear(dst);
    ucb_free(dst->data);

    dst->data = src->data;
    dst->size = src->size;
    dst->capacity = src->capacity;

    src->data = UCB_NULL;
    src->size = 0;
    src->capacity = 0;
}

bool ucb_vector_str_reserve(ucb_vector_str* vec, size_t new_capacity)
{
    UCB_VERIFY_ARGS(vec);
    if (new_capacity == 0)
        new_capacity = 8;
    if (new_capacity <= vec->capacity)
        return true;

    struct ucb_str** new_data = ucb_realloc_type(vec->data, new_capacity, struct ucb_str*);
    if (!new_data)
        return false;

    vec->data = new_data;
    vec->capacity = new_capacity;
    return true;
}

void ucb_vector_str_clear_deep(ucb_vector_str* vec)
{
    UCB_VERIFY_ARGS(vec);
    for (size_t i = 0; i < vec->size; i++)
    {
        ucb_str_free(vec->data[i]);
    }
    vec->size = 0;
}

void ucb_vector_str_fit(ucb_vector_str* vec)
{
    UCB_VERIFY_ARGS(vec);
    if (vec->capacity > vec->size)
    {
        struct ucb_str** new_data = ucb_realloc_type(vec->data, vec->size, struct ucb_str*);
        if (new_data)
        {
            vec->data = new_data;
            vec->capacity = vec->size;
        }
    }
}

void ucb_vector_str_insert(ucb_vector_str* vec, size_t index, struct ucb_str* data)
{
    UCB_VERIFY_ARGS(vec && index <= vec->size);
    if (vec->size == vec->capacity)
    {
        if (!ucb_vector_str_reserve(vec, vec->capacity * 2))
            return;
    }
    if (index < vec->size)
    {
        memmove(vec->data + (index + 1),
                vec->data + index,
                (vec->size - index) * sizeof(struct ucb_str*));
    }
    vec->data[index] = data;
    vec->size++;
}

struct ucb_str* ucb_vector_str_remove(ucb_vector_str* vec, size_t index)
{
    UCB_VERIFY_ARGS(vec && index < vec->size);
    UCB_VERIFY_MSG(vec->size > 0, UCB_ERROR_INVALID_ARG, "Cannot remove from empty container");
    struct ucb_str* data = vec->data[index];
    if (index < vec->size - 1)
    {
        memmove(vec->data + index,
                vec->data + index + 1,
                (vec->size - index - 1) * sizeof(struct ucb_str*));
    }
    vec->size--;
    return data;
}

void ucb_vector_str_push_back(ucb_vector_str* vec, struct ucb_str* data)
{
    UCB_VERIFY_ARGS(vec);
    if (vec->size < vec->capacity)
    {
        vec->data[vec->size] = data;
        vec->size++;
        return;
    }
    ucb_vector_str_insert(vec, vec->size, data);
}

void ucb_vector_str_push_front(ucb_vector_str* vec, struct ucb_str* data)
{
    UCB_VERIFY_ARGS(vec);
    if (vec->size == 0 && vec->capacity > 0)
    {
        vec->data[0] = data;
        vec->size++;
        return;
    }
    ucb_vector_str_insert(vec, 0, data);
}

struct ucb_str* ucb_vector_str_pop_back(ucb_vector_str* vec)
{
    UCB_VERIFY_ARGS(vec && vec->size);
    UCB_VERIFY_MSG(vec->size > 0, UCB_ERROR_INVALID_ARG, "Cannot remove from empty container");
    vec->size--;
    return vec->data[vec->size];
}

struct ucb_str* ucb_vector_str_pop_front(ucb_vector_str* vec)
{
    UCB_VERIFY_ARGS(vec && vec->size);
    UCB_VERIFY_MSG(vec->size > 0, UCB_ERROR_INVALID_ARG, "Cannot remove from empty container");
    vec->size--;
    struct ucb_str* ret = vec->data[0];
    if (vec->size > 0)
    {
        memmove(vec->data, vec->data + 1, vec->size * sizeof(struct ucb_str*));
    }
    return ret;
}

size_t ucb_vector_str_foreach(ucb_vector_str* vec, ucb_vector_str_iter_func func, void* user_data)
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

void ucb_vector_str_sort(ucb_vector_str* vec)
{
    ucb_vector_str_sort_with(vec, ucb_str_cmp_func);
}

int ucb_vector_str_cmp_wrapper(const void* a, const void* b, void* ctx)
{
    ucb_cmp_func func = (ucb_cmp_func)ctx;
    const void* a_ptr = *(void**)a;
    const void* b_ptr = *(void**)b;
    return func(a_ptr, b_ptr);
}

void ucb_vector_str_sort_with(ucb_vector_str* vec, ucb_cmp_func func)
{
    UCB_VERIFY_ARGS(vec && func);

    ucb_qsort_ctx(vec->data,
                  vec->size,
                  sizeof(struct ucb_str*),
                  ucb_vector_str_cmp_wrapper,
                  (void*)(uintptr_t)func);
}

size_t ucb_vector_str_insert_sorted(ucb_vector_str* vec, struct ucb_str* val)
{
    return ucb_vector_str_insert_sorted_with(vec, val, ucb_str_cmp_func);
}

size_t ucb_vector_str_insert_sorted_with(ucb_vector_str* vec,
                                         struct ucb_str* val,
                                         ucb_cmp_func func)
{
    UCB_VERIFY_ARGS(vec && func);
    ucb_ssize pos = ucb_vector_str_find_with(vec, val, func);
    size_t ins_pos;
    if (pos >= 0)
    {
        ins_pos = pos + 1;
    }
    else
    {
        ins_pos = -pos - 1;
    }
    ucb_vector_str_insert(vec, ins_pos, val);
    return ins_pos;
}

ucb_ssize ucb_vector_str_find(const ucb_vector_str* vec, const struct ucb_str* val)
{
    return ucb_vector_str_find_with(vec, val, ucb_str_cmp_func);
}

ucb_ssize ucb_vector_str_find_with(const ucb_vector_str* vec,
                                   const struct ucb_str* val,
                                   ucb_cmp_func func)
{
    UCB_VERIFY_ARGS(vec && func);

    ucb_ssize low = 0;
    ucb_ssize high = ucb_vector_str_size(vec) - 1;
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
