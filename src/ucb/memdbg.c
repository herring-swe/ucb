/**
 * This file is part of the UCB project
 * SPDX-FileCopyrightText: © 2025 Åke Svedin <ake@svedin.org>
 * SPDX-License-Identifier: MIT
 *
 * @brief Memory debug functions
 */

#ifndef NDEBUG

#define UCB_MEMORY_IMPL

#include "ucb/memdbg.h"

#include "ucb/errcodes.h"
#include "ucb/error.h"
#include "ucb/math.h"
#include "ucb/memory.h"
#include "ucb/mutex.h"
#include "ucb/mutex_private.h"

#include <assert.h>
#include <stdarg.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define VERIFY_LEVEL 0

#define ALLOC_MAGIC   0xBEA01234
#define DEALLOC_MAGIC 0x0DEADBEA
#define META_SIZE     sizeof(ucb_alloc_meta_t)

// TODO: Should be configurable via CMake
#define MAX_TRACEPOINTS     10
#define MAX_TRACEPOINT_NAME 64

typedef struct ucb_alloc_meta
{
    struct ucb_alloc_meta* prev;
    struct ucb_alloc_meta* next;
    size_t size;
    const char* file;
    int line;
    int level; // tracepoint
    uint32_t magic;
} ucb_alloc_meta_t;

typedef struct ucb_tracepoint
{
    char name[MAX_TRACEPOINT_NAME];
    ucb_alloc_meta_t* alloc;
    size_t current_alloc;    // Current number of allocations
    size_t current_size;     // Current allocated memory
    size_t peak_alloc;       // Max number of allocations at any point
    size_t peak_size;        // Max allocated memory at any point
    size_t peak_alloc_block; // Biggest memory block allocated
    size_t total_alloc;      // Total number of allocations
    size_t total_size;       // Total allocated memory
} ucb_tracepoint_t;

// Global list of allocations
static ucb_tracepoint_t* s_trace_points[MAX_TRACEPOINTS];
static int s_trace_level                   = -1;
static ucb_mem_report_func_t s_report_func = UCB_NULL;

static ucb_mutex_t s_mutex = {0};

// Global setting
static bool s_tracking_enabled = false;

/* -------------------------------------------------------------------------- */
/*                              Standalone Utils                              */
/* -------------------------------------------------------------------------- */
/*
 * Cannot depend on ucb/memory.h
 */

static inline int mem_sprintf(char* str, size_t size, const char* fmt, ...)
{
    va_list args;
    va_start(args, fmt);
    UCB_DIAG_PUSH
    UCB_DIAG_IGN_FORMAT_NONLITERAL
    int ret = vsnprintf(str, size, fmt, args);
    UCB_DIAG_POP
    va_end(args);
    return ret;
}

/* -------------------------------------------------------------------------- */
/*                               Implementation                               */
/* -------------------------------------------------------------------------- */

static void ucb_mem_tracking_default_report_func(const ucb_mem_report_t* const report)
{
    if (!report)
        return;

    printf("*** Memory report *************************************************************\n");
    printf("Tracepoint: %s (Level %d)\n", report->name, report->level);
    printf("  Current: %zu allocs, %zu bytes\n", report->current_alloc, report->current_size);
    printf("  Peaks: %zu allocs, %zu bytes, largest block %zu\n", report->peak_alloc,
           report->peak_size, report->peak_alloc_block);
    printf("  Totals: %zu allocs, %zu bytes\n", report->total_alloc, report->total_size);

    // List leaks
    if (report->allocs)
    {
        printf("\n");
        if (report->leaks)
            printf("!! LEAKS DETECTED !!\n\n");
        printf("Current allocations:\n\n");
    }
    for (ucb_mem_alloc_t* alloc = report->allocs; alloc; alloc = alloc->next)
    {
        printf("    Address: %p of size %zu bytes\n", alloc->ptr, alloc->size);
        printf("    Allocated at: %s:%d\n\n", alloc->file, alloc->line);
    }
    printf("*** End of report *************************************************************\n");
}

#if VERIFY_LEVEL
static void verify_level(int level)
{
    if (level < 0 || level > s_trace_level)
        return;

    ucb_tracepoint_t* tp = s_trace_points[level];
    assert(tp);

    size_t num  = 0;
    size_t size = 0;

    ucb_alloc_meta_t* prev = UCB_NULL;
    for (ucb_alloc_meta_t* cur = tp->alloc; cur; cur = cur->next)
    {
        assert(cur->magic == ALLOC_MAGIC);
        assert(cur->level == level);
        assert(cur->size > 0);
        assert(cur->file);
        assert(cur->prev == prev);
        prev = cur;
        num++;
        size += cur->size;
    }
    assert(tp->current_alloc == num);
    assert(tp->current_size == size);
}
#endif // VERIFY_LEVEL

/**
 * Generate a report of allocations from a specific level and up
 * Will always announce a report, even if NULL as long as report function is set.
 */
static int gen_tracepoint_report(int from_level, bool leaks)
{
    ucb_mutex_lock(&s_mutex);
    if (!s_report_func || s_trace_level < 0)
    {
        ucb_mutex_unlock(&s_mutex);
        return -1;
    }

    ucb_mem_report_t* report = UCB_NULL;
    ucb_mem_alloc_t* last    = UCB_NULL;
    ucb_mem_alloc_t* cur     = UCB_NULL;

    if (from_level < 0)
        from_level = 0;
    if (from_level > s_trace_level)
        from_level = s_trace_level;

    for (int level = from_level; level <= s_trace_level; level++)
    {
        ucb_tracepoint_t* tp = s_trace_points[level];
        assert(tp);

        if (!report)
        {
            report = (ucb_mem_report_t*)calloc(1, sizeof(ucb_mem_report_t));
            if (!report)
            {
                ucb_mutex_unlock(&s_mutex);
                ucb_fatal(ucb_error_literal(UCB_ERROR_OUT_OF_MEMORY,
                                            "Failed to allocate memory for memory report"));
                from_level = -1;
                goto cleanup;
            }
            report->level = level;
            report->name  = tp->name;
            report->leaks = leaks;
        }

        report->peak_alloc       = ucb_max(report->peak_alloc, tp->peak_alloc);
        report->peak_size        = ucb_max(report->peak_size, tp->peak_size);
        report->peak_alloc_block = ucb_max(report->peak_alloc_block, tp->peak_alloc_block);
        report->total_alloc += tp->total_alloc;
        report->total_size += tp->total_size;

        for (ucb_alloc_meta_t* alloc = tp->alloc; alloc; alloc = alloc->next)
        {
            cur = (ucb_mem_alloc_t*)calloc(1, sizeof(ucb_mem_alloc_t));
            if (!cur)
            {
                ucb_mutex_unlock(&s_mutex);
                ucb_fatal(ucb_error_literal(UCB_ERROR_OUT_OF_MEMORY,
                                            "Failed to allocate memory for memory report"));
                from_level = -1;
                goto cleanup;
            }

            cur->ptr  = alloc + 1;
            cur->size = alloc->size;
            cur->file = alloc->file;
            cur->line = alloc->line;

            report->current_alloc++;
            report->current_size += cur->size;

            if (last)
                last->next = cur;
            else
                report->allocs = cur;
            last = cur;
        }
    }

    ucb_mutex_unlock(&s_mutex);
    s_report_func(report);

    // Free report
cleanup:
    if (report)
    {
        cur = report->allocs;
        while (cur)
        {
            last = cur->next;
            free(cur);
            cur = last;
        }
        free(report);
    }
    return from_level;
}

void ucb_mem_tracking_enable(void)
{
    if (s_tracking_enabled)
        return;

    ucb_mutex_init(&s_mutex, UCB_MUTEX_DEFAULT);
    ucb_mutex_lock(&s_mutex);
    s_tracking_enabled = true;

    s_report_func     = ucb_mem_tracking_default_report_func;
    s_trace_level     = 0;
    s_trace_points[0] = (ucb_tracepoint_t*)calloc(1, sizeof(ucb_tracepoint_t));
    mem_sprintf(s_trace_points[0]->name, MAX_TRACEPOINT_NAME, "%s", "all memory");

    for (int i = 0; i < MAX_TRACEPOINTS; i++)
    {
        if (i <= s_trace_level)
            assert(s_trace_points[i]);
        else
            assert(!s_trace_points[i]);
    }
    ucb_mutex_unlock(&s_mutex);
}

void ucb_mem_tracking_reset(void)
{
    ucb_mutex_lock(&s_mutex);
    if (s_trace_level >= 0)
    {
        for (int level = 0; level <= s_trace_level; level++)
        {
            ucb_tracepoint_t* tp = s_trace_points[level];
            assert(tp);

            tp->peak_alloc       = 0;
            tp->peak_size        = 0;
            tp->peak_alloc_block = 0;
            tp->total_alloc      = 0;
            tp->total_size       = 0;
        }
    }
    ucb_mutex_unlock(&s_mutex);
}

ucb_mem_report_func_t ucb_mem_tracking_set_report_func(ucb_mem_report_func_t func)
{
    ucb_mem_report_func_t old = s_report_func;
    s_report_func             = func;
    return old;
}

bool ucb_mem_tracking_is_enabled(void)
{
    return s_tracking_enabled;
}

int ucb_mem_tracking_level(void)
{
    ucb_mutex_lock(&s_mutex);
    int level = s_trace_level;
    ucb_mutex_unlock(&s_mutex);
    return level;
}

void ucb_mem_tracking_push(void)
{
    ucb_mem_tracking_push_name(UCB_NULL);
}

void ucb_mem_tracking_push_name(const char* name)
{
    ucb_mutex_lock(&s_mutex);
    assert(s_trace_points);
    if (s_trace_level < MAX_TRACEPOINTS - 1)
    {
        s_trace_level++;
        ucb_tracepoint_t* tp = (ucb_tracepoint_t*)calloc(1, sizeof(ucb_tracepoint_t));
        if (name && name[0])
        {
            mem_sprintf(tp->name, MAX_TRACEPOINT_NAME, "%s", name);
        }
        else
        {
            mem_sprintf(tp->name, MAX_TRACEPOINT_NAME, "Tracepoint %02d", s_trace_level);
        }

        s_trace_points[s_trace_level] = tp;
    }
    ucb_mutex_unlock(&s_mutex);
}

void ucb_mem_tracking_pop(void)
{
    ucb_mutex_lock(&s_mutex);
    if (s_trace_level == 0)
    {
        ucb_mutex_unlock(&s_mutex);
        return;
    }

    ucb_tracepoint_t* tp = s_trace_points[s_trace_level];
    assert(tp);

    ucb_alloc_meta_t* entry = tp->alloc;
    ucb_alloc_meta_t* next;

    if (entry)
    {
        // Report leak
        ucb_mutex_unlock(&s_mutex);
        gen_tracepoint_report(s_trace_level, true);
        ucb_mutex_lock(&s_mutex);
    }

    int level_up            = s_trace_level - 1;
    ucb_tracepoint_t* tp_up = s_trace_points[level_up];
    assert(tp_up);

#if VERIFY_LEVEL
    verify_level(s_trace_level);
    verify_level(level_up);
#endif

    while (entry)
    {
        assert(entry->level == s_trace_level);

        next = entry->next;

        // Add to parent level
        entry->level = level_up;
        entry->prev  = UCB_NULL;
        entry->next  = tp_up->alloc;
        if (entry->next)
            entry->next->prev = entry;
        tp_up->alloc = entry;

        entry = next;
    }
    tp->alloc = UCB_NULL;

    // Merge stats to parent level
    tp_up->current_alloc += tp->current_alloc;
    tp_up->current_size += tp->current_size;

    tp_up->total_alloc += tp->total_alloc;
    tp_up->total_size += tp->total_size;

    tp_up->peak_alloc += tp->peak_alloc;
    tp_up->peak_size += tp->peak_size;
    tp_up->peak_alloc_block = ucb_max(tp_up->peak_alloc_block, tp->peak_alloc_block);

    free(tp);
    s_trace_points[s_trace_level] = UCB_NULL;
    s_trace_level--;

#if VERIFY_LEVEL
    verify_level(level_up);
#endif
    ucb_mutex_unlock(&s_mutex);
}

void ucb_mem_tracking_report(void)
{
    gen_tracepoint_report(s_trace_level, false);
}

static inline void* register_alloc(ucb_alloc_meta_t* entry, size_t size, const char* file, int line)
{
    assert(entry);

    entry->magic = ALLOC_MAGIC;
    entry->size  = size;
    entry->file  = file;
    entry->line  = line;

    ucb_mutex_lock(&s_mutex);

    ucb_tracepoint_t* tp = s_trace_points[s_trace_level];
    assert(tp);

    // Add as first entry for current level
    entry->level = s_trace_level;
    entry->next  = tp->alloc;
    entry->prev  = UCB_NULL;
    if (entry->next)
    {
        assert(entry->next->magic == ALLOC_MAGIC);
        entry->next->prev = entry;
    }
    tp->alloc = entry;

    // Update metrics
    tp->current_alloc++;
    tp->current_size += entry->size;

    tp->total_alloc++;
    tp->total_size += entry->size;

    tp->peak_alloc       = ucb_max(tp->current_alloc, tp->peak_alloc);
    tp->peak_size        = ucb_max(tp->current_size, tp->peak_size);
    tp->peak_alloc_block = ucb_max(entry->size, tp->peak_alloc_block);

    ucb_mutex_unlock(&s_mutex);

    // Return pointer to the user data
    return (void*)(entry + 1);
}

static inline void unregister_alloc(ucb_alloc_meta_t* entry)
{
    ucb_mutex_lock(&s_mutex);

    assert(entry && entry->magic == ALLOC_MAGIC);
    entry->magic = DEALLOC_MAGIC;

    ucb_tracepoint_t* tp = s_trace_points[s_trace_level];
    assert(tp);

    // Update metrics
    tp->current_alloc--;
    tp->current_size -= entry->size;

    // Remove from current level’s list
    if (entry->prev)
    {
        assert(entry->prev->magic == ALLOC_MAGIC);
        entry->prev->next = entry->next;
    }
    else
        s_trace_points[entry->level]->alloc = entry->next;
    if (entry->next)
    {
        assert(entry->next->magic == ALLOC_MAGIC);
        entry->next->prev = entry->prev;
    }

    ucb_mutex_unlock(&s_mutex);
}

/* -------------------------------------------------------------------------- */
/*                              Memory overloads                              */
/* -------------------------------------------------------------------------- */

void* ucb_malloc_debug(size_t size, const char* file, int line)
{
    if (s_tracking_enabled && size > 0)
    {
        ucb_alloc_meta_t* entry = (ucb_alloc_meta_t*)ucb_malloc(size + META_SIZE);
        return register_alloc(entry, size, file, line);
    }
    return ucb_malloc(size);
}

void* ucb_calloc_debug(size_t num, size_t size, const char* file, int line)
{
    if (s_tracking_enabled && num > 0 && size > 0)
    {
        ucb_alloc_meta_t* entry = (ucb_alloc_meta_t*)ucb_calloc(1, num * size + META_SIZE);
        return register_alloc(entry, num * size, file, line);
    }
    return ucb_calloc(num, size);
}

void* ucb_realloc_debug(void* ptr, size_t size, const char* file, int line)
{
    return ucb_realloc2_debug(ptr, size, true, file, line);
}

void* ucb_realloc2_debug(void* ptr, size_t size, bool free_on_failure, const char* file, int line)
{
    if (!s_tracking_enabled)
        return ucb_realloc2(ptr, size, free_on_failure);

    if (!ptr)
    {
        // No previous allocation, just do the malloc
        return ucb_malloc_debug(size, file, line);
    }

    ucb_alloc_meta_t* entry = ((ucb_alloc_meta_t*)ptr) - 1;
    if (entry->magic != ALLOC_MAGIC)
    {
        ucb_fatal(ucb_error_msg(UCB_ERROR_INVALID_ALLOC,
                                "Invalid allocation, possible memory corruption at %p\n."
                                "Current realloc of %zu bytes called from: %s:%d\n"
                                "NOTE, the following info may be incorrect:\n"
                                "Originally %zu bytes allocated at %s:%s",
                                ptr, size, file, line, entry->size, entry->file, entry->line));
        // If allowed to continue, reallocate and register
        if (size > 0)
        {
            entry = (ucb_alloc_meta_t*)ucb_realloc2(ptr, size + META_SIZE, free_on_failure);
        }
        else
        {
            entry = UCB_NULL;
            ucb_free(ptr);
        }
        if (entry)
            return register_alloc(entry, size, file, line);
        return UCB_NULL;
    }

    ucb_alloc_meta_t* old_entry = entry;

    // Always remove entry first or we may use invalid memory
    unregister_alloc(entry);
    if (size > 0)
    {
        entry = (ucb_alloc_meta_t*)ucb_realloc2(entry, size + META_SIZE, free_on_failure);
    }
    else
    {
        entry = UCB_NULL;
        ucb_free(entry);
    }
    if (entry)
        return register_alloc(entry, size, file, line);
    else if (!free_on_failure)
    {
        // Re-register the old allocation
        // The old_entry pointer is still valid and contains the old data
        register_alloc(old_entry, old_entry->size, old_entry->file, old_entry->line);
    }
    return UCB_NULL;
}

void ucb_free_debug(void* ptr, const char* file, int line)
{
    /*
     * TODO Use file and line to track where free was called from.
     *      For this we need another tracking list or table.
     */
    UCB_UNUSED(file);
    UCB_UNUSED(line);
    if (!s_tracking_enabled)
    {
        ucb_free(ptr);
        return;
    }

    if (!ptr)
        return;

    ucb_alloc_meta_t* entry = ((ucb_alloc_meta_t*)ptr) - 1;
    if (entry->magic == ALLOC_MAGIC)
    {
        unregister_alloc(entry);
        ucb_free(entry);
    }
    else
    {
        ucb_fatal(ucb_error_msg(UCB_ERROR_INVALID_ALLOC,
                                "Invalid allocation, possible memory corruption at %p\n."
                                "Current free called from: %s:%d\n"
                                "NOTE, the following info may be incorrect:\n"
                                "Originally %zu bytes allocated at %s:%s",
                                ptr, file, line, entry->size, entry->file, entry->line));
        // In this case, rather leak than free possibly invalid memory.
    }
}

#endif // NDEBUG
