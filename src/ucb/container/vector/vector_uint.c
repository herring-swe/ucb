#include <ucb/container/vector/vector_uint.h>
#include <ucb/error.h>
#include <ucb/memory.h>

#include <string.h>

ucb_vector_uint* ucb_vector_uint_new(void)
{
    return ucb_calloc_type(1, ucb_vector_uint);
}

void ucb_vector_uint_free(ucb_vector_uint* vec)
{
    if (vec)
    {
        ucb_vector_uint_clear(vec);
        ucb_free(vec->data);
        ucb_free(vec);
    }
}

bool ucb_vector_uint_copy(ucb_vector_uint* dst, const ucb_vector_uint* src)
{
    UCB_VERIFY_ARGS(dst && src && dst != src);

    ucb_vector_uint_clear(dst);

    dst->data = UCB_NULL;
    dst->size = src->size;

    if (!ucb_vector_uint_reserve(dst, src->capacity))
    {
        ucb_free(dst);
        return false;
    }

    memcpy(dst->data, src->data, src->size * sizeof(unsigned int));

    return true;
}

ucb_vector_uint* ucb_vector_uint_clone(const ucb_vector_uint* src)
{
    UCB_VERIFY_ARGS(src);

    ucb_vector_uint* dst = ucb_calloc_type(1, ucb_vector_uint);
    if (dst)
    {
        if (!ucb_vector_uint_copy(dst, src))
        {
            ucb_free(dst);
            return UCB_NULL;
        }
    }
    return dst;
}

void ucb_vector_uint_move(ucb_vector_uint* dst, ucb_vector_uint* src)
{
    UCB_VERIFY_ARGS(dst && src && dst != src);

    ucb_vector_uint_clear(dst);
    ucb_free(dst->data);

    dst->data = src->data;
    dst->size = src->size;
    dst->capacity = src->capacity;

    src->data = UCB_NULL;
    src->size = 0;
    src->capacity = 0;
}

bool ucb_vector_uint_reserve(ucb_vector_uint* vec, size_t new_capacity)
{
    UCB_VERIFY_ARGS(vec);
    if (new_capacity == 0)
        new_capacity = 8;
    if (new_capacity <= vec->capacity)
        return true;

    unsigned int* new_data = ucb_realloc_type(vec->data, new_capacity, unsigned int);
    if (!new_data)
        return false;

    vec->data = new_data;
    vec->capacity = new_capacity;
    return true;
}

void ucb_vector_uint_fit(ucb_vector_uint* vec)
{
    UCB_VERIFY_ARGS(vec);
    if (vec->capacity > vec->size)
    {
        unsigned int* new_data = ucb_realloc_type(vec->data, vec->size, unsigned int);
        if (new_data)
        {
            vec->data = new_data;
            vec->capacity = vec->size;
        }
    }
}

void ucb_vector_uint_insert(ucb_vector_uint* vec, size_t index, unsigned int data)
{
    UCB_VERIFY_ARGS(vec && index <= vec->size);
    if (vec->size == vec->capacity)
    {
        if (!ucb_vector_uint_reserve(vec, vec->capacity * 2))
            return;
    }
    if (index < vec->size)
    {
        memmove(vec->data + (index + 1) * sizeof(unsigned int),
                vec->data + index * sizeof(unsigned int),
                (vec->size - index) * sizeof(unsigned int));
    }
    vec->data[index] = data;
    vec->size++;
}

void ucb_vector_uint_remove(ucb_vector_uint* vec, size_t index)
{
    UCB_VERIFY_ARGS(vec && index < vec->size);
    if (index < vec->size - 1)
    {
        memmove(vec->data + index * sizeof(unsigned int),
                vec->data + (index + 1) * sizeof(unsigned int),
                (vec->size - index - 1) * sizeof(unsigned int));
    }
    vec->size--;
}

void ucb_vector_uint_push_back(ucb_vector_uint* vec, unsigned int data)
{
    UCB_VERIFY_ARGS(vec);
    if (vec->size < vec->capacity)
    {
        vec->data[vec->size] = data;
        vec->size++;
        return;
    }
    ucb_vector_uint_insert(vec, vec->size, data);
}

void ucb_vector_uint_push_front(ucb_vector_uint* vec, unsigned int data)
{
    UCB_VERIFY_ARGS(vec);
    if (vec->size == 0 && vec->capacity > 0)
    {
        vec->data[0] = data;
        vec->size++;
        return;
    }
    ucb_vector_uint_insert(vec, 0, data);
}

unsigned int ucb_vector_uint_pop_back(ucb_vector_uint* vec)
{
    UCB_VERIFY_ARGS(vec && vec->size);
    vec->size--;
    return vec->data[vec->size];
}

unsigned int ucb_vector_uint_pop_front(ucb_vector_uint* vec)
{
    UCB_VERIFY_ARGS(vec && vec->size);
    vec->size--;
    unsigned int ret = vec->data[0];
    if (vec->size > 0)
    {
        memmove(vec->data, vec->data + sizeof(unsigned int), vec->size * sizeof(unsigned int));
    }
    return ret;
}

size_t ucb_vector_uint_foreach(ucb_vector_uint* vec,
                               ucb_vector_uint_iter_func func,
                               void* user_data)
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

typedef struct ucb_vector_uint_iter
{
    ucb_iter2 base;
    ucb_vector_uint* vec;
    ucb_ssize index;
} ucb_vector_uint_iter;

static void ucb_vector_uint_iter_next(ucb_iter2* iter)
{
    ucb_vector_uint_iter* it = (ucb_vector_uint_iter*)iter;
    if (it->base.flags & UCB_ITER2_FLAG_REVERSE)
    {
        it->index--;
        if (it->index < 0)
            it->base.flags |= UCB_ITER2_FLAG_IS_END;
    }
    else
    {
        it->index++;
        if ((size_t)it->index >= it->vec->size)
            it->base.flags |= UCB_ITER2_FLAG_IS_END;
    }
}

static void ucb_vector_uint_iter_get(ucb_iter2* iter, void* out_item)
{
    ucb_vector_uint_iter* it = (ucb_vector_uint_iter*)iter;
    *((unsigned int*)out_item) = ucb_vector_uint_get(it->vec, (size_t)it->index);
}

static void ucb_vector_uint_iter_set(ucb_iter2* iter, void* value)
{
    ucb_vector_uint_iter* it = (ucb_vector_uint_iter*)iter;
    ucb_vector_uint_set(it->vec, (size_t)it->index, *(unsigned int*)value);
}

static void ucb_vector_uint_iter_remove(ucb_iter2* iter)
{
    ucb_vector_uint_iter* it = (ucb_vector_uint_iter*)iter;
    if (it->index < 0 || (size_t)it->index >= it->vec->size)
        return;
    ucb_vector_uint_remove(it->vec, (size_t)it->index);
    if (it->base.flags & UCB_ITER2_FLAG_REVERSE)
    {
        it->index--;
        if (it->index < 0)
            it->base.flags |= UCB_ITER2_FLAG_IS_END;
    }
    else
    {
        if ((size_t)it->index >= it->vec->size)
            it->base.flags |= UCB_ITER2_FLAG_IS_END;
    }
}

static bool ucb_vector_uint_iter_advance(ucb_iter2* iter, ptrdiff_t offset)
{
    ucb_vector_uint_iter* it = (ucb_vector_uint_iter*)iter;
    const bool reverse = (it->base.flags & UCB_ITER2_FLAG_REVERSE) != 0;

    it->index += reverse ? -(ucb_ssize)offset : (ucb_ssize)offset;

    if (it->index < 0)
    {
        if (reverse)
        {
            it->index = -1;
            it->base.flags = (it->base.flags & ~UCB_ITER2_FLAG_IS_BEGIN) | UCB_ITER2_FLAG_IS_END;
        }
        else
        {
            it->index = 0;
            it->base.flags = (it->base.flags & ~UCB_ITER2_FLAG_IS_END) | UCB_ITER2_FLAG_IS_BEGIN;
        }
        return false;
    }
    if ((size_t)it->index >= it->vec->size)
    {
        if (reverse)
        {
            it->index = (ucb_ssize)(it->vec->size - 1);
            it->base.flags = (it->base.flags & ~UCB_ITER2_FLAG_IS_END) | UCB_ITER2_FLAG_IS_BEGIN;
        }
        else
        {
            it->index = (ucb_ssize)it->vec->size;
            it->base.flags = (it->base.flags & ~UCB_ITER2_FLAG_IS_BEGIN) | UCB_ITER2_FLAG_IS_END;
        }
        return false;
    }
    it->base.flags &= ~(UCB_ITER2_FLAG_IS_BEGIN | UCB_ITER2_FLAG_IS_END);
    return true;
}

static ptrdiff_t ucb_vector_uint_iter_distance(ucb_iter2* iter_a, struct ucb_iter2* iter_b)
{
    ucb_vector_uint_iter* it_a = (ucb_vector_uint_iter*)iter_a;
    ucb_vector_uint_iter* it_b = (ucb_vector_uint_iter*)iter_b;
    ptrdiff_t d = (ptrdiff_t)(it_b->index - it_a->index);

    return (it_a->base.flags & UCB_ITER2_FLAG_REVERSE) ? -d : d;
}

static ucb_iter2* ucb_vector_uint_iter_init(ucb_vector_uint* vec, ucb_ssize index, uint8_t flags)
{
    static ucb_iter2_ops ops = {0};
    if (!ops.next)
    {
        ops.next = ucb_vector_uint_iter_next;
        ops.get = ucb_vector_uint_iter_get;
        ops.set = ucb_vector_uint_iter_set;
        ops.remove = ucb_vector_uint_iter_remove;
        ops.advance = ucb_vector_uint_iter_advance;
        ops.distance = ucb_vector_uint_iter_distance;
    }

    ucb_vector_uint_iter* it = ucb_calloc_type(1, ucb_vector_uint_iter);
    it->base.ops = &ops;
    it->base.flags = flags;
    it->base.size = sizeof(ucb_vector_uint_iter);
    it->index = index;
    it->vec = vec;
    return (ucb_iter2*)it;
}

ucb_iter2* ucb_vector_uint_begin(ucb_vector_uint* vec)
{
    UCB_VERIFY_ARGS(vec);
    return ucb_vector_uint_iter_init(vec, 0, UCB_ITER2_FLAG_IS_BEGIN);
}

ucb_iter2* ucb_vector_uint_end(ucb_vector_uint* vec)
{
    UCB_VERIFY_ARGS(vec);
    return ucb_vector_uint_iter_init(vec, vec->size, UCB_ITER2_FLAG_IS_END);
}

ucb_iter2* ucb_vector_uint_cbegin(const ucb_vector_uint* vec)
{
    UCB_VERIFY_ARGS(vec);
    return ucb_vector_uint_iter_init((ucb_vector_uint*)vec,
                                     0,
                                     UCB_ITER2_FLAG_IS_BEGIN | UCB_ITER2_FLAG_CONST);
}

ucb_iter2* ucb_vector_uint_cend(const ucb_vector_uint* vec)
{
    UCB_VERIFY_ARGS(vec);
    return ucb_vector_uint_iter_init((ucb_vector_uint*)vec,
                                     vec->size,
                                     UCB_ITER2_FLAG_IS_END | UCB_ITER2_FLAG_CONST);
}

ucb_iter2* ucb_vector_uint_rbegin(ucb_vector_uint* vec)
{
    UCB_VERIFY_ARGS(vec);
    return ucb_vector_uint_iter_init(vec,
                                     vec->size - 1,
                                     UCB_ITER2_FLAG_IS_BEGIN | UCB_ITER2_FLAG_REVERSE);
}

ucb_iter2* ucb_vector_uint_rend(ucb_vector_uint* vec)
{
    UCB_VERIFY_ARGS(vec);
    return ucb_vector_uint_iter_init(vec, -1, UCB_ITER2_FLAG_IS_END | UCB_ITER2_FLAG_REVERSE);
}

ucb_iter2* ucb_vector_uint_crbegin(const ucb_vector_uint* vec)
{
    UCB_VERIFY_ARGS(vec);
    return ucb_vector_uint_iter_init(
        (ucb_vector_uint*)vec,
        vec->size - 1,
        UCB_ITER2_FLAG_IS_BEGIN | UCB_ITER2_FLAG_REVERSE | UCB_ITER2_FLAG_CONST);
}

ucb_iter2* ucb_vector_uint_crend(const ucb_vector_uint* vec)
{
    UCB_VERIFY_ARGS(vec);
    return ucb_vector_uint_iter_init(
        (ucb_vector_uint*)vec,
        -1,
        UCB_ITER2_FLAG_IS_END | UCB_ITER2_FLAG_REVERSE | UCB_ITER2_FLAG_CONST);
}
