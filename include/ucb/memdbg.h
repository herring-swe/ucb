/**
 * This file is part of the UCB project
 * SPDX-FileCopyrightText: © 2025 Åke Svedin <ake@svedin.org>
 * SPDX-License-Identifier: MIT
 *
 * @brief Memory debugging functions
 */

#include "diag.h"
#include "export.h"

#include <stdbool.h>
#include <stddef.h>

UCB_DIAG_PUSH
UCB_DIAG_IGN_PADDED
typedef struct ucb_mem_report_alloc
{
    struct ucb_mem_report_alloc* next;
    void* ptr;        // Memory pointer
    size_t size;      // Size of memory
    const char* file; // File where allocated
    int line;         // Line where allocated
} ucb_mem_alloc_t;

typedef struct
{
    const char* name;        // Tracepoint level name
    int level;               // Tracepoint level, 0 indicate all memory
    bool leaks;              // If true, allocs contains leaks
    ucb_mem_alloc_t* allocs; // Pointer to remaining allocation (leaks), May be UCB_NULL.
    size_t current_alloc;    // Current number of allocations (leaks)
    size_t current_size;     // Current allocated memory (leaks)
    size_t peak_alloc;       // Max number of allocations at any point
    size_t peak_size;        // Max allocated memory at any point
    size_t peak_alloc_block; // Biggest memory block allocated
    size_t total_alloc;      // Total number of allocations
    size_t total_size;       // Total allocated memory
} ucb_mem_report_t;
UCB_DIAG_POP

/**
 * @brief Custom report function
 * @param report Pointer to report, which will be free'd after this call.
 */
typedef void (*ucb_mem_report_func_t)(const ucb_mem_report_t* const report);

/**
 * @brief Enable memory tracking
 * Once enabled, cannot be disabled. Since all memory allocated will be
 * wrapped with metadata.
 */
UCB_API void ucb_mem_tracking_enable(void);
UCB_API void ucb_mem_tracking_reset(void);
UCB_API ucb_mem_report_func_t ucb_mem_tracking_set_report_func(ucb_mem_report_func_t report_func);
UCB_API bool ucb_mem_tracking_is_enabled(void);
UCB_API int ucb_mem_tracking_level(void);
UCB_API int ucb_mem_tracking_push(void);
UCB_API int ucb_mem_tracking_push_name(const char* name);
UCB_API int ucb_mem_tracking_pop(void);
UCB_API int ucb_mem_tracking_report(void);

#ifdef NDEBUG
#define UCB_MEMTRACK_ENABLE()        ((void)0)
#define UCB_MEMTRACK_RESET()         ((void)0)
#define UCB_MEMTRACK_SET_FUNC(func)  ((ucb_mem_report_func_t)0)
#define UCB_MEMTRACK_IS_ENABLED()    false
#define UCB_MEMTRACK_LEVEL()         (-1)
#define UCB_MEMTRACK_PUSH()          (-1)
#define UCB_MEMTRACK_PUSH_NAME(name) (-1)
#define UCB_MEMTRACK_POP()           (-1)
#define UCB_MEMTRACK_REPORT()        (-1)
#else // Debug build
#define UCB_MEMTRACK_ENABLE()        ucb_mem_tracking_enable()
#define UCB_MEMTRACK_RESET()         ucb_mem_tracking_reset()
#define UCB_MEMTRACK_SET_FUNC(func)  ucb_mem_tracking_set_report_func(func)
#define UCB_MEMTRACK_IS_ENABLED()    ucb_mem_tracking_is_enabled()
#define UCB_MEMTRACK_LEVEL()         ucb_mem_tracking_level()
#define UCB_MEMTRACK_PUSH()          ucb_mem_tracking_push()
#define UCB_MEMTRACK_PUSH_NAME(name) ucb_mem_tracking_push_name(name)
#define UCB_MEMTRACK_POP()           ucb_mem_tracking_pop()
#define UCB_MEMTRACK_REPORT()        ucb_mem_tracking_report()
#endif // NDEBUG
