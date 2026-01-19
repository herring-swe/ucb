/**
 * @file memdbg.h
 *
 * This file is part of the UCB project
 * - SPDX-FileCopyrightText: © 2026 Åke Svedin <ake@svedin.org>
 * - SPDX-License-Identifier: MIT
 *
 * @brief Memory debugging functions
 */

#ifndef UCB_MEMDBG_H
#define UCB_MEMDBG_H

#include "btrace.h"
#include "defines.h"
#include "diag.h"
#include "export.h"

#include <stdbool.h>
#include <stddef.h>

/**
 * @addtogroup MemDebug
 * @{
 */

UCB_DIAG_PUSH()
UCB_DIAG_IGN_PADDED()

/**
 * @struct ucb_mem_report_alloc
 * @brief Memory allocation report
 */
typedef struct ucb_mem_report_alloc
{
    struct ucb_mem_report_alloc* next;
    void* ptr;        ///< Memory pointer of user allocated memory
    size_t size;      ///< Size of user allocated memory
    const char* file; ///< File where allocated
    int line;         ///< Line where allocated
    ucb_btrace* bt;   ///< Backtrace. UCB_NULL if not enabled in configuration
} ucb_mem_alloc;

/**
 * @struct ucb_mem_report
 * @brief Memory report
 *
 * Passed as argument to the user ucb_mem_report callback function.
 * Must not be manipulated by the user.
 */
typedef struct ucb_mem_report
{
    const char* name;        ///< Tracepoint level name
    int level;               ///< Tracepoint level, 0 indicate all memory
    bool leaks;              ///< If true, allocated data are considered leaks
    ucb_mem_alloc* allocs;   ///< Pointer to remaining allocation (leaks), May be UCB_NULL.
    size_t current_alloc;    ///< Current number of allocations (leaks)
    size_t current_size;     ///< Current allocated memory (leaks)
    size_t peak_alloc;       ///< Max number of allocations at any point
    size_t peak_size;        ///< Max allocated memory at any point
    size_t peak_alloc_block; ///< Biggest memory block allocated
    size_t total_alloc;      ///< Total number of allocations
    size_t total_size;       ///< Total allocated memory
} ucb_mem_report;
UCB_DIAG_POP()

/**
 * @brief Custom report function
 * @param report Pointer to report data
 */
typedef void (*ucb_mem_report_func)(const ucb_mem_report* const report);

/// @cond INTERNAL
UCB_API void ucb_mem_tracking_enable(void);
UCB_API void ucb_mem_tracking_reset(void);
UCB_API ucb_mem_report_func ucb_mem_tracking_set_report_func(ucb_mem_report_func report_func);
UCB_API bool ucb_mem_tracking_is_enabled(void);
UCB_API int ucb_mem_tracking_level(void);
UCB_API void ucb_mem_tracking_push(void);
UCB_API void ucb_mem_tracking_push_name(const char* name);
UCB_API void ucb_mem_tracking_pop(void);
UCB_API void ucb_mem_tracking_report(bool final);
/// @endcond

#ifdef NDEBUG
UCB_DIAG_PUSH()
UCB_DIAG_IGN_UNUSED_VALUE()
#define UCB_MEMTRACK_ENABLE()        ((void)0)
#define UCB_MEMTRACK_RESET()         ((void)0)
#define UCB_MEMTRACK_SET_FUNC(func)  ((ucb_mem_report_func)0)
#define UCB_MEMTRACK_IS_ENABLED()    false
#define UCB_MEMTRACK_LEVEL()         (-1)
#define UCB_MEMTRACK_PUSH()          ((void)0)
#define UCB_MEMTRACK_PUSH_NAME(name) ((void)0)
#define UCB_MEMTRACK_POP()           ((void)0)
#define UCB_MEMTRACK_REPORT()        ((void)0)
#define UCB_MEMTRACK_FINAL()         ((void)0)
UCB_DIAG_POP()
#else // Debug build

/**
 * @brief Enable memory tracking
 *
 * Will start tracking all allocations made with ucb_malloc, ucb_calloc, ucb_realloc and
 * untracked allocations with ucb_free.
 *
 * Once enabled, cannot be disabled. Since all memory allocations will be wrapped with
 * metadata and reallocations would invalidate existing pointers.
 *
 * Allocations outside of ucb cannot be tracked.
 *
 * @see UCB_MEMTRACK_FINAL for the corresponding call before termination.
 * @note This is only available in debug builds.
 * @note This must be done before any memory allocations are made using ucb functions.
 */
#define UCB_MEMTRACK_ENABLE()        ucb_mem_tracking_enable()

/**
 * @brief Reset memory tracking
 *
 * This will zero allocation stats.
 */
#define UCB_MEMTRACK_RESET()         ucb_mem_tracking_reset()

/**
 * @brief Set the memory report function
 * @param func The function to call when reporting memory usage
 * @return The previous function or NULL
 */
#define UCB_MEMTRACK_SET_FUNC(func)  ucb_mem_tracking_set_report_func(func)

/**
 * @brief Check if memory tracking is enabled
 * @return true if enabled, false otherwise
 */
#define UCB_MEMTRACK_IS_ENABLED()    ucb_mem_tracking_is_enabled()

/**
 * @brief Get the current memory tracking level
 * @return The current level or -1 if not enabled
 */
#define UCB_MEMTRACK_LEVEL()         ucb_mem_tracking_level()

/**
 * @brief Push a new memory tracking level
 */
#define UCB_MEMTRACK_PUSH()          ucb_mem_tracking_push()

/**
 * @brief Push a new memory tracking level with a name
 */
#define UCB_MEMTRACK_PUSH_NAME(name) ucb_mem_tracking_push_name(name)

/**
 * @brief Pop the current memory tracking level
 *
 * If there are still allocations remaining at this level, a leak report
 * is generated, and the memory tracked is moved to the previous level.
 */
#define UCB_MEMTRACK_POP()           ucb_mem_tracking_pop()

/**
 * @brief Manually trigger a report
 *
 * Will report the state in the current memory tracking level.
 * Data is not considered leaks in this case.
 */
#define UCB_MEMTRACK_REPORT()        ucb_mem_tracking_report(false)

/**
 * @brief Trigger a final report
 *
 * To be called just before program termination.
 * It will generate a report of all memory still allocated as leaks.
 * @note No actual cleanup is done and levels remain intact.
 */
#define UCB_MEMTRACK_FINAL()         ucb_mem_tracking_report(true)
#endif // NDEBUG

/**
 * @}
 */

#endif // UCB_MEMDBG_H
