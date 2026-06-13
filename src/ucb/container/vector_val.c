#include "ucb/container/vector_val.h"

#include "ucb/errcodes.h"
#include "ucb/error.h"
#include "ucb/memory.h"
#include "ucb/types.h"

#include <stdlib.h>
#include <string.h>

#define UCB_VECTOR_VAL_DEFAULT_CAPACITY 8

/* -------------------------------------------------------------------------- */
/*                                  Lifecycle                                 */
/* -------------------------------------------------------------------------- */

ucb_vector_val* ucb_vector_val_new(size_t esize)
{
    ucb_vector_val* vec = ucb_malloc_type(1, ucb_vector_val);
    if (vec)
    {
        vec->data = UCB_NULL;
        vec->esize = esize;
        vec->size = 0;
        vec->capacity = 0;
    }
    return vec;
}

ucb_vector_val* ucb_vector_val_new_args(size_t esize,
                                        size_t initial_capacity,
                                        ucb_release_func release_func)
{
    ucb_vector_val* vec = ucb_vector_val_new(esize);
    if (vec)
    {
        vec->release_func = release_func;
        if (initial_capacity > 0 && !ucb_vector_val_reserve(vec, initial_capacity))
        {
            ucb_free(vec);
            vec = UCB_NULL;
        }
    }
    return vec;
}

void ucb_vector_val_free(ucb_vector_val* vec)
{
    ucb_vector_val_clear(vec);
    ucb_free(vec->data);
    ucb_free(vec);
}

void ucb_vector_val_free_deep(ucb_vector_val* vec)
{
    ucb_vector_val_clear_deep(vec);
    ucb_free(vec->data);
    ucb_free(vec);
}

ucb_vector_val* ucb_vector_val_clone(const ucb_vector_val* vec)
{
    UCB_VERIFY_ARGS(vec);

    ucb_vector_val* new_vec = ucb_vector_val_new(vec->capacity);

    if (new_vec)
    {
        if (!ucb_vector_val_copy(new_vec, vec))
        {
            ucb_vector_val_free(new_vec);
            new_vec = UCB_NULL;
        }
    }
    return new_vec;
}

ucb_vector_val* ucb_vector_val_clone_deep(const ucb_vector_val* vec, ucb_copy_func copy_func)
{
    UCB_VERIFY_ARGS(vec && copy_func);

    ucb_vector_val* new_vec = ucb_vector_val_new(vec->capacity);

    if (new_vec)
    {
        if (!ucb_vector_val_copy_deep(new_vec, vec, copy_func))
        {
            ucb_vector_val_free_deep(new_vec);
            new_vec = UCB_NULL;
        }
    }
    return new_vec;
}

bool ucb_vector_val_copy(ucb_vector_val* dst, const ucb_vector_val* src)
{
    UCB_VERIFY_ARGS(dst && src && dst != src);

    ucb_vector_val_clear(dst);
    dst->esize = src->esize;
    dst->release_func = src->release_func;

    if (!ucb_vector_val_reserve(dst, src->capacity))
    {
        return false;
    }

    memcpy(dst->data, src->data, src->size * sizeof(src->esize));
    dst->size = src->size;
    return true;
}

bool ucb_vector_val_copy_deep(ucb_vector_val* dst,
                              const ucb_vector_val* src,
                              ucb_copy_func copy_func)
{
    UCB_VERIFY_ARGS(copy_func);
    if (ucb_vector_val_copy(dst, src))
    {
        return false;
    }

    for (size_t i = 0; i < src->size; i++)
    {
        void* elem_dst = dst->data + i * src->esize;
        void* elem_src = src->data + i * src->esize;

        if (!copy_func(elem_dst, elem_src))
        {
            return false;
        }
        dst->size++;
    }
    return true;
}

void ucb_vector_val_move(ucb_vector_val* dst, ucb_vector_val* src)
{
    UCB_VERIFY_ARGS(dst && src && dst != src);

    ucb_vector_val_clear(dst);
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

bool ucb_vector_val_reserve(ucb_vector_val* vec, size_t new_capacity)
{
    UCB_VERIFY_ARGS(vec);
    if (new_capacity == 0)
        new_capacity = UCB_VECTOR_VAL_DEFAULT_CAPACITY;
    if (new_capacity <= vec->capacity)
        return true;

    char* new_data = (char*)ucb_realloc(vec->data, new_capacity * vec->esize);
    if (!new_data)
        return false;

    vec->data = new_data;
    vec->capacity = new_capacity;
    return true;
}

#ifndef UCB_FAST
size_t ucb_vector_val_capacity(const ucb_vector_val* vec)
{
    UCB_VERIFY_ARGS(vec);
    return vec->capacity;
}

size_t ucb_vector_val_size(const ucb_vector_val* vec)
{
    UCB_VERIFY_ARGS(vec);
    return vec->size;
}

bool ucb_vector_val_is_empty(const ucb_vector_val* vec)
{
    UCB_VERIFY_ARGS(vec);
    return vec->size == 0;
}

void ucb_vector_val_clear(ucb_vector_val* vec)
{
    UCB_VERIFY_ARGS(vec);
    vec->size = 0;
}
#endif

void ucb_vector_val_clear_deep(ucb_vector_val* vec)
{
    UCB_VERIFY_ARGS(vec);
    UCB_VERIFY_MSG(vec->release_func,
                   UCB_ERROR_INVALID_STATE,
                   "Vector has no release function set. Cannot free member data");
    char* elem = vec->data;
    for (size_t i = 0; i < vec->size; i++, elem += vec->esize)
    {
        vec->release_func(elem);
    }
    vec->size = 0;
}

void ucb_vector_val_fit(ucb_vector_val* vec)
{
    UCB_VERIFY_ARGS(vec);
    if (vec->capacity > vec->size)
    {
        char* new_data = (char*)ucb_realloc(vec->data, vec->size * vec->esize);
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

void ucb_vector_val_insert(ucb_vector_val* vec, size_t index, void* data)
{
    UCB_VERIFY_ARGS(vec && index <= vec->size);
    if (vec->size == vec->capacity)
    {
        if (!ucb_vector_val_reserve(vec, vec->capacity * 2))
            return;
    }
    if (index < vec->size)
    {
        memmove(vec->data + (index + 1) * vec->esize,
                vec->data + index * vec->esize,
                (vec->size - index) * vec->esize);
    }

    memcpy(vec->data + index * vec->esize, data, vec->esize);
    vec->size++;
}

void ucb_vector_val_remove(ucb_vector_val* vec, size_t index)
{
    UCB_VERIFY_ARGS(vec && index < vec->size);

    if (index < vec->size - 1)
    {
        memmove(vec->data + index * vec->esize,
                vec->data + (index + 1) * vec->esize,
                (vec->size - index - 1) * vec->esize);
    }
    vec->size--;
}

void ucb_vector_val_push_back(ucb_vector_val* vec, const void* data)
{
    ucb_vector_val_insert(vec, vec->size, data);
}

void ucb_vector_val_push_front(ucb_vector_val* vec, void* data)
{
    ucb_vector_val_insert(vec, 0, data);
}

bool ucb_vector_val_pop_back(ucb_vector_val* vec)
{
    UCB_VERIFY_ARGS(vec);
    if (!vec->size)
        return false;
    return vec->data + (vec->size - 1) * vec->esize;
}

bool ucb_vector_val_pop_front(ucb_vector_val* vec)
{
    UCB_VERIFY_ARGS(vec);
    if (!vec->size)
        return false;
    ucb_vector_val_remove(vec, 0);
    return true;
}

#ifndef UCB_FAST
void ucb_vector_val_peek_back(const ucb_vector_val* vec, void* out_data)
{
    UCB_VERIFY_ARGS(vec && vec->size);
    memcpy(out_data, vec->data + (vec->size - 1) * vec->esize, vec->esize);
}

void ucb_vector_val_peek_front(const ucb_vector_val* vec, void* out_data)
{
    UCB_VERIFY_ARGS(vec && vec->size);
    memcpy(out_data, vec->data, vec->esize);
}

void ucb_vector_val_get(const ucb_vector_val* vec, size_t index, void* out_data)
{
    UCB_VERIFY_ARGS(vec && index < vec->size);
    memcpy(out_data, vec->data + index * vec->esize, vec->esize);
}

void ucb_vector_val_set(ucb_vector_val* vec, size_t index, void* data)
{
    UCB_VERIFY_ARGS(vec && index < vec->size);
    memcpy(vec->data + index * vec->esize, data, vec->esize);
}
#endif

size_t ucb_vector_val_foreach(ucb_vector_val* vec, ucb_vector_val_iter_func func, void* user_data)
{
    UCB_VERIFY_ARGS(vec && func);
    size_t i = 0;
    for (; i < vec->size; ++i)
    {
        void* val = vec->data + i * vec->esize;
        if (!func(val, i, user_data))
            break;
    }
    return i;
}
