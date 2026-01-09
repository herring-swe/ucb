/**
 * @file memory.h
 * 
 * This file is part of the UCB project
 * - SPDX-FileCopyrightText: © 2026 Åke Svedin <ake@svedin.org>
 * - SPDX-License-Identifier: MIT
 * 
 * @brief Memory allocation routines
 *
 * Many of these are designed to return the allocation memory directly.
 * In case of out-of-memory, the application will abort instead, @see ucb/error.h
 */

#ifndef UCB_MEMORY_H
#define UCB_MEMORY_H

#include "defines.h"
#include "export.h"
#include "types.h"

#include <stdbool.h>
#include <stddef.h>

/* -------------------------------------------------------------------------- */
/*                            Core memory functions                           */
/* -------------------------------------------------------------------------- */
/*
 * All of these have debug functions to track memory allocations
 */

/**
 * @brief Safe alloc. Allocate uninitialized memory block.
 * As stdlib alloc but:
 * - If size is 0, will set UCB_ERROR_INVALID_ARG and return UCB_NULL
 * - If memory failed to allocate, will report fatal error UCB_ERROR_OUT_OF_MEMORY.
 * @param size size of new memory
 * @return pointer to new memory or UCB_NULL
 */
UCB_API void* ucb_malloc(size_t size);

/**
 * @brief Safe calloc. Allocate zeroied memory block.
 * As stdlib calloc but:
 * - If num or size is 0, will set UCB_ERROR_INVALID_ARG and return UCB_NULL.
 * - If memory failed to allocate, will report fatal error UCB_ERROR_OUT_OF_MEMORY.
 * @param num number of elements to allocate
 * @param size size of each element
 * @return pointer to new memory or UCB_NULL
 */
UCB_API void* ucb_calloc(size_t num, size_t size);

/**
 * @brief Safe realloc. Reallocate memory block.
 * As stdlib realloc but:
 * - If ptr is NULL, will behave like ucb_malloc(size).
 * - If size is 0, will behave like ucb_free(ptr) but set UCB_ERROR_INVALID_ARG.
 * - If memory failed to allocate, will free ptr and call report fatal error
 *   UCB_ERROR_OUT_OF_MEMORY.
 * @param ptr memory to reallocate
 * @param size size of new memory
 * @return pointer to new memory or UCB_NULL
 */
UCB_API void* ucb_realloc(void* ptr, size_t size);
// Exactly like ucb_realloc but with controllable behavior on freeing ptr on failure
UCB_API void* ucb_realloc2(void* ptr, size_t size, bool free_on_failure);

/**
 * @brief Same as free, ignores NULL.
 * @param ptr memory to free
 */
UCB_API void ucb_free(void* ptr);

/* -------------------------------------------------------------------------- */
/*                               Memory tracking                              */
/* -------------------------------------------------------------------------- */

#ifndef NDEBUG
UCB_API void* ucb_malloc_debug(size_t size, const char* file, int line);
UCB_API void* ucb_calloc_debug(size_t num, size_t size, const char* file, int line);
UCB_API void* ucb_realloc_debug(void* ptr, size_t size, const char* file, int line);
UCB_API void* ucb_realloc2_debug(void* ptr, size_t size, bool free_on_failure, const char* file,
                                 int line);
UCB_API void ucb_free_debug(void* ptr, const char* file, int line);

#ifndef UCB_MEMORY_IMPL
/**
 * Override memory functions to include file and line information
 * Only for debug builds, see memdbh.h
 */
#define ucb_malloc(num)              ucb_malloc_debug((num), __FILE__, __LINE__)
#define ucb_calloc(num, size)        ucb_calloc_debug((num), (size), __FILE__, __LINE__)
#define ucb_realloc(ptr, num)        ucb_realloc_debug((ptr), (num), __FILE__, __LINE__)
#define ucb_realloc2(ptr, num, free) ucb_realloc2_debug((ptr), (num), (free), __FILE__, __LINE__)
#define ucb_free(ptr)                ucb_free_debug((ptr), __FILE__, __LINE__)

#endif // UCB_MEMORY_IMPL
#endif // NDEBUG

/* -------------------------------------------------------------------------- */
/*                           Other memory functions                           */
/* -------------------------------------------------------------------------- */

UCB_API void ucb_memcpy_s(void* UCB_RESTRICT dest, size_t dest_size, const void* UCB_RESTRICT src,
                          size_t src_size);

/* -------------------------------------------------------------------------- */
/*                                Helper macros                               */
/* -------------------------------------------------------------------------- */

/*
 * Allocations with explicit type. Same signature as calloc.
 * Can be used on same line as declaration.
 *
 * Usage example:
 * struct MyType *my_type = ucb_calloc_type(1, struct MyType);
 */
#ifdef __cplusplus

// clang-format off
#define ucb_malloc_type(num, tname) \
    (reinterpret_cast<tname*>(ucb_malloc((num) * sizeof(tname))))
#define ucb_calloc_type(num, tname) \
    (reinterpret_cast<tname*>(ucb_calloc((num), sizeof(tname))))
#define ucb_realloc_type(ptr, num, tname) \
    (reinterpret_cast<tname*>(ucb_realloc((ptr), (num) * sizeof(tname))))

#define ucb_free_null(ptr) \
    do { ucb_free(static_cast<void*>(ptr)); (ptr) = nullptr; } while (0)
#else

#define ucb_malloc_type(num, tname) \
    ((tname*)ucb_malloc((num) * sizeof(tname)))
#define ucb_calloc_type(num, tname) \
    ((tname*)ucb_calloc((num), sizeof(tname)))
#define ucb_realloc_type(ptr, num, tname) \
    ((tname*)ucb_realloc((ptr), (num) * sizeof(tname)))

#define ucb_free_null(ptr)      \
    do { ucb_free((void*)(ptr)); (ptr) = UCB_NULL; } while (0)
#endif
// clang-format on

#ifndef UCB_BUILD_LIB

/*
 * Allocation with explicit type, using pointer type.
 * Must be used AFTER ptr have been declared.
 *
 * Note: Requires C23 or later or Visual Studio 19.39 or later.
 * Due to this, it's not used internally.
 *
 * Usage example ():
 * struct MyType *my_type;
 * my_type = ucb_calloc_ptr(1, my_type);
 */
#if defined(__STDC_VERSION__) && __STDC_VERSION__ >= 202311L
#define ucb_malloc_ptr(ptr, num)  ((typeof(ptr))ucb_malloc((num) * sizeof(*(ptr))))
#define ucb_calloc_ptr(ptr, num)  ((typeof(ptr))ucb_calloc((num), sizeof(*(ptr))))
#define ucb_realloc_ptr(ptr, num) ((typeof(ptr))ucb_realloc((ptr), (num) * sizeof(*(ptr))))
#elif defined(__gcc__) || defined(__clang__) || (defined(_MSC_VER) && _MSC_VER >= 1939)
#define ucb_malloc_ptr(ptr, num)  ((__typeof__(ptr))ucb_malloc((num) * sizeof(*(ptr))))
#define ucb_calloc_ptr(ptr, num)  ((__typeof__(ptr))ucb_calloc((num), sizeof(*(ptr))))
#define ucb_realloc_ptr(ptr, num) ((__typeof__(ptr))ucb_realloc((ptr), (num) * sizeof(*(ptr))))
#endif

#endif // UCB_BUILD_LIB

#endif // UCB_MEMORY_H
