// NO HEADER GUARD - this file is meant to be included multiple times with different T definitions

/**
 * @file vector_template.h
 * This file is part of the UCB project
 * - SPDX-FileCopyrightText: © 2026 Åke Svedin <ake@svedin.org>
 * - SPDX-License-Identifier: MIT
 *
 * @brief Vector template header providing type-specialized operations.
 *
 * This header is meant to be included multiple times with different T definitions.
 * It generates type-specialized vectors and functions as a regular header and implementation
 * file.
 *
 * The container supports PODs (primitives, trivial structs) and pointers. Complex structs are only
 * supported as pointers.
 *
 * For PODs, memory management is implicit.
 *
 * For pointers the container doesn't manage the memory by default, leaving the choice to clone or
 * free up to the caller. If the additional *FUNC macros are defined. Extra functions will be
 * generated as a convenience. See each functions description for details.
 *
 * A small subset of the functions will be declared as static inline in the header for better
 * performance of trivial operations.
 *
 * Define the following macros before inclusion:
 * - Either:
 *   - UCB_T_DECLARE @c type - Define the type and declare functions/define as static inline.
 *   - UCB_T_DEFINE @c type  - Define the remaining functions.
 * - Optional:
 *   - UCB_T_SUFFIX     - Suffix to use in function names. Defaults to @c type given from
 *                        UCB_T_DECLARE etc.
 *   - UCB_T_NO_INLINE  - If defined, will not declare functions as static inline.
 *   - UCB_T_EXPORT     - Export macro for function declarations. Defaults to nothing.
 *   - UCB_T_DEBUG      - If defined, will print messages about which type is being
 *                        declared/defined.
 *   - UCB_T_IS_POD     - Type is a POD type (primitive or simple struct). If not set,
 *                        the type is considered a pointer type.
 * - Optional (for memory management, pointer only):
 *   - UCB_T_FREE_FUNC  - Function to free an element. Generates the functions:
 *     - free_full
 *     - clear_full
 *   - UCB_T_COPY_FUNC  - Function to copy an element. Generates the function:
 *     - clone_full
 *     - copy_full
 * - Optional (for sorting and finding):
 *   - UCB_T_CMP_FUNC  - Function to compare two elements. Generates the functions:
 *     - sort
 *     - find
 *     - insert_sorted
 *
 * Signatures of provided functions (where T holds the complete type, including pointer):
 * - UCB_T_FREE_FUNC: void (*)(T element) (or void (*)(void* element)
 * - UCB_T_COPY_FUNC: void (*)(T dst, const T src)
 *   - The copy function must be able to operate on a fully zeroed destination.
 * - UCB_T_CMP_FUNC: int (*)(const T a, const T b)
 *   - Where return value is 0 if equal, negative if a < b, positive if a > b.
 *
 * All of the above macros are undefined after inclusion, except for UCB_T_DEBUG.
 *
 * The generated type will be named @c ucb_vector_<suffix>, and functions will be named like
 * @c ucb_vector_<suffix>_new, etc.
 *
 * The container is not thread-safe.
 *
 * Example usage patterns:
 *
 * Example 1: Declaring an @c ucb_vector_int in a header file.
 * @code{.c}
 * #define UCB_T_DECLARE int
 * #define UCB_T_IS_POD
 * #include <ucb/container/templates/vector_template.h>
 * @endcode
 *
 * Example 2: Defining an @c ucb_vector_int in a source file.
 * @code{.c}
 * #define UCB_T_DEFINE int
 * #define UCB_T_IS_POD
 * #include <ucb/container/templates/vector_template.h>
 * @endcode
 *
 * Example 3: Defining a @c ucb_vector_MyStruct using custom suffix for a pointer type.
 *            Includes memory management functions.
 * @code{.c}
 * #define UCB_T_DECLARE struct MyStruct*
 * #define UCB_T_SUFFIX MyStruct
 * #define UCB_T_FREE_FUNC my_struct_free
 * #define UCB_T_COPY_FUNC my_struct_clone
 * #define UCB_T_CMP_FUNC my_struct_cmp
 * #include <ucb/container/templates/vector_template.h>
 * @endcode
 */

/* -------------------------------------------------------------------------- */
/*                                   Defines                                  */
/* -------------------------------------------------------------------------- */

#ifndef UCB_VECTOR_T_DEFAULT_CAPACITY
#define UCB_VECTOR_T_DEFAULT_CAPACITY 8
#endif

/* -------------------------------------------------------------------------- */
/*                               Initialization                               */
/* -------------------------------------------------------------------------- */

#if defined(UCB_DEVEL) && !defined(UCB_T_DEBUG)
#define UCB_T_DEBUG 1
#endif

#if defined(__INTELLISENSE__)

// Dummy definitions for better code completion in IDEs
void _fake_free(void*);
void _fake_copy(void*, const void*);
int _fake_cmp(const void*, const void*);

#define _UCB_T void*
#define UCB_T_SUFFIX ptr
#define UCB_T_DECLARE
#define UCB_T_DEFINE
#define UCB_T_FREE_FUNC _fake_free
#define UCB_T_COPY_FUNC _fake_copy
#define UCB_T_CMP_FUNC _fake_cmp

#elif defined(UCB_T_DECLARE)

#define _UCB_T UCB_T_DECLARE
#ifdef UCB_T_DEFINE
#error Only one of UCB_T_DECLARE or UCB_T_DEFINE can be defined before inclusiong.
#endif

#elif defined(UCB_T_DEFINE)

#define _UCB_T UCB_T_DEFINE

#else
#error You must define UCB_T_DECLARE or UCB_T_DEFINE before including this template header
#endif

#ifndef UCB_T_SUFFIX
#define UCB_T_SUFFIX _UCB_T
#endif

#ifndef UCB_T_EXPORT
#define UCB_T_EXPORT
#endif

#ifdef UCB_T_IS_POD
#define _UCB_T_CONST
#define _UCB_T_PTR _UCB_T*
#define _UCB_T_IS_POD 1
#else
#define _UCB_T_CONST const
#define _UCB_T_PTR _UCB_T
#define _UCB_T_IS_POD 0
#endif

// Includes needed for this template header
#ifndef _GEN_TEMPLATE_PARSER
#include <ucb/defines.h>
#endif

// Internal macros - will be undefined at the end

#define _UCB_T_FUNC(func) UCB_SNAKE(ucb_vector, UCB_SNAKE(UCB_T_SUFFIX, func))
#define _UCB_T_TYPE UCB_SNAKE(ucb_vector, UCB_T_SUFFIX)

// Debug printing

#ifdef UCB_T_DEBUG

#if defined(UCB_T_DECLARE)
// Pragmas messages are too annoyingly verbose on GCC...
#ifdef _MSC_VER
#pragma message("Declaring " UCB_STRINGIFY(_UCB_T_TYPE) " with element type " UCB_STRINGIFY(_UCB_T))
#endif
#endif
#if defined(UCB_T_DEFINE)
#ifdef _MSC_VER
#pragma message("Defining " UCB_STRINGIFY(_UCB_T_TYPE) " with element type " UCB_STRINGIFY(_UCB_T))
#endif
#endif

#endif // UCB_T_DEBUG

/* -------------------------------------------------------------------------- */
/*                             File documentation                             */
/* -------------------------------------------------------------------------- */

#ifdef _GEN_TEMPLATE_PARSER
#ifdef UCB_T_DECLARE
#pragma once
#endif

/**
 * @file _GEN_FILENAME
 * This file is part of the UCB project
 * - SPDX-FileCopyrightText: © 2026 Åke Svedin <ake@svedin.org>
 * - SPDX-License-Identifier: MIT
 *
 * @brief Vector container for type T.
 *
 * @note This is generated from ucb/container/vector/template.h
 */

#endif

/* -------------------------------------------------------------------------- */
/*                               Header includes                              */
/* -------------------------------------------------------------------------- */

#ifdef UCB_T_DECLARE
#include <ucb/container/common.h>
#ifndef UCB_T_NO_INLINE
#include <ucb/error.h>
#endif

#include <stdbool.h>
#include <stddef.h>
#endif

#ifdef UCB_T_DEFINE
#include <ucb/error.h>
#include <ucb/memory.h>
#ifndef UCB_T_IS_POD
#include <ucb/algorithm.h>
#endif

#include <stdlib.h>
#include <string.h>
#endif

/* -------------------------------------------------------------------------- */
/*                               Type definition                              */
/* -------------------------------------------------------------------------- */

#ifdef UCB_T_DECLARE

/**
 * @struct _UCB_T_TYPE
 * @brief vector containing type T.
 *
 * This is a generated vector for type T.
 * @if !_UCB_T_IS_POD
 * @note The vector does not by default manage the lifetime of the elements. Please review
 * the function documentation for details.
 * @endif
 */
typedef struct _UCB_T_TYPE
{
    _UCB_T* data;    ///< pointer to vector data
    size_t size;     ///< number of elements in the vector
    size_t capacity; ///< allocated capacity of the vector
} _UCB_T_TYPE;

/**
 * @brief Function callback for foreach.
 * @if _UCB_T_IS_POD
 * @param pval The element pointer to process.
 * @else
 * @param val The element to process.
 * @endif
 * @param index The index of the element.
 * @param user_data User data passed to the function.
 * @return true to continue, false to stop iteration.
 */
#ifdef UCB_T_IS_POD
typedef bool (*_UCB_T_FUNC(iter_func))(_UCB_T* pval, size_t index, void* user_data);
#else
typedef bool (*_UCB_T_FUNC(iter_func))(_UCB_T val, size_t index, void* user_data);
#endif

#endif // UCB_T_DECLARE

/* -------------------------------------------------------------------------- */
/*                           Function documentation                           */
/* -------------------------------------------------------------------------- */
/*
 * Create a macro for each function to provide a base documentation.
 * This works with some IDEs (like VS Code) to provide inline help.
 *
 * Only public functions need to be declared as separate macros.
 *
 * Since this is parsed by python, the macros must be defined on a single line.
 */

#if defined(UCB_T_DECLARE) || defined(UCB_T_DEFINE)

/// @name Lifecycle
/// @{

/**
 * @def UCB_VECTOR_T_NEW
 * @brief Creates a new empty vector containing type T.
 *
 * Capacity is zero. First insertion will allocate the initial capacity.
 * @return A new vector or @c UCB_NULL if the allocation failed.
 */
#define UCB_VECTOR_T_NEW _UCB_T_FUNC(new)

/**
 * @def UCB_VECTOR_T_FREE
 * @brief Frees the vector.
 * @param vec The vector to free.
 * @if !_UCB_T_IS_POD
 * @warning The element pointers are not freed.
 * @endif
 */
#define UCB_VECTOR_T_FREE _UCB_T_FUNC(free)

#if defined(UCB_T_FREE_FUNC)
/**
 * @brief Frees the vector and the contents of its elements.
 * @param vec The vector to free.
 */
#define UCB_VECTOR_T_FREE_FULL _UCB_T_FUNC(free_full)
#endif

/**
 * @def UCB_VECTOR_T_COPY
 * @brief Copies the contents of from one vector to another
 *
 * @p dst must be empty before calling this.
 * @param dst The vector to copy to.
 * @param src The vector to copy from.
 * @return true if the copy was successful, false on memory allocation failure.
 * @if !_UCB_T_IS_POD
 * @note This is a shallow copy.
 * @endif
 */
#define UCB_VECTOR_T_COPY _UCB_T_FUNC(copy)

#if defined(UCB_T_COPY_FUNC)
/**
 * @def UCB_VECTOR_T_COPY_FULL
 * @brief Deep copies the contents with elements from one vector to another.
 *
 * @p dst must be empty before calling this.
 * @param dst The vector to copy to.
 * @param src The vector to copy from.
 * @return true if the copy was successful, false on memory allocation failure.
 */
#define UCB_VECTOR_T_COPY_FULL _UCB_T_FUNC(copy_full)
#endif

/**
 * @def UCB_VECTOR_T_CLONE
 * @brief Creates a vector as a copy of the source vector.
 * @param src The original vector to clone.
 * @return A new vector or NULL if the allocation failed.
 * @if !_UCB_T_IS_POD
 * @note This is a shallow clone.
 * @endif
 */
#define UCB_VECTOR_T_CLONE _UCB_T_FUNC(clone)

#if defined(UCB_T_COPY_FUNC)
/**
 * @def UCB_VECTOR_T_CLONE_FULL
 * @brief Creates a vector as a deep copy of the source vector.
 * @param src The original vector to clone.
 * @return A new vector or NULL if the allocation failed.
 */
#define UCB_VECTOR_T_CLONE_FULL _UCB_T_FUNC(clone_full)
#endif

/**
 * @def UCB_VECTOR_T_MOVE
 * @brief Moves the contents of one vector to another.
 *
 * @p dst takes ownership of the vector data from @p src.
 * After the move, @p src will be valid but empty with zero size and capacity.
 *
 * @param dst The vector to move to.
 * @param src The vector to move from.
 */
#define UCB_VECTOR_T_MOVE _UCB_T_FUNC(move)

/// @}
/// @name Capacity and size
/// @{

/**
 * @def UCB_VECTOR_T_RESERVE
 * @brief Resizes the internal buffer to hold at least new_capacity number of elements.
 *
 * If @c new_capacity is less than the current capacity, this call is a noop.
 * If @c new_capacity is 0 or less than default capacity, it will default minimum capacity.
 *
 * @param vec The vector to resize.
 * @param new_capacity The desired capacity for the vector data.
 * @return true if the reservation was successful, false if the allocation failed.
 */
#define UCB_VECTOR_T_RESERVE _UCB_T_FUNC(reserve)

/**
 * @def UCB_VECTOR_T_CAPACITY
 * @brief Returns the current capacity of the vector in bytes.
 * @param vec The vector to query.
 * @return The capacity of the vector data.
 */
#define UCB_VECTOR_T_CAPACITY _UCB_T_FUNC(capacity)

/**
 * @def UCB_VECTOR_T_SIZE
 * @brief Returns the number of elements currently stored in the vector.
 * @param vec The vector to query.
 * @return The number of elements in the vector.
 */
#define UCB_VECTOR_T_SIZE _UCB_T_FUNC(size)

/**
 * @def UCB_VECTOR_T_IS_EMPTY
 * @brief Returns true if the vector contains no elements.
 * @param vec The vector to check.
 * @return true if the vector is empty, false otherwise.
 */
#define UCB_VECTOR_T_IS_EMPTY _UCB_T_FUNC(is_empty)

/**
 * @def UCB_VECTOR_T_CLEAR
 * @brief Clears the vector by setting size to 0.
 * @param vec The vector to clear.
 * @if !_UCB_T_IS_POD
 * @warning The element pointers are not freed.
 * @endif
 */
#define UCB_VECTOR_T_CLEAR _UCB_T_FUNC(clear)

#if defined(UCB_T_FREE_FUNC)
/**
 * @def UCB_VECTOR_T_CLEAR_FULL
 * @brief Clears the vector by setting size to 0 and freeing all elements.
 * @param vec The vector to clear.
 */
#define UCB_VECTOR_T_CLEAR_FULL _UCB_T_FUNC(clear_deep)
#endif

/**
 * @def UCB_VECTOR_T_FIT
 * @brief Reduces the vector capacity to exactly match its current size, if capacity > size.
 * @param vec The vector to fit.
 */
#define UCB_VECTOR_T_FIT _UCB_T_FUNC(fit)

/// @}
/// @name Element access and insertion
/// @{

/**
 * @def UCB_VECTOR_T_INSERT
 * @brief Inserts data at the specified index, shifting existing elements to make space.
 * @param vec The vector to modify.
 * @param index The position at which to insert the element.
 * @param data The element to insert.
 * @note The index can be equal to the current size, effectively appending the element.
 * @if !_UCB_T_IS_POD
 * @note The element pointer is inserted as-is and must remain valid through the lifetime of the
 * container.
 * @endif
 */
#define UCB_VECTOR_T_INSERT _UCB_T_FUNC(insert)

/**
 * @def UCB_VECTOR_T_REMOVE
 * @brief Removes the element at the specified index, shifting subsequent elements to fill the gap.
 * @param vec The vector to modify.
 * @param index The position of the element to remove.
 * @return the element at the specified index.
 * @if !_UCB_T_IS_POD
 * @warning The element is not freed. The caller must manually free the returned pointer.
 * @endif
 */
#define UCB_VECTOR_T_REMOVE _UCB_T_FUNC(remove)

/**
 * @def UCB_VECTOR_T_PUSH_BACK
 * @brief Appends an element to the end of the vector, resizing if necessary.
 * @param vec The vector to append to.
 * @param data The element to append.
 * @if !_UCB_T_IS_POD
 * @note The element pointer is inserted as-is and must remain valid through the lifetime of the
 * container.
 * @endif
 */
#define UCB_VECTOR_T_PUSH_BACK _UCB_T_FUNC(push_back)

/**
 * @def UCB_VECTOR_T_PUSH_FRONT
 * @brief Inserts an element at the front of the vector, shifting existing elements.
 * @param vec The vector to modify.
 * @param data The element to insert.
 * @if !_UCB_T_IS_POD
 * @note The element pointer is inserted as-is and must remain valid through the lifetime of the
 * container.
 * @endif
 */
#define UCB_VECTOR_T_PUSH_FRONT _UCB_T_FUNC(push_front)

/**
 * @def UCB_VECTOR_T_POP_BACK
 * @brief Removes and returns the last element of the vector.
 * @param vec The vector to pop from.
 * @return The element that was removed.
 * @if !_UCB_T_IS_POD
 * @warning The element is not freed. The caller must manually free the returned pointer.
 * @endif
 */
#define UCB_VECTOR_T_POP_BACK _UCB_T_FUNC(pop_back)

/**
 * @def UCB_VECTOR_T_POP_FRONT
 * @brief Removes and returns the first element of the vector.
 * @param vec The vector to pop from.
 * @return The element that was removed.
 * @if !_UCB_T_IS_POD
 * @warning The element is not freed. The caller must manually free the returned pointer.
 * @endif
 */
#define UCB_VECTOR_T_POP_FRONT _UCB_T_FUNC(pop_front)

/**
 * @def UCB_VECTOR_T_PEEK_BACK
 * @brief Returns a reference to the last element without modifying the vector.
 * @param vec The vector to peek.
 * @return A reference to the last element.
 * @if !_UCB_T_IS_POD
 * @note The pointer to the element is returned and must not be freed by the caller.
 * @endif
 */
#define UCB_VECTOR_T_PEEK_BACK _UCB_T_FUNC(peek_back)

/**
 * @def UCB_VECTOR_T_PEEK_FRONT
 * @brief Returns a reference to the first element without modifying the vector.
 * @param vec The vector to peek.
 * @return A reference to the first element.
 * @if !_UCB_T_IS_POD
 * @note The pointer to the element is returned and must not be freed by the caller.
 * @endif
 */
#define UCB_VECTOR_T_PEEK_FRONT _UCB_T_FUNC(peek_front)

/**
 * @def UCB_VECTOR_T_GET
 * @brief Returns a reference to the element at the specified index.
 * @param vec The vector to query.
 * @param index The index of the element to retrieve.
 * @return A reference to the element at index.
 * @if !_UCB_T_IS_POD
 * @note The pointer to the element is returned and must not be freed by the caller.
 * @endif
 */
#define UCB_VECTOR_T_GET _UCB_T_FUNC(get)

/**
 * @def UCB_VECTOR_T_SET
 * @brief Sets the element at the specified index to the given data.
 * @param vec The vector to modify.
 * @param index The index of the element to set.
 * @param data The new value for the element.
 * @if !_UCB_T_IS_POD
 * @note The element pointer is set as-is and must remain valid through the lifetime of the
 * container. The previous pointer is not freed.
 * @endif
 */
#define UCB_VECTOR_T_SET _UCB_T_FUNC(set)

/**
 * @def UCB_VECTOR_T_SWAP
 * @brief Swap position of two elements in the vector.
 * @param vec The vector to modify.
 * @param idx1 The index of the first element.
 * @param idx2 The index of the second element.
 */
#define UCB_VECTOR_T_SWAP _UCB_T_FUNC(swap)

/**
 * @def UCB_VECTOR_T_FOREACH
 * @brief Applies a function to each element of the vector.
 * @param vec The vector to iterate over.
 * @param func The function to apply to each element.
 * @param user_data Optional user data passed to the function.
 * @return number of elements processed.
 * @if _UCB_T_IS_POD
 * @note The function receives a pointer to the element.
 * @endif
 */
#define UCB_VECTOR_T_FOREACH _UCB_T_FUNC(foreach)

/// @}
/// @name Find and sort
/// @{

#ifdef UCB_T_CMP_FUNC
/**
 * @def UCB_VECTOR_T_SORT
 * @brief Sorts the vector using the default comparison function.
 * @param vec The vector to sort.
 */
#define UCB_VECTOR_T_SORT _UCB_T_FUNC(sort)
#endif

/**
 * @def UCB_VECTOR_T_SORT_WITH
 * @brief Sort the vector using a custom comparison function.
 * @param vec The vector to sort.
 * @param cmp The comparison function to use.
 */
#define UCB_VECTOR_T_SORT_WITH _UCB_T_FUNC(sort_with)

#ifdef UCB_T_CMP_FUNC
/**
 * @def UCB_VECTOR_T_INSERT_SORTED
 * @brief Inserts an element into the vector in sorted order using the default comparison function.
 *
 * The vector must be sorted with the same comparison function before calling this function or the
 * result is undefined.
 *
 * @param vec The vector to insert into.
 * @param element The element to insert.
 * @return the index of the inserted element
 */
#define UCB_VECTOR_T_INSERT_SORTED _UCB_T_FUNC(insert_sorted)
#endif

/**
 * @def UCB_VECTOR_T_INSERT_SORTED_WITH
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
#define UCB_VECTOR_T_INSERT_SORTED_WITH _UCB_T_FUNC(insert_sorted_with)

#ifdef UCB_T_CMP_FUNC
/**
 * @def UCB_VECTOR_T_FIND
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
#define UCB_VECTOR_T_FIND _UCB_T_FUNC(find)
#endif

/**
 * @def UCB_VECTOR_T_FIND_WITH
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
#define UCB_VECTOR_T_FIND_WITH _UCB_T_FUNC(find_with)

/// @}

#endif // UCB_T_DECLARE || UCB_T_DEFINE

/* -------------------------------------------------------------------------- */
/*                            Function declarations                           */
/* -------------------------------------------------------------------------- */

#ifdef UCB_T_DECLARE

/* -------------------------------- Lifecycle ------------------------------- */

UCB_T_EXPORT _UCB_T_TYPE* UCB_VECTOR_T_NEW(void);

UCB_T_EXPORT void UCB_VECTOR_T_FREE(_UCB_T_TYPE* vec);

#if defined(UCB_T_FREE_FUNC)
UCB_T_EXPORT void UCB_VECTOR_T_FREE_FULL(_UCB_T_TYPE* vec);
#endif

UCB_T_EXPORT bool UCB_VECTOR_T_COPY(_UCB_T_TYPE* dst, const _UCB_T_TYPE* src);

#if defined(UCB_T_COPY_FUNC)
UCB_T_EXPORT bool UCB_VECTOR_T_COPY_FULL(_UCB_T_TYPE* dst, const _UCB_T_TYPE* src);
#endif

UCB_T_EXPORT _UCB_T_TYPE* UCB_VECTOR_T_CLONE(const _UCB_T_TYPE* src);

#if defined(UCB_T_COPY_FUNC)
UCB_T_EXPORT _UCB_T_TYPE* UCB_VECTOR_T_CLONE_FULL(const _UCB_T_TYPE* src);
#endif

UCB_T_EXPORT void UCB_VECTOR_T_MOVE(_UCB_T_TYPE* dst, _UCB_T_TYPE* src);

/* ---------------------------- Capacity and size --------------------------- */

UCB_T_EXPORT bool UCB_VECTOR_T_RESERVE(_UCB_T_TYPE* vec, size_t new_capacity);

#ifdef UCB_T_NO_INLINE
UCB_T_EXPORT size_t UCB_VECTOR_T_CAPACITY(const _UCB_T_TYPE* vec);
#else
static inline size_t UCB_VECTOR_T_CAPACITY(const _UCB_T_TYPE* vec)
{
    UCB_VERIFY_ARGS(vec);
    return vec->capacity;
}
#endif // UCB_T_NO_INLINE

#ifdef UCB_T_NO_INLINE
UCB_T_EXPORT size_t UCB_VECTOR_T_SIZE(const _UCB_T_TYPE* vec);
#else
static inline size_t UCB_VECTOR_T_SIZE(const _UCB_T_TYPE* vec)
{
    UCB_VERIFY_ARGS(vec);
    return vec->size;
}
#endif // UCB_T_NO_INLINE

#ifdef UCB_T_NO_INLINE
UCB_T_EXPORT bool UCB_VECTOR_T_IS_EMPTY(const _UCB_T_TYPE* vec);
#else
static inline bool UCB_VECTOR_T_IS_EMPTY(const _UCB_T_TYPE* vec)
{
    UCB_VERIFY_ARGS(vec);
    return vec->size == 0;
}
#endif // UCB_T_NO_INLINE

#ifdef UCB_T_NO_INLINE
UCB_T_EXPORT void UCB_VECTOR_T_CLEAR(_UCB_T_TYPE* vec);
#else
static inline void UCB_VECTOR_T_CLEAR(_UCB_T_TYPE* vec)
{
    UCB_VERIFY_ARGS(vec);
    vec->size = 0;
}
#endif

#if defined(UCB_T_FREE_FUNC)
UCB_T_EXPORT void UCB_VECTOR_T_CLEAR_FULL(_UCB_T_TYPE* vec);
#endif

UCB_T_EXPORT void UCB_VECTOR_T_FIT(_UCB_T_TYPE* vec);

/* --------------------- Element access and modification -------------------- */

UCB_T_EXPORT void UCB_VECTOR_T_INSERT(_UCB_T_TYPE* vec, size_t index, _UCB_T data);

UCB_T_EXPORT _UCB_T UCB_VECTOR_T_REMOVE(_UCB_T_TYPE* vec, size_t index);

UCB_T_EXPORT void UCB_VECTOR_T_PUSH_BACK(_UCB_T_TYPE* vec, _UCB_T data);

UCB_T_EXPORT void UCB_VECTOR_T_PUSH_FRONT(_UCB_T_TYPE* vec, _UCB_T data);

UCB_T_EXPORT _UCB_T UCB_VECTOR_T_POP_BACK(_UCB_T_TYPE* vec);

UCB_T_EXPORT _UCB_T UCB_VECTOR_T_POP_FRONT(_UCB_T_TYPE* vec);

#ifdef UCB_T_NO_INLINE
UCB_T_EXPORT _UCB_T UCB_VECTOR_T_PEEK_BACK(const _UCB_T_TYPE* vec);
#else  // UCB_T_NO_INLINE
static inline _UCB_T_CONST _UCB_T UCB_VECTOR_T_PEEK_BACK(const _UCB_T_TYPE* vec)
{
    UCB_VERIFY_ARGS(vec && vec->size);
    return vec->data[vec->size - 1];
}
#endif // UCB_T_NO_INLINE

#ifdef UCB_T_NO_INLINE
UCB_T_EXPORT _UCB_T UCB_VECTOR_T_PEEK_FRONT(const _UCB_T_TYPE* vec);
#else
static inline _UCB_T UCB_VECTOR_T_PEEK_FRONT(const _UCB_T_TYPE* vec)
{
    UCB_VERIFY_ARGS(vec && vec->size);
    return vec->data[0];
}
#endif // UCB_T_NO_INLINE

#ifdef UCB_T_NO_INLINE
UCB_T_EXPORT _UCB_T UCB_VECTOR_T_GET(const _UCB_T_TYPE* vec, size_t index);
#else
static inline _UCB_T UCB_VECTOR_T_GET(const _UCB_T_TYPE* vec, size_t index)
{
    UCB_VERIFY_ARGS(vec && index < vec->size);
    return vec->data[index];
}
#endif // UCB_T_NO_INLINE

#ifdef UCB_T_NO_INLINE
UCB_T_EXPORT void UCB_VECTOR_T_SET(_UCB_T_TYPE* vec, size_t index, _UCB_T data);
#else
static inline void UCB_VECTOR_T_SET(_UCB_T_TYPE* vec, size_t index, _UCB_T data)
{
    UCB_VERIFY_ARGS(vec && index < vec->size);
    vec->data[index] = data;
}
#endif // UCB_T_NO_INLINE

#ifdef UCB_T_NO_INLINE
UCB_T_EXPORT void UCB_VECTOR_T_SWAP(_UCB_T_TYPE* vec, size_t idx1, size_t idx2);
#else
static inline void UCB_VECTOR_T_SWAP(_UCB_T_TYPE* vec, size_t idx1, size_t idx2)
{
    UCB_VERIFY_ARGS(vec && idx1 < vec->size && idx2 < vec->size && idx1 != idx2);
    _UCB_T tmp = vec->data[idx1];
    vec->data[idx1] = vec->data[idx2];
    vec->data[idx2] = tmp;
}
#endif

UCB_T_EXPORT size_t UCB_VECTOR_T_FOREACH(_UCB_T_TYPE* vec,
                                         _UCB_T_FUNC(iter_func) func,
                                         void* user_data);

/* ------------------------------ Find and sort ----------------------------- */

#ifdef UCB_T_CMP_FUNC
UCB_T_EXPORT void UCB_VECTOR_T_SORT(_UCB_T_TYPE* vec);
#endif

UCB_T_EXPORT void UCB_VECTOR_T_SORT_WITH(_UCB_T_TYPE* vec, ucb_cmp_func func);

#ifdef UCB_T_CMP_FUNC
UCB_T_EXPORT size_t UCB_VECTOR_T_INSERT_SORTED(_UCB_T_TYPE* vec, _UCB_T val);
#endif

UCB_T_EXPORT size_t UCB_VECTOR_T_INSERT_SORTED_WITH(_UCB_T_TYPE* vec,
                                                    _UCB_T val,
                                                    ucb_cmp_func func);

#ifdef UCB_T_CMP_FUNC
UCB_T_EXPORT ucb_ssize UCB_VECTOR_T_FIND(const _UCB_T_TYPE* vec, const _UCB_T_PTR val);
#endif

UCB_T_EXPORT ucb_ssize UCB_VECTOR_T_FIND_WITH(const _UCB_T_TYPE* vec,
                                              const _UCB_T_PTR val,
                                              ucb_cmp_func func);

#endif // UCB_T_DECLARE

/* -------------------------------------------------------------------------- */
/*                                 Definition                                 */
/* -------------------------------------------------------------------------- */

#ifdef UCB_T_DEFINE

/* -------------------------------- Lifecycle ------------------------------- */

_UCB_T_TYPE* UCB_VECTOR_T_NEW(void)
{
    return ucb_calloc_type(1, _UCB_T_TYPE);
}

void UCB_VECTOR_T_FREE(_UCB_T_TYPE* vec)
{
    if (vec)
    {
        UCB_VECTOR_T_CLEAR(vec);
        ucb_free(vec->data);
        ucb_free(vec);
    }
}

#if defined(UCB_T_FREE_FUNC)
void UCB_VECTOR_T_FREE_FULL(_UCB_T_TYPE* vec)
{
    if (vec)
    {
        UCB_VECTOR_T_CLEAR_FULL(vec);
        ucb_free(vec->data);
        ucb_free(vec);
    }
}
#endif

bool UCB_VECTOR_T_COPY(_UCB_T_TYPE* dst, const _UCB_T_TYPE* src)
{
    UCB_VERIFY_ARGS(dst && src && dst != src);

    UCB_VECTOR_T_CLEAR(dst);

    if (!UCB_VECTOR_T_RESERVE(dst, src->capacity))
    {
        UCB_VECTOR_T_CLEAR(dst);
        return false;
    }

    dst->size = src->size;

    memcpy(dst->data, src->data, src->size * sizeof(_UCB_T));

    return true;
}

#if defined(UCB_T_COPY_FUNC)
bool UCB_VECTOR_T_COPY_FULL(_UCB_T_TYPE* dst, const _UCB_T_TYPE* src)
{
    UCB_VERIFY_ARGS(dst && src && dst != src);

    UCB_VECTOR_T_CLEAR_FULL(dst);

    if (!UCB_VECTOR_T_RESERVE(dst, src->capacity))
    {
        // Only set to zero
        UCB_VECTOR_T_CLEAR(dst);
        return false;
    }

    for (size_t i = 0; i < src->size; i++)
    {
        dst->data[i] = (_UCB_T)ucb_calloc(1, sizeof(void*));
        if (!dst->data[i])
        {
            // Free the elements copied so far. The destination buffer and the
            // vector struct are owned by the caller and must be released by it.
            UCB_VECTOR_T_CLEAR_FULL(dst);
            return false;
        }
        UCB_T_COPY_FUNC(dst->data[i], src->data[i]);
        dst->size++;
    }

    return true;
}
#endif

_UCB_T_TYPE* UCB_VECTOR_T_CLONE(const _UCB_T_TYPE* src)
{
    UCB_VERIFY_ARGS(src);

    _UCB_T_TYPE* dst = ucb_calloc_type(1, _UCB_T_TYPE);
    if (dst)
    {
        if (!UCB_VECTOR_T_COPY(dst, src))
        {
            ucb_free(dst);
            return UCB_NULL;
        }
    }
    return dst;
}

#if defined(UCB_T_COPY_FUNC)
_UCB_T_TYPE* UCB_VECTOR_T_CLONE_FULL(const _UCB_T_TYPE* src)
{
    UCB_VERIFY_ARGS(src);

    _UCB_T_TYPE* dst = ucb_calloc_type(1, _UCB_T_TYPE);
    if (dst)
    {
        if (!UCB_VECTOR_T_COPY_FULL(dst, src))
        {
            // COPY_FULL freed the elements it copied; release the rest
            UCB_VECTOR_T_FREE_FULL(dst);
            return UCB_NULL;
        }
    }
    return dst;
}
#endif

void UCB_VECTOR_T_MOVE(_UCB_T_TYPE* dst, _UCB_T_TYPE* src)
{
    UCB_VERIFY_ARGS(dst && src && dst != src);

    UCB_VECTOR_T_CLEAR(dst);
    ucb_free(dst->data);

    dst->data = src->data;
    dst->size = src->size;
    dst->capacity = src->capacity;

    // Leave src in a valid but empty state
    src->data = UCB_NULL;
    src->size = 0;
    src->capacity = 0;
}

/* ---------------------------- Capacity and size --------------------------- */

bool UCB_VECTOR_T_RESERVE(_UCB_T_TYPE* vec, size_t new_capacity)
{
    UCB_VERIFY_ARGS(vec);
    if (new_capacity == 0)
        new_capacity = UCB_VECTOR_T_DEFAULT_CAPACITY;
    if (new_capacity <= vec->capacity)
        return true;

    _UCB_T* new_data = ucb_realloc_type(vec->data, new_capacity, _UCB_T);
    if (!new_data)
        return false;

    vec->data = new_data;
    vec->capacity = new_capacity;
    return true;
}

#ifdef UCB_T_NO_INLINE
size_t UCB_VECTOR_T_CAPACITY(const _UCB_T_TYPE* vec)
{
    UCB_VERIFY_ARGS(vec);
    return vec->capacity;
}

size_t UCB_VECTOR_T_SIZE(const _UCB_T_TYPE* vec)
{
    UCB_VERIFY_ARGS(vec);
    return vec->size;
}

bool UCB_VECTOR_T_IS_EMPTY(const _UCB_T_TYPE* vec)
{
    UCB_VERIFY_ARGS(vec);
    return vec->size == 0;
}

void UCB_VECTOR_T_CLEAR(_UCB_T_TYPE* vec)
{
    UCB_VERIFY_ARGS(vec);
    vec->size = 0;
}
#endif

#if defined(UCB_T_FREE_FUNC)
void UCB_VECTOR_T_CLEAR_FULL(_UCB_T_TYPE* vec)
{
    UCB_VERIFY_ARGS(vec);
    for (size_t i = 0; i < vec->size; i++)
    {
        UCB_T_FREE_FUNC(vec->data[i]);
    }
    vec->size = 0;
}
#endif

void UCB_VECTOR_T_FIT(_UCB_T_TYPE* vec)
{
    UCB_VERIFY_ARGS(vec);
    if (vec->capacity > vec->size)
    {
        _UCB_T* new_data = ucb_realloc_type(vec->data, vec->size, _UCB_T);
        if (new_data)
        {
            vec->data = new_data;
            vec->capacity = vec->size;
        }
    }
}

/* --------------------- Element access and modification -------------------- */

void UCB_VECTOR_T_INSERT(_UCB_T_TYPE* vec, size_t index, _UCB_T data)
{
    UCB_VERIFY_ARGS(vec && index <= vec->size);
    if (vec->size == vec->capacity)
    {
        if (!UCB_VECTOR_T_RESERVE(vec, vec->capacity * 2))
            return;
    }
    if (index < vec->size)
    {
        memmove(vec->data + (index + 1), vec->data + index, (vec->size - index) * sizeof(_UCB_T));
    }
    vec->data[index] = data;
    vec->size++;
}

_UCB_T UCB_VECTOR_T_REMOVE(_UCB_T_TYPE* vec, size_t index)
{
    UCB_VERIFY_ARGS(vec && index < vec->size);
    UCB_VERIFY_MSG(vec->size > 0, UCB_ERROR_INVALID_ARG, "Cannot remove from empty container");
    _UCB_T data = vec->data[index];
    if (index < vec->size - 1)
    {
        memmove(vec->data + index, vec->data + index + 1, (vec->size - index - 1) * sizeof(_UCB_T));
    }
    vec->size--;
    return data;
}

void UCB_VECTOR_T_PUSH_BACK(_UCB_T_TYPE* vec, _UCB_T data)
{
    UCB_VERIFY_ARGS(vec);
    if (vec->size < vec->capacity)
    {
        vec->data[vec->size] = data;
        vec->size++;
        return;
    }
    UCB_VECTOR_T_INSERT(vec, vec->size, data);
}

void UCB_VECTOR_T_PUSH_FRONT(_UCB_T_TYPE* vec, _UCB_T data)
{
    UCB_VERIFY_ARGS(vec);
    if (vec->size == 0 && vec->capacity > 0)
    {
        vec->data[0] = data;
        vec->size++;
        return;
    }
    UCB_VECTOR_T_INSERT(vec, 0, data);
}

_UCB_T UCB_VECTOR_T_POP_BACK(_UCB_T_TYPE* vec)
{
    UCB_VERIFY_ARGS(vec && vec->size);
    UCB_VERIFY_MSG(vec->size > 0, UCB_ERROR_INVALID_ARG, "Cannot remove from empty container");
    vec->size--;
    return vec->data[vec->size];
}

_UCB_T UCB_VECTOR_T_POP_FRONT(_UCB_T_TYPE* vec)
{
    UCB_VERIFY_ARGS(vec && vec->size);
    UCB_VERIFY_MSG(vec->size > 0, UCB_ERROR_INVALID_ARG, "Cannot remove from empty container");
    vec->size--;
    _UCB_T ret = vec->data[0];
    if (vec->size > 0)
    {
        memmove(vec->data, vec->data + 1, vec->size * sizeof(_UCB_T));
    }
    return ret;
}

#ifdef UCB_T_NO_INLINE
_UCB_T UCB_VECTOR_T_PEEK_BACK(const _UCB_T_TYPE* vec)
{
    UCB_VERIFY_ARGS(vec && vec->size);
    return vec->data[vec->size - 1];
}

_UCB_T UCB_VECTOR_T_PEEK_FRONT(const _UCB_T_TYPE* vec)
{
    UCB_VERIFY_ARGS(vec && vec->size);
    return vec->data[0];
}

_UCB_T UCB_VECTOR_T_GET(const _UCB_T_TYPE* vec, size_t index)
{
    UCB_VERIFY_ARGS(vec && index < vec->size);
    return vec->data[index];
}

void UCB_VECTOR_T_SET(_UCB_T_TYPE* vec, size_t index, _UCB_T data)
{
    UCB_VERIFY_ARGS(vec && index < vec->size);
    vec->data[index] = data;
}

void UCB_VECTOR_T_SWAP(_UCB_T_TYPE* vec, size_t idx1, size_t idx2)
{
    UCB_VERIFY_ARGS(vec && idx1 < vec->size && idx2 < vec->size && idx1 != idx2);
    _UCB_T tmp = vec->data[idx1];
    vec->data[idx1] = vec->data[idx2];
    vec->data[idx2] = tmp;
}
#endif

size_t UCB_VECTOR_T_FOREACH(_UCB_T_TYPE* vec, _UCB_T_FUNC(iter_func) func, void* user_data)
{
    UCB_VERIFY_ARGS(vec && func);
    size_t i = 0;
    for (; i < vec->size; ++i)
    {
#ifdef UCB_T_IS_POD
        if (!func(&vec->data[i], i, user_data))
#else
        if (!func(vec->data[i], i, user_data))
#endif
        {
            break;
        }
    }
    return i;
}

/* ------------------------------ Find and sort ----------------------------- */

#ifdef UCB_T_CMP_FUNC
void UCB_VECTOR_T_SORT(_UCB_T_TYPE* vec)
{
    UCB_VECTOR_T_SORT_WITH(vec, UCB_T_CMP_FUNC);
}
#endif

#ifndef UCB_T_IS_POD
int _UCB_T_FUNC(cmp_wrapper)(const void* a, const void* b, void* ctx)
{
    ucb_cmp_func func = (ucb_cmp_func)ctx;
    const void* a_ptr = *(void**)a;
    const void* b_ptr = *(void**)b;
    return func(a_ptr, b_ptr);
}
#endif

void UCB_VECTOR_T_SORT_WITH(_UCB_T_TYPE* vec, ucb_cmp_func func)
{
    UCB_VERIFY_ARGS(vec && func);
#ifdef UCB_T_IS_POD
    qsort(vec->data, vec->size, sizeof(_UCB_T), func);
#else
    /*
     * qsort will call comparison with a required cast
     * const _UCB_T str_a = *(const _UCB_T*)a;
     * But given that _UCB_T itself is a pointer, this is an extra unexpected layer for
     * the caller of this function. Therefore, we will handle the extra pointer level
     * using an intermediate wrapper.
     */
    ucb_qsort_ctx(vec->data,
                  vec->size,
                  sizeof(_UCB_T),
                  _UCB_T_FUNC(cmp_wrapper),
                  (void*)(uintptr_t)func);
#endif
}

#ifdef UCB_T_CMP_FUNC
size_t UCB_VECTOR_T_INSERT_SORTED(_UCB_T_TYPE* vec, _UCB_T val)
{
    return UCB_VECTOR_T_INSERT_SORTED_WITH(vec, val, UCB_T_CMP_FUNC);
}
#endif

size_t UCB_VECTOR_T_INSERT_SORTED_WITH(_UCB_T_TYPE* vec, _UCB_T val, ucb_cmp_func func)
{
    UCB_VERIFY_ARGS(vec && func);
#ifdef UCB_T_IS_POD
    ucb_ssize pos = UCB_VECTOR_T_FIND_WITH(vec, &val, func);
#else
    ucb_ssize pos = UCB_VECTOR_T_FIND_WITH(vec, val, func);
#endif
    size_t ins_pos;
    if (pos >= 0)
    {
        // Insert to right of match (rightmost of all matches)
        ins_pos = pos + 1;
    }
    else
    {
        // Insert a new element
        ins_pos = -pos - 1;
    }
    UCB_VECTOR_T_INSERT(vec, ins_pos, val);
    return ins_pos;
}

#ifdef UCB_T_CMP_FUNC
ucb_ssize UCB_VECTOR_T_FIND(const _UCB_T_TYPE* vec, const _UCB_T_PTR val)
{
    return UCB_VECTOR_T_FIND_WITH(vec, val, UCB_T_CMP_FUNC);
}
#endif

ucb_ssize UCB_VECTOR_T_FIND_WITH(const _UCB_T_TYPE* vec, const _UCB_T_PTR val, ucb_cmp_func func)
{
    UCB_VERIFY_ARGS(vec && func);
    // return rightmost index if found, otherwise -(index + 1)
    ucb_ssize low = 0;
    ucb_ssize high = UCB_VECTOR_T_SIZE(vec) - 1;
    while (low <= high)
    {
        ucb_ssize mid = low + (high - low) / 2;
#ifdef UCB_T_IS_POD
        int cmp = func(val, &vec->data[mid]);
#else
        int cmp = func(val, vec->data[mid]);
#endif
        if (cmp == 0)
        {
#ifdef UCB_T_IS_POD
            while (mid < high && func(val, &vec->data[mid + 1]) == 0)
#else
            while (mid < high && func(val, vec->data[mid + 1]) == 0)
#endif
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

#endif // UCB_T_DEFINE

// Cleanup functions
#undef UCB_VECTOR_T_NEW
#undef UCB_VECTOR_T_FREE
#undef UCB_VECTOR_T_FREE_FULL
#undef UCB_VECTOR_T_COPY
#undef UCB_VECTOR_T_COPY_FULL
#undef UCB_VECTOR_T_CLONE
#undef UCB_VECTOR_T_CLONE_FULL
#undef UCB_VECTOR_T_MOVE
#undef UCB_VECTOR_T_RESERVE
#undef UCB_VECTOR_T_CAPACITY
#undef UCB_VECTOR_T_SIZE
#undef UCB_VECTOR_T_IS_EMPTY
#undef UCB_VECTOR_T_FIT
#undef UCB_VECTOR_T_CLEAR
#undef UCB_VECTOR_T_CLEAR_FULL
#undef UCB_VECTOR_T_INSERT
#undef UCB_VECTOR_T_REMOVE
#undef UCB_VECTOR_T_PUSH_BACK
#undef UCB_VECTOR_T_PUSH_FRONT
#undef UCB_VECTOR_T_POP_BACK
#undef UCB_VECTOR_T_POP_FRONT
#undef UCB_VECTOR_T_PEEK_BACK
#undef UCB_VECTOR_T_PEEK_FRONT
#undef UCB_VECTOR_T_GET
#undef UCB_VECTOR_T_SET
#undef UCB_VECTOR_T_SWAP
#undef UCB_VECTOR_T_BEGIN
#undef UCB_VECTOR_T_END
#undef UCB_VECTOR_T_CBEGIN
#undef UCB_VECTOR_T_CEND
#undef UCB_VECTOR_T_RBEGIN
#undef UCB_VECTOR_T_REND
#undef UCB_VECTOR_T_CRBEGIN
#undef UCB_VECTOR_T_CREND

// Cleanup internal macros
#undef _UCB_T_FUNC
#undef _UCB_T_TYPE
#undef _UCB_T_CONST
#undef _UCB_T_PTR
#undef _UCB_T_IS_POD
#undef _UCB_T

// Cleanup calling macros to prevent accidental reuse in other contexts
#undef UCB_T_SUFFIX
#undef UCB_T_DECLARE
#undef UCB_T_DEFINE
#undef UCB_T_NO_INLINE
#undef UCB_T_EXPORT
#undef UCB_T_IS_POD
#undef UCB_T_FREE_FUNC
#undef UCB_T_COPY_FUNC
#undef UCB_T_CMP_FUNC
