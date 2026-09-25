/**
 * @file vector_generic.h
 * This file is part of the UCB project
 * - SPDX-FileCopyrightText: © 2026 Åke Svedin <ake@svedin.org>
 * - SPDX-License-Identifier: MIT
 *
 * @brief Generic dynamic array (vector)
 */

#ifndef UCB_CONTAINER_VECTOR_H
#define UCB_CONTAINER_VECTOR_H

#include <ucb/container/common.h>
#include <ucb/export.h>

#include <stddef.h>

/**
 * @struct ucb_vector
 * @brief Dynamic array (vector)
 *
 * This is a generic dynamic array data container.
 *
 * It supports both single and multi-threaded access depending on the allocator used.
 *
 * Example usage:
 * @code
 * ucb_vector_args args = {0};
 * args.data_clone = my_clone; // user-provided function
 * args.data_free  = my_free;  // user-provided function
 * @endcode
 *
 * @code
 * ucb_vector* vec = ucb_vector_new(args);
 * ucb_vector_push_back(vec, my_item);
 * void* item = ucb_vector_pop_back(vec);
 * ucb_vector_free(vec);
 * @endcode
 */
typedef struct ucb_vector ucb_vector;

/**
 * @brief Configuration for @ref ucb_vector_new.
 *
 * The `element_size` field selects the storage mode:
 *
 * - **Value mode** (`element_size > 0`, no clone/free): each element is stored by
 *   copying `element_size` bytes. `push_back(vec, &value)` copies the value in.
 *   `get(vec, i, &value)` copies it back out. No lifetime management.
 *
 * - **Owned pointer mode** (`element_size == 0`, `data_clone` + `data_free` provided):
 *   each element is a heap-allocated pointer. `push_back(vec, ptr)` calls `data_clone(ptr)`
 *   and stores the result. `get(vec, i, &ptr)` returns the stored pointer (not a copy).
 *   `data_free` is called on remove/clear/free unless the caller takes ownership via out_data.
 *
 * - **Non-owning pointer mode** (`element_size == 0`, no clone/free): each element is a
 *   raw pointer stored as-is. `push_back(vec, ptr)` stores `ptr` directly.
 *   `get(vec, i, &ptr)` returns the same pointer. The vector never allocates or frees data.
 *
 * `data_clone` and `data_free` must always be provided together; both require pointer mode.
 */
typedef struct ucb_vector_args
{
    ucb_cmp_func data_cmp;     // optional, enables sorting and searching
    ucb_clone_func data_clone; // owned pointer mode only; must be paired with data_free
    ucb_free_func data_free;   // owned pointer mode only; must be paired with data_clone
    size_t element_size;       // 0 = pointer mode, >0 = value mode (number of bytes per element)
    size_t initial_capacity;   // optional, default initial capacity will be used if 0
} ucb_vector_args;

/**
 * @brief Function callback for foreach.
 * @param val The element to process.
 * @param index The index of the element.
 * @param user_data User data passed to the function.
 * @return true to continue, false to stop iteration.
 */
typedef bool (*ucb_vector_iter_func)(void* val, size_t index, void* user_data);

#ifdef __cplusplus
extern "C" {
#endif

// Lifecycle

/**
 * @brief Creates a new empty vector using the given configuration.
 * @param args the vector configuration
 * @return a new vector or UCB_NULL on allocation failure
 */
UCB_API ucb_vector* ucb_vector_new(ucb_vector_args args);
// UCB_API ucb_vector* ucb_vector_new_mt(ucb_vector_args args);

/**
 * @brief Frees the vector and its storage.
 *
 * In owned pointer mode, any remaining elements are freed with the configured
 * data_free function.
 * @param vec the vector to free
 */
UCB_API void ucb_vector_free(ucb_vector* vec);

/**
 * @brief Creates a copy of a vector and its contents.
 * @param vec the vector to clone
 * @return a new vector or UCB_NULL on allocation failure
 */
UCB_API ucb_vector* ucb_vector_clone(const ucb_vector* vec);

/**
 * @brief Copies the contents of one vector into another.
 *
 * @p dst is cleared first. In owned pointer mode the elements are deep copied.
 * @param dst the destination vector
 * @param src the source vector
 * @return true on success, false on allocation failure
 */
UCB_API bool ucb_vector_copy(ucb_vector* dst, const ucb_vector* src);

/**
 * @brief Moves the storage of one vector into another.
 *
 * @p dst is cleared first. @p src is left valid but empty.
 * @param dst the destination vector
 * @param src the source vector
 */
UCB_API void ucb_vector_move(ucb_vector* dst, ucb_vector* src);

// Capacity and size

/**
 * @brief Ensures the vector can hold at least @p new_capacity elements.
 *
 * If @p new_capacity is not greater than the current capacity this is a no-op.
 * @param vec the vector
 * @param new_capacity the desired minimum capacity
 * @return true on success, false on allocation failure
 */
UCB_API bool ucb_vector_reserve(ucb_vector* vec, size_t new_capacity);

/**
 * @brief Returns the capacity of the vector in elements.
 * @param vec the vector to query
 * @return the number of elements the vector can hold without growing
 */
UCB_API size_t ucb_vector_capacity(const ucb_vector* vec);

/**
 * @brief Returns the number of elements in the vector.
 * @param vec the vector to query
 * @return the number of stored elements
 */
UCB_API size_t ucb_vector_size(const ucb_vector* vec);

/**
 * @brief Checks whether the vector contains no elements.
 * @param vec the vector to query
 * @return true if the vector is empty
 */
UCB_API bool ucb_vector_is_empty(const ucb_vector* vec);

/**
 * @brief Clears the vector by setting its size to zero.
 *
 * The capacity is not changed. In owned pointer mode the elements are freed.
 * @param vec the vector to clear
 */
UCB_API void ucb_vector_clear(ucb_vector* vec);

/**
 * @brief Shrinks the capacity to match the current size.
 * @param vec the vector to fit
 */
UCB_API void ucb_vector_fit(ucb_vector* vec);

// Element access and modification

/**
 * @brief Inserts an element at the given index, shifting later elements.
 *
 * The index may be equal to the current size to append. In owned pointer mode
 * the data is cloned.
 * @param vec the vector to modify
 * @param index the insertion position
 * @param data the element to insert
 */
UCB_API void ucb_vector_insert(ucb_vector* vec, size_t index, const void* data);

/**
 * @brief Removes the element at the given index, shifting later elements.
 *
 * If @p out_data is UCB_NULL and a data_free function is configured, the removed
 * element is freed. Otherwise the caller receives it in @p out_data.
 * @param vec the vector to modify
 * @param index the position to remove
 * @param out_data optional location to receive the removed element
 */
UCB_API void ucb_vector_remove(ucb_vector* vec, size_t index, void* out_data);

/**
 * @brief Appends an element to the end of the vector.
 * @param vec the vector to append to
 * @param data the element to append
 */
UCB_API void ucb_vector_push_back(ucb_vector* vec, const void* data);

/**
 * @brief Prepends an element to the front of the vector.
 * @param vec the vector to prepend to
 * @param data the element to insert
 */
UCB_API void ucb_vector_push_front(ucb_vector* vec, const void* data);

/**
 * @brief Pops the last element from the vector
 *
 * If out_data is NULL and a data_free function is provided, the popped element will be freed.
 * Otherwise, the caller is responsible for freeing the popped element if necessary.
 *
 * @param vec The vector from which to pop the element.
 * @param out_data Optional pointer to store the popped element.
 * @return true if the element was successfully popped, false otherwise.
 */
UCB_API bool ucb_vector_pop_back(ucb_vector* vec, void* out_data);
/**
 * @brief Pops the first element from the vector.
 *
 * If out_data is NULL and a data_free function is provided, the popped element will be freed.
 * Otherwise, the caller is responsible for freeing the popped element if necessary.
 * @param vec the vector from which to pop the element
 * @param out_data optional pointer to store the popped element
 * @return true if the element was successfully popped, false otherwise
 */
UCB_API bool ucb_vector_pop_front(ucb_vector* vec, void* out_data);

/**
 * @brief Returns the last element without removing it.
 * @param vec the vector to query
 * @param out_data pointer to store the peeked element
 * @return true on success, false if the vector is empty
 */
UCB_API bool ucb_vector_peek_back(const ucb_vector* vec, void* out_data);

/**
 * @brief Returns the first element without removing it.
 * @param vec the vector to query
 * @param out_data pointer to store the peeked element
 * @return true on success, false if the vector is empty
 */
UCB_API bool ucb_vector_peek_front(const ucb_vector* vec, void* out_data);

/**
 * @brief Copies the element at the given index into out_data.
 * @param vec the vector to query
 * @param index the index of the element
 * @param out_data pointer to store the element
 * @return true on success, false on invalid arguments
 */
UCB_API bool ucb_vector_get(const ucb_vector* vec, size_t index, void* out_data);

/**
 * @brief Replaces the element at the given index.
 *
 * In owned pointer mode the previous element is freed and the new data cloned.
 * @param vec the vector to modify
 * @param index the index of the element to replace
 * @param data the new element
 */
UCB_API void ucb_vector_set(ucb_vector* vec, size_t index, const void* data);

/**
 * @brief Calls a function for each element in order.
 * @param vec the vector to iterate
 * @param func the callback, returning false to stop iteration
 * @param user_data optional user data passed to the callback
 * @return the index of the element that stopped iteration, or the size if none did
 */
UCB_API size_t ucb_vector_foreach(ucb_vector* vec, ucb_vector_iter_func func, void* user_data);

#ifdef __cplusplus
}
#endif

#endif // UCB_CONTAINER_VECTOR_H
