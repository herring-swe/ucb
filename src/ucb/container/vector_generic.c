#include "ucb/container/vector_generic.h"

#include "ucb/errcodes.h"
#include "ucb/error.h"
#include "ucb/memory.h"
#include "ucb/types.h"

#include <stdlib.h>
#include <string.h>

/*
 * Important:
 * The vector is designed to store either data items directly or pointers to data items.
 *
 * If data_clone or data_free is provided, the vector will store pointers to data items,
 * and the user must ensure that the data items are properly allocated and freed.
 *
 * If data_clone and data_free are not provided, the vector will store data items directly,
 * and the user must ensure that the data items are trivially copyable and do not require special
 * handling.
 */
struct ucb_vector
{
    char* data;                // array of data or pointers to data
    size_t esize;              // size of each element
    size_t size;               // number of elements in the vector
    size_t capacity;           // allocated capacity of the vector
    ucb_cmp_func data_cmp;     // function to compare data items
    ucb_clone_func data_clone; // function to clone data items (owned pointer mode only)
    ucb_free_func data_free;   // function to free data items (owned pointer mode only)
    bool is_pointer;           // true when element_size was 0: data slot holds a void*
};

/* -------------------------------------------------------------------------- */
/*                                  Lifecycle                                 */
/* -------------------------------------------------------------------------- */

ucb_vector* ucb_vector_new(ucb_vector_args args)
{
    bool is_pointer = (args.element_size == 0);
    if (is_pointer)
        args.element_size = sizeof(void*);
    if (args.initial_capacity == 0)
        args.initial_capacity = 4;

    if (args.data_clone || args.data_free)
    {
        UCB_VERIFY_MSG(is_pointer, UCB_ERROR_INVALID_ARG,
                       "data_clone/data_free require pointer mode (element_size must be 0)");
        UCB_VERIFY_MSG(args.data_clone && args.data_free, UCB_ERROR_INVALID_ARG,
                       "data_clone and data_free must both be provided together");
    }

    ucb_vector* vec = ucb_malloc_type(1, ucb_vector);
    if (vec)
    {
        vec->data = UCB_NULL;
        vec->data_cmp = args.data_cmp;
        vec->data_clone = args.data_clone;
        vec->data_free = args.data_free;
        vec->esize = args.element_size;
        vec->is_pointer = is_pointer;
        vec->size = 0;
        vec->capacity = 0;

        if (!ucb_vector_reserve(vec, args.initial_capacity))
        {
            ucb_free(vec);
            vec = UCB_NULL;
        }
    }
    return vec;
}

void ucb_vector_free(ucb_vector* vec)
{
    UCB_VERIFY_ARGS(vec);
    ucb_vector_clear(vec);
    ucb_free(vec->data);
    ucb_free(vec);
}

ucb_vector* ucb_vector_clone(const ucb_vector* vec)
{
    UCB_VERIFY_ARGS(vec);

    ucb_vector* new_vec = ucb_vector_new((ucb_vector_args){
        .data_cmp = vec->data_cmp,
        .data_clone = vec->data_clone,
        .data_free = vec->data_free,
        .element_size = vec->is_pointer ? 0 : vec->esize,
        .initial_capacity = vec->capacity,
    });

    if (new_vec)
    {
        if (!ucb_vector_copy(new_vec, vec))
        {
            ucb_vector_free(new_vec);
            new_vec = UCB_NULL;
        }
    }
    return new_vec;
}

bool ucb_vector_copy(ucb_vector* dst, const ucb_vector* src)
{
    UCB_VERIFY_ARGS(dst && src && dst != src);

    ucb_vector_clear(dst);
    dst->esize = src->esize;
    dst->is_pointer = src->is_pointer;
    dst->data_cmp = src->data_cmp;
    dst->data_clone = src->data_clone;
    dst->data_free = src->data_free;
    ucb_vector_reserve(dst, src->capacity);

    if (dst->is_pointer && dst->data_clone)
    {
        for (size_t i = 0; i < src->size; i++)
        {
            void* item = dst->data_clone(((void**)src->data)[i]);
            if (!item)
                return false;
            ((void**)dst->data)[i] = item;
            dst->size++;
        }
    }
    else
    {
        memcpy(dst->data, src->data, src->size * dst->esize);
        dst->size = src->size;
    }
    return true;
}

void ucb_vector_move(ucb_vector* dst, ucb_vector* src)
{
    UCB_VERIFY_ARGS(dst && src && dst != src);

    ucb_vector_clear(dst);
    ucb_free(dst->data);

    dst->data = src->data;
    dst->esize = src->esize;
    dst->is_pointer = src->is_pointer;
    dst->size = src->size;
    dst->capacity = src->capacity;
    dst->data_cmp = src->data_cmp;
    dst->data_clone = src->data_clone;
    dst->data_free = src->data_free;

    // Leave src in a valid but empty state
    src->data = UCB_NULL;
    src->size = 0;
    src->capacity = 0;
}

/* -------------------------------------------------------------------------- */
/*                              Capacity and size                             */
/* -------------------------------------------------------------------------- */

bool ucb_vector_reserve(ucb_vector* vec, size_t new_capacity)
{
    UCB_VERIFY_ARGS(vec);
    if (new_capacity <= vec->capacity)
        return true;

    char* new_data = ucb_realloc(vec->data, new_capacity * vec->esize);
    if (!new_data)
        return false;

    vec->data = new_data;
    vec->capacity = new_capacity;
    return true;
}

size_t ucb_vector_capacity(const ucb_vector* vec)
{
    UCB_VERIFY_ARGS(vec);
    return vec->capacity;
}

size_t ucb_vector_size(const ucb_vector* vec)
{
    UCB_VERIFY_ARGS(vec);
    return vec->size;
}

bool ucb_vector_is_empty(const ucb_vector* vec)
{
    UCB_VERIFY_ARGS(vec);
    return vec->size == 0;
}

void ucb_vector_clear(ucb_vector* vec)
{
    UCB_VERIFY_ARGS(vec);
    if (vec->data_free)
    {
        for (size_t i = 0; i < vec->size; i++)
        {
            void* item = ((void**)vec->data)[i];
            vec->data_free(item);
        }
    }
    vec->size = 0;
}

void ucb_vector_fit(ucb_vector* vec)
{
    UCB_VERIFY_ARGS(vec);
    if (vec->capacity > vec->size)
    {
        char* new_data = ucb_realloc(vec->data, vec->size * vec->esize);
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

void ucb_vector_insert(ucb_vector* vec, size_t index, const void* data)
{
    UCB_VERIFY_ARGS(vec && index <= vec->size);
    if (vec->size == vec->capacity)
    {
        if (!ucb_vector_reserve(vec, vec->capacity * 2))
            return;
    }
    if (index < vec->size)
    {
        memmove(vec->data + (index + 1) * vec->esize, vec->data + index * vec->esize,
                (vec->size - index) * vec->esize);
    }
    if (vec->is_pointer)
    {
        void* item = vec->data_clone ? vec->data_clone(data) : (void*)data;
        if (vec->data_clone && !item)
            return;
        ((void**)vec->data)[index] = item;
    }
    else
    {
        memcpy(vec->data + index * vec->esize, data, vec->esize);
    }
    vec->size++;
}

void ucb_vector_remove(ucb_vector* vec, size_t index, void* out_data)
{
    // UCB_VERIFY_ARGS(vec && index < vec->size);
    if (out_data)
    {
        if (vec->is_pointer)
        {
            void* item = ((void**)vec->data)[index];
            memcpy(out_data, &item, sizeof(void*));
        }
        else
        {
            memcpy(out_data, vec->data + index * vec->esize, vec->esize);
        }
    }
    // Only free if the user didn't request the removed item back, otherwise they are responsible
    // for freeing it
    else if (vec->data_free)
    {
        void* item = ((void**)vec->data)[index];
        vec->data_free(item);
    }
    if (index < vec->size - 1)
    {
        memmove(vec->data + index * vec->esize, vec->data + (index + 1) * vec->esize,
                (vec->size - index - 1) * vec->esize);
    }
    vec->size--;
}

void ucb_vector_push_back(ucb_vector* vec, const void* data)
{
    if (!vec->is_pointer && vec->size < vec->capacity)
    {
        memcpy(vec->data + vec->size * vec->esize, data, vec->esize);
        vec->size++;
        return;
    }
    ucb_vector_insert(vec, vec->size, data);
}

void ucb_vector_push_front(ucb_vector* vec, const void* data)
{
    ucb_vector_insert(vec, 0, data);
}

bool ucb_vector_pop_back(ucb_vector* vec, void* out_data)
{
    if (vec->size == 0)
        return false;
    ucb_vector_remove(vec, vec->size - 1, out_data);
    return true;
}

bool ucb_vector_pop_front(ucb_vector* vec, void* out_data)
{
    if (vec->size == 0)
        return false;
    ucb_vector_remove(vec, 0, out_data);
    return true;
}

bool ucb_vector_peek_back(const ucb_vector* vec, void* out_data)
{
    if (vec->size == 0)
        return false;
    return ucb_vector_get(vec, vec->size - 1, out_data);
}

bool ucb_vector_peek_front(const ucb_vector* vec, void* out_data)
{
    if (vec->size == 0)
        return false;
    return ucb_vector_get(vec, 0, out_data);
}

bool ucb_vector_get(const ucb_vector* vec, size_t index, void* out_data)
{
    UCB_VERIFY_ARGS(vec && index < vec->size);
    if (out_data)
    {
        if (vec->is_pointer)
        {
            void* item = ((void**)vec->data)[index];
            memcpy(out_data, &item, sizeof(void*));
        }
        else
        {
            memcpy(out_data, vec->data + index * vec->esize, vec->esize);
        }
    }
    return true;
}

void ucb_vector_set(ucb_vector* vec, size_t index, const void* data)
{
    UCB_VERIFY_ARGS(vec && index < vec->size);
    if (vec->data_free)
    {
        void* old_item = ((void**)vec->data)[index]; // data_free only valid in pointer mode
        vec->data_free(old_item);
    }
    if (vec->is_pointer)
    {
        void* item = vec->data_clone ? vec->data_clone(data) : (void*)data;
        if (vec->data_clone && !item)
            return;
        ((void**)vec->data)[index] = item;
    }
    else
    {
        memcpy(vec->data + index * vec->esize, data, vec->esize);
    }
}

size_t ucb_vector_foreach(ucb_vector* vec, ucb_vector_iter_func func, void* user_data)
{
    UCB_VERIFY_ARGS(vec && func);
    size_t i = 0;
    for (; i < vec->size; ++i)
    {
        void* item;
        if (vec->is_pointer)
        {
            item = ((void**)vec->data)[i];
        }
        else
        {
            item = (vec->data + i * vec->esize);
        }
        if (!func(item, i, user_data))
            break;
    }
    return i;
}
