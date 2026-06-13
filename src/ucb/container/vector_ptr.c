#include "ucb/container/vector_ptr.h"

#include "ucb/errcodes.h"
#include "ucb/error.h"
#include "ucb/memory.h"
#include "ucb/types.h"

#include <stdlib.h>
#include <string.h>

#define UCB_VECTOR_PTR_DEFAULT_CAPACITY 8

/* -------------------------------------------------------------------------- */
/*                                  Lifecycle                                 */
/* -------------------------------------------------------------------------- */

ucb_vector_ptr* ucb_vector_ptr_new(void)
{
    ucb_vector_ptr* vec = ucb_malloc_type(1, ucb_vector_ptr);
    if (vec)
    {
        vec->data = UCB_NULL;
        vec->size = 0;
        vec->capacity = 0;
    }
    return vec;
}

ucb_vector_ptr* ucb_vector_ptr_new_args(size_t initial_capacity, ucb_free_func free_func)
{
    ucb_vector_ptr* vec = ucb_vector_ptr_new();
    if (vec)
    {
        if (initial_capacity > 0 && !ucb_vector_ptr_reserve(vec, initial_capacity))
        {
            ucb_free(vec);
            vec = UCB_NULL;
        }
        vec->free_func = free_func;
    }
    return vec;
}

void ucb_vector_ptr_free(ucb_vector_ptr* vec)
{
    ucb_vector_ptr_clear(vec);
    ucb_free(vec->data);
    ucb_free(vec);
}

void ucb_vector_ptr_free_deep(ucb_vector_ptr* vec)
{
    ucb_vector_ptr_clear_deep(vec);
    ucb_free(vec->data);
    ucb_free(vec);
}

ucb_vector_ptr* ucb_vector_ptr_clone(const ucb_vector_ptr* vec)
{
    UCB_VERIFY_ARGS(vec);

    ucb_vector_ptr* new_vec = ucb_vector_ptr_new_args(vec->capacity, vec->free_func);

    if (new_vec)
    {
        if (!ucb_vector_ptr_copy(new_vec, vec))
        {
            ucb_vector_ptr_free(new_vec);
            new_vec = UCB_NULL;
        }
    }
    return new_vec;
}

ucb_vector_ptr* ucb_vector_ptr_clone_deep(const ucb_vector_ptr* vec, ucb_clone_func clone_func)
{
    UCB_VERIFY_ARGS(vec && clone_func);

    ucb_vector_ptr* new_vec = ucb_vector_ptr_new_args(vec->capacity, vec->free_func);

    if (new_vec)
    {
        if (!ucb_vector_ptr_copy_deep(new_vec, vec, clone_func))
        {
            ucb_vector_ptr_free_deep(new_vec);
            new_vec = UCB_NULL;
        }
    }
    return new_vec;
}

bool ucb_vector_ptr_copy(ucb_vector_ptr* dst, const ucb_vector_ptr* src)
{
    UCB_VERIFY_ARGS(dst && src && dst != src);

    ucb_vector_ptr_clear(dst);
    if (!ucb_vector_ptr_reserve(dst, src->capacity))
    {
        return false;
    }

    memcpy(dst->data, src->data, src->size * sizeof(void*));
    dst->size = src->size;
    return true;
}

bool ucb_vector_ptr_copy_deep(ucb_vector_ptr* dst,
                              const ucb_vector_ptr* src,
                              ucb_clone_func clone_func)
{
    UCB_VERIFY_ARGS(dst && src && dst != src && clone_func);

    ucb_vector_ptr_clear_deep(dst);
    if (!ucb_vector_ptr_reserve(dst, src->capacity))
    {
        return false;
    }

    for (size_t i = 0; i < src->size; i++)
    {
        dst->data[i] = clone_func(src->data[i]);
        if (!dst->data[i])
        {
            return false;
        }
        // Update per element so freeing is safe if we fail
        dst->size++;
    }
    return true;
}

void ucb_vector_ptr_move(ucb_vector_ptr* dst, ucb_vector_ptr* src)
{
    UCB_VERIFY_ARGS(dst && src && dst != src);

    ucb_vector_ptr_clear(dst);
    ucb_free(dst->data);

    dst->data = src->data;
    dst->size = src->size;
    dst->capacity = src->capacity;

    // Leave src in a valid but empty state
    src->data = UCB_NULL;
    src->size = 0;
    src->capacity = 0;
}

/* -------------------------------------------------------------------------- */
/*                              Capacity and size                             */
/* -------------------------------------------------------------------------- */

bool ucb_vector_ptr_reserve(ucb_vector_ptr* vec, size_t new_capacity)
{
    UCB_VERIFY_ARGS(vec);
    if (new_capacity == 0)
        new_capacity = UCB_VECTOR_PTR_DEFAULT_CAPACITY;
    if (new_capacity <= vec->capacity)
        return true;

    void** new_data = ucb_realloc(vec->data, new_capacity * sizeof(void*));
    if (!new_data)
        return false;

    vec->data = new_data;
    vec->capacity = new_capacity;
    return true;
}

#ifndef UCB_FAST
size_t ucb_vector_ptr_capacity(const ucb_vector_ptr* vec)
{
    UCB_VERIFY_ARGS(vec);
    return vec->capacity;
}

size_t ucb_vector_ptr_size(const ucb_vector_ptr* vec)
{
    UCB_VERIFY_ARGS(vec);
    return vec->size;
}

bool ucb_vector_ptr_is_empty(const ucb_vector_ptr* vec)
{
    UCB_VERIFY_ARGS(vec);
    return vec->size == 0;
}

void ucb_vector_ptr_clear(ucb_vector_ptr* vec)
{
    UCB_VERIFY_ARGS(vec);
    vec->size = 0;
}
#endif

void ucb_vector_ptr_clear_deep(ucb_vector_ptr* vec)
{
    UCB_VERIFY_ARGS(vec);
    UCB_VERIFY_MSG(vec->free_func,
                   UCB_ERROR_INVALID_STATE,
                   "Vector has no free function set. Cannot free members");
    for (size_t i = 0; i < vec->size; i++)
    {
        vec->free_func(vec->data[i]);
    }
    vec->size = 0;
}

void ucb_vector_ptr_fit(ucb_vector_ptr* vec)
{
    UCB_VERIFY_ARGS(vec);
    if (vec->capacity > vec->size)
    {
        void** new_data = ucb_realloc(vec->data, vec->size * sizeof(void*));
        if (new_data)
        {
            vec->data = new_data;
            vec->capacity = vec->size;
        }
    }
}

/* -------------------------------------------------------------------------- */
/*                       Element access and modification                      */
/* -------------------------------------------------------------------------- */

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
        memmove(vec->data + (index + 1) * sizeof(void*),
                vec->data + index * sizeof(void*),
                (vec->size - index) * sizeof(void*));
    }

    vec->data[index] = data;
    vec->size++;
}

void* ucb_vector_ptr_remove(ucb_vector_ptr* vec, size_t index)
{
    UCB_VERIFY_ARGS(vec && index < vec->size);

    void* ret = vec->data[index];
    if (index < vec->size - 1)
    {
        memmove(vec->data + index * sizeof(void*),
                vec->data + (index + 1) * sizeof(void*),
                (vec->size - index - 1) * sizeof(void*));
    }
    vec->size--;
    return ret;
}

void ucb_vector_ptr_push_back(ucb_vector_ptr* vec, void* data)
{
    ucb_vector_ptr_insert(vec, vec->size, data);
}

void ucb_vector_ptr_push_front(ucb_vector_ptr* vec, void* data)
{
    ucb_vector_ptr_insert(vec, 0, data);
}

void* ucb_vector_ptr_pop_back(ucb_vector_ptr* vec)
{
    UCB_VERIFY_ARGS(vec && vec->size);
    vec->size--;
    return vec->data[vec->size];
}

void* ucb_vector_ptr_pop_front(ucb_vector_ptr* vec)
{
    UCB_VERIFY_ARGS(vec && vec->size);
    return ucb_vector_ptr_remove(vec, 0);
}

#ifndef UCB_FAST
void* ucb_vector_ptr_peek_back(const ucb_vector_ptr* vec)
{
    UCB_VERIFY_ARGS(vec && vec->size);
    return vec->data[vec->size - 1];
}

void* ucb_vector_ptr_peek_front(const ucb_vector_ptr* vec)
{
    UCB_VERIFY_ARGS(vec && vec->size);
    return vec->data[0];
}

void* ucb_vector_ptr_get(const ucb_vector_ptr* vec, size_t index)
{
    UCB_VERIFY_ARGS(vec && index < vec->size);
    return vec->data[index];
}

void ucb_vector_ptr_set(ucb_vector_ptr* vec, size_t index, void* data)
{
    UCB_VERIFY_ARGS(vec && index < vec->size);
    vec->data[index] = data;
}
#endif

size_t ucb_vector_ptr_foreach(ucb_vector_ptr* vec, ucb_vector_ptr_func func, void* user_data)
{
    UCB_VERIFY_ARGS(vec && func);
    size_t i = 0;
    for (; i < vec->size; ++i)
    {
        if (!func(vec->data[i], i, user_data))
            break;
    }
    return i;
}
