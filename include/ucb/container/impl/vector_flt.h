#pragma once

/**
 * @file vector_flt.h
 * This file is part of the UCB project
 * - SPDX-FileCopyrightText: © 2026 Åke Svedin <ake@svedin.org>
 * - SPDX-License-Identifier: MIT
 *
 * @brief Vector container for float.
 *
 * @note This is generated from ucb/container/vector/template.h
 */

#include <ucb/container/common.h>
#include <ucb/error.h>
#include <ucb/export.h>

#include <stdbool.h>
#include <stddef.h>

/**
 * @struct ucb_vector_flt
 * @brief vector containing float.
 *
 * This is a generated vector for float.
 * @note The vector does not by default manage the lifetime of the elements. Please review
 * the function documentation for details.
 * @endif
 */
typedef struct ucb_vector_flt
{
    float* data;     ///< pointer to vector data
    size_t size;     ///< number of elements in the vector
    size_t capacity; ///< allocated capacity of the vector
} ucb_vector_flt;

/**
 * @brief Function callback for foreach.
 * @param index The index of the element.
 * @param user_data User data passed to the function.
 * @return true to continue, false to stop iteration.
 */
typedef bool (*ucb_vector_flt_iter_func)(float* pval, size_t index, void* user_data);

/// @name Lifecycle
/// @{

/**
 * @fn ucb_vector_flt_new
 * @brief Creates a new empty vector containing float.
 *
 * Capacity is zero. First insertion will allocate the initial capacity.
 * @return A new vector or @c UCB_NULL if the allocation failed.
 */

/**
 * @fn ucb_vector_flt_free
 * @brief Frees the vector.
 * @param vec The vector to free.
 * @warning The element pointers are not freed.
 * @endif
 */

/**
 * @fn ucb_vector_flt_copy
 * @brief Copies the contents of from one vector to another
 *
 * @p dst must be empty before calling this.
 * @param dst The vector to copy to.
 * @param src The vector to copy from.
 * @return true if the copy was successful, false on memory allocation failure.
 * @note This is a shallow copy.
 * @endif
 */

/**
 * @fn ucb_vector_flt_clone
 * @brief Creates a vector as a copy of the source vector.
 * @param src The original vector to clone.
 * @return A new vector or NULL if the allocation failed.
 * @note This is a shallow clone.
 * @endif
 */

/**
 * @fn ucb_vector_flt_move
 * @brief Moves the contents of one vector to another.
 *
 * @p dst takes ownership of the vector data from @p src.
 * After the move, @p src will be valid but empty with zero size and capacity.
 *
 * @param dst The vector to move to.
 * @param src The vector to move from.
 */

/// @}
/// @name Capacity and size
/// @{

/**
 * @fn ucb_vector_flt_reserve
 * @brief Resizes the internal buffer to hold at least new_capacity number of elements.
 *
 * If @c new_capacity is less than the current capacity, this call is a noop.
 * If @c new_capacity is 0 or less than default capacity, it will default minimum capacity.
 *
 * @param vec The vector to resize.
 * @param new_capacity The desired capacity for the vector data.
 * @return true if the reservation was successful, false if the allocation failed.
 */

/**
 * @fn ucb_vector_flt_capacity
 * @brief Returns the current capacity of the vector in bytes.
 * @param vec The vector to query.
 * @return The capacity of the vector data.
 */

/**
 * @fn ucb_vector_flt_size
 * @brief Returns the number of elements currently stored in the vector.
 * @param vec The vector to query.
 * @return The number of elements in the vector.
 */

/**
 * @fn ucb_vector_flt_is_empty
 * @brief Returns true if the vector contains no elements.
 * @param vec The vector to check.
 * @return true if the vector is empty, false otherwise.
 */

/**
 * @fn ucb_vector_flt_clear
 * @brief Clears the vector by setting size to 0.
 * @param vec The vector to clear.
 * @warning The element pointers are not freed.
 * @endif
 */

/**
 * @fn ucb_vector_flt_fit
 * @brief Reduces the vector capacity to exactly match its current size, if capacity > size.
 * @param vec The vector to fit.
 */

/// @}
/// @name Element access and insertion
/// @{

/**
 * @fn ucb_vector_flt_insert
 * @brief Inserts data at the specified index, shifting existing elements to make space.
 * @param vec The vector to modify.
 * @param index The position at which to insert the element.
 * @param data The element to insert.
 * @note The index can be equal to the current size, effectively appending the element.
 * @note The element pointer is inserted as-is and must remain valid through the lifetime of the
 * container.
 * @endif
 */

/**
 * @fn ucb_vector_flt_remove
 * @brief Removes the element at the specified index, shifting subsequent elements to fill the gap.
 * @param vec The vector to modify.
 * @param index The position of the element to remove.
 * @return the element at the specified index.
 * @warning The element is not freed. The caller must manually free the returned pointer.
 * @endif
 */

/**
 * @fn ucb_vector_flt_push_back
 * @brief Appends an element to the end of the vector, resizing if necessary.
 * @param vec The vector to append to.
 * @param data The element to append.
 * @note The element pointer is inserted as-is and must remain valid through the lifetime of the
 * container.
 * @endif
 */

/**
 * @fn ucb_vector_flt_push_front
 * @brief Inserts an element at the front of the vector, shifting existing elements.
 * @param vec The vector to modify.
 * @param data The element to insert.
 * @note The element pointer is inserted as-is and must remain valid through the lifetime of the
 * container.
 * @endif
 */

/**
 * @fn ucb_vector_flt_pop_back
 * @brief Removes and returns the last element of the vector.
 * @param vec The vector to pop from.
 * @return The element that was removed.
 * @warning The element is not freed. The caller must manually free the returned pointer.
 * @endif
 */

/**
 * @fn ucb_vector_flt_pop_front
 * @brief Removes and returns the first element of the vector.
 * @param vec The vector to pop from.
 * @return The element that was removed.
 * @warning The element is not freed. The caller must manually free the returned pointer.
 * @endif
 */

/**
 * @fn ucb_vector_flt_peek_back
 * @brief Returns a reference to the last element without modifying the vector.
 * @param vec The vector to peek.
 * @return A reference to the last element.
 * @note The pointer to the element is returned and must not be freed by the caller.
 * @endif
 */

/**
 * @fn ucb_vector_flt_peek_front
 * @brief Returns a reference to the first element without modifying the vector.
 * @param vec The vector to peek.
 * @return A reference to the first element.
 * @note The pointer to the element is returned and must not be freed by the caller.
 * @endif
 */

/**
 * @fn ucb_vector_flt_get
 * @brief Returns a reference to the element at the specified index.
 * @param vec The vector to query.
 * @param index The index of the element to retrieve.
 * @return A reference to the element at index.
 * @note The pointer to the element is returned and must not be freed by the caller.
 * @endif
 */

/**
 * @fn ucb_vector_flt_set
 * @brief Sets the element at the specified index to the given data.
 * @param vec The vector to modify.
 * @param index The index of the element to set.
 * @param data The new value for the element.
 * @note The element pointer is set as-is and must remain valid through the lifetime of the
 * container. The previous pointer is not freed.
 * @endif
 */

/**
 * @fn ucb_vector_flt_swap
 * @brief Swap position of two elements in the vector.
 * @param vec The vector to modify.
 * @param idx1 The index of the first element.
 * @param idx2 The index of the second element.
 */

/**
 * @fn ucb_vector_flt_foreach
 * @brief Applies a function to each element of the vector.
 * @param vec The vector to iterate over.
 * @param func The function to apply to each element.
 * @param user_data Optional user data passed to the function.
 * @return number of elements processed.
 */

/// @}
/// @name Find and sort
/// @{

/**
 * @fn ucb_vector_flt_sort
 * @brief Sorts the vector using the default comparison function.
 * @param vec The vector to sort.
 */

/**
 * @fn ucb_vector_flt_sort_with
 * @brief Sort the vector using a custom comparison function.
 * @param vec The vector to sort.
 * @param cmp The comparison function to use.
 */

/**
 * @fn ucb_vector_flt_insert_sorted
 * @brief Inserts an element into the vector in sorted order using the default comparison function.
 *
 * The vector must be sorted with the same comparison function before calling this function or the
 * result is undefined.
 *
 * @param vec The vector to insert into.
 * @param element The element to insert.
 * @return the index of the inserted element
 */

/**
 * @fn UCB_VECTOR_T_INSERT_SORT
 * @brief Insert an element into the vector in sorted order using a custom comparison function.
 *
 * The vector must be sorted with the same comparison function before calling this function or the
 * result is undefined.
 *
 * @param vec The vector to insert into.
 * @param elem The element to insert.
 * @param cmp The comparison function to use.
 * @return the index of the inserted element
 */

/**
 * @fn ucb_vector_flt_find
 * @brief Finds the first element in the vector that matches the default comparison function.
 *
 * The vector must be sorted with the same comparison function before calling this function or the
 * result is undefined.
 *
 * If the item is found, the returned position will be the right-most position of all matching
 * items.
 *
 * If the item is not found, the returned value will be -(insert_pos + 1). Which can be used to
 * insert the item into the sorted position.
 *
 * @param vec The vector to search.
 * @param elem The element to find.
 * @return the index of the element or < 0 if not found.
 */

/**
 * @fn ucb_vector_flt_find_with
 * @brief Finds the first element in the vector that matches the given value using a custom
 * comparison function.
 *
 * The vector must be sorted with the same comparison function before calling this function or the
 * result is undefined.
 *
 * If the item is found, the returned position will be the right-most position of all matching
 * items.
 *
 * If the item is not found, the returned value will be -(insert_pos + 1). Which can be used to
 * insert the item into the sorted position.
 *
 * @param vec The vector to search.
 * @param elem The element to find.
 * @param cmp The comparison function to use.
 * @return the index of the element or < 0 if not found.
 */

/// @}

/* -------------------------------- Lifecycle ------------------------------- */

UCB_API ucb_vector_flt* ucb_vector_flt_new(void);

UCB_API void ucb_vector_flt_free(ucb_vector_flt* vec);

UCB_API bool ucb_vector_flt_copy(ucb_vector_flt* dst, const ucb_vector_flt* src);

UCB_API ucb_vector_flt* ucb_vector_flt_clone(const ucb_vector_flt* src);

UCB_API void ucb_vector_flt_move(ucb_vector_flt* dst, ucb_vector_flt* src);

/* ---------------------------- Capacity and size --------------------------- */

UCB_API bool ucb_vector_flt_reserve(ucb_vector_flt* vec, size_t new_capacity);

static inline size_t ucb_vector_flt_capacity(const ucb_vector_flt* vec)
{
    UCB_VERIFY_ARGS(vec);
    return vec->capacity;
}

static inline size_t ucb_vector_flt_size(const ucb_vector_flt* vec)
{
    UCB_VERIFY_ARGS(vec);
    return vec->size;
}

static inline bool ucb_vector_flt_is_empty(const ucb_vector_flt* vec)
{
    UCB_VERIFY_ARGS(vec);
    return vec->size == 0;
}

static inline void ucb_vector_flt_clear(ucb_vector_flt* vec)
{
    UCB_VERIFY_ARGS(vec);
    vec->size = 0;
}

UCB_API void ucb_vector_flt_fit(ucb_vector_flt* vec);

/* --------------------- Element access and modification -------------------- */

UCB_API void ucb_vector_flt_insert(ucb_vector_flt* vec, size_t index, float data);

UCB_API float ucb_vector_flt_remove(ucb_vector_flt* vec, size_t index);

UCB_API void ucb_vector_flt_push_back(ucb_vector_flt* vec, float data);

UCB_API void ucb_vector_flt_push_front(ucb_vector_flt* vec, float data);

UCB_API float ucb_vector_flt_pop_back(ucb_vector_flt* vec);

UCB_API float ucb_vector_flt_pop_front(ucb_vector_flt* vec);

static inline float ucb_vector_flt_peek_back(const ucb_vector_flt* vec)
{
    UCB_VERIFY_ARGS(vec && vec->size);
    return vec->data[vec->size - 1];
}

static inline float ucb_vector_flt_peek_front(const ucb_vector_flt* vec)
{
    UCB_VERIFY_ARGS(vec && vec->size);
    return vec->data[0];
}

static inline float ucb_vector_flt_get(const ucb_vector_flt* vec, size_t index)
{
    UCB_VERIFY_ARGS(vec && index < vec->size);
    return vec->data[index];
}

static inline void ucb_vector_flt_set(ucb_vector_flt* vec, size_t index, float data)
{
    UCB_VERIFY_ARGS(vec && index < vec->size);
    vec->data[index] = data;
}

static inline void ucb_vector_flt_swap(ucb_vector_flt* vec, size_t idx1, size_t idx2)
{
    UCB_VERIFY_ARGS(vec && idx1 < vec->size && idx2 < vec->size && idx1 != idx2);
    float tmp = vec->data[idx1];
    vec->data[idx1] = vec->data[idx2];
    vec->data[idx2] = tmp;
}

UCB_API size_t ucb_vector_flt_foreach(ucb_vector_flt* vec,
                                      ucb_vector_flt_iter_func func,
                                      void* user_data);

/* ------------------------------ Find and sort ----------------------------- */

UCB_API void ucb_vector_flt_sort(ucb_vector_flt* vec);

UCB_API void ucb_vector_flt_sort_with(ucb_vector_flt* vec, ucb_cmp_func func);

UCB_API size_t ucb_vector_flt_insert_sorted(ucb_vector_flt* vec, float val);

UCB_API size_t ucb_vector_flt_insert_sorted_with(ucb_vector_flt* vec, float val, ucb_cmp_func func);

UCB_API ucb_ssize ucb_vector_flt_find(const ucb_vector_flt* vec, const float* val);

UCB_API ucb_ssize ucb_vector_flt_find_with(const ucb_vector_flt* vec,
                                           const float* val,
                                           ucb_cmp_func func);
