/**
 * @file memdbg.c
 *
 * This file is part of the UCB project
 * - SPDX-FileCopyrightText: © 2026 Åke Svedin <ake@svedin.org>
 * - SPDX-License-Identifier: MIT
 *
 * @brief Memory debugging functions implementation
 */

#ifndef NDEBUG

#define UCB_MEMORY_IMPL

#include "ucb/memdbg.h"

#include "ucb/btrace.h"
#include "ucb/config.h"
#include "ucb/debug.h"
#include "ucb/error.h"
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

#define ALLOC_MAGIC 0xBEA01234
#define DEALLOC_MAGIC 0x0DEADBEA
#define META_SIZE sizeof(ucb_alloc_meta)

typedef struct ucb_alloc_meta
{
    struct ucb_alloc_meta* prev;
    struct ucb_alloc_meta* next;
#ifdef UCB_MEMTRACK_BACKTRACE
    ucb_btrace bt;
#endif
    size_t size;
    const char* file;
    int line;
    int level; // tracepoint
    uint32_t magic;
} ucb_alloc_meta;

typedef struct ucb_tracepoint
{
    char name[UCB_MEMTRACK_MAX_TRACEPOINT_NAME];
    ucb_alloc_meta* alloc;
    size_t current_alloc;    // Current number of allocations
    size_t current_size;     // Current allocated memory
    size_t peak_alloc;       // Max number of allocations at any point
    size_t peak_size;        // Max allocated memory at any point
    size_t peak_alloc_block; // Biggest memory block allocated
    size_t total_alloc;      // Total number of allocations
    size_t total_size;       // Total allocated memory
} ucb_tracepoint_t;

/*
 * Bounded, hash keyed free log. Retains the call site of tracked frees so that
 * reports can list where memory was freed from and double frees can be detected.
 * All memory for this table is raw stdlib allocation; this translation unit
 * defines UCB_MEMORY_IMPL so no re-entrant tracking occurs.
 */
typedef struct ucb_free_meta
{
    void* ptr;              // User pointer that was freed
    size_t size;            // Size of freed block
    const char* alloc_file; // Allocation site
    int alloc_line;
    const char* free_file; // Free call site
    int free_line;
    int level; // tracepoint level at free time
#ifdef UCB_MEMTRACK_BACKTRACE
    ucb_btrace bt; // free-site backtrace
#endif
    struct ucb_free_meta* hash_next;
    struct ucb_free_meta* age_prev;
    struct ucb_free_meta* age_next;
    struct ucb_free_meta* pool_next;
} ucb_free_meta;

// Global list of allocations
static ucb_tracepoint_t* s_trace_points[UCB_MEMTRACK_MAX_TRACEPOINTS];
static int s_trace_level = -1;
static ucb_mem_report_func s_report_func = UCB_NULL;

static ucb_mutex s_mutex = {0};

// Global setting
static bool s_tracking_enabled = false;
static bool s_trace_limit_reached = false;

// Free log (guarded by s_mutex)
static ucb_free_meta* s_free_pool = UCB_NULL;
static ucb_free_meta* s_free_slots = UCB_NULL;
static ucb_free_meta* s_free_buckets[UCB_MEMTRACK_FREE_CAPACITY];
static ucb_free_meta* s_free_newest = UCB_NULL;
static ucb_free_meta* s_free_oldest = UCB_NULL;
static size_t s_free_count = 0;

#define UCB_MAX(a, b) ((a) > (b) ? (a) : (b))

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
    UCB_DIAG_PUSH()
    UCB_DIAG_IGN_FORMAT_NONLITERAL()
    int ret = vsnprintf(str, size, fmt, args);
    UCB_DIAG_POP()
    va_end(args);
    return ret;
}

/* -------------------------------------------------------------------------- */
/*                                Free log table                              */
/* -------------------------------------------------------------------------- */
/*
 * All helpers below assume s_mutex is held unless noted. All table memory is
 * raw stdlib allocation and invisible to memory tracking.
 */

#define FREE_HASH(ptr) (((uintptr_t)(ptr) >> 4) % UCB_MEMTRACK_FREE_CAPACITY)

static ucb_free_meta* free_table_find_locked(void* ptr)
{
    if (!s_free_pool)
        return UCB_NULL;

    for (ucb_free_meta* node = s_free_buckets[FREE_HASH(ptr)]; node; node = node->hash_next)
    {
        if (node->ptr == ptr)
            return node;
    }
    return UCB_NULL;
}

// Unlink a node from its bucket and the age list, releasing any owned backtrace.
static void free_table_detach_locked(ucb_free_meta* node)
{
    assert(node);

    ucb_free_meta** link = &s_free_buckets[FREE_HASH(node->ptr)];
    while (*link)
    {
        if (*link == node)
        {
            *link = node->hash_next;
            break;
        }
        link = &(*link)->hash_next;
    }

    if (node->age_prev)
        node->age_prev->age_next = node->age_next;
    else
        s_free_newest = node->age_next;
    if (node->age_next)
        node->age_next->age_prev = node->age_prev;
    else
        s_free_oldest = node->age_prev;

    node->hash_next = UCB_NULL;
    node->age_prev = UCB_NULL;
    node->age_next = UCB_NULL;

#ifdef UCB_MEMTRACK_BACKTRACE
    ucb_btrace_release(&node->bt);
#endif

    s_free_count--;
}

// Return a detached node to the slot pool.
static void free_slot_return_locked(ucb_free_meta* node)
{
    node->pool_next = s_free_slots;
    s_free_slots = node;
}

static void free_table_remove_locked(void* ptr)
{
    ucb_free_meta* node = free_table_find_locked(ptr);
    if (!node)
        return;

    free_table_detach_locked(node);
    free_slot_return_locked(node);
}

// Takes ownership of @p bt, which is stored on success or released otherwise.
static void free_table_insert_locked(void* ptr,
                                     size_t size,
                                     const char* alloc_file,
                                     int alloc_line,
                                     const char* free_file,
                                     int free_line,
                                     int level,
                                     ucb_btrace* bt)
{
    if (!s_free_pool)
    {
#ifdef UCB_MEMTRACK_BACKTRACE
        if (bt)
            ucb_btrace_release(bt);
#else
        UCB_UNUSED(bt);
#endif
        return;
    }

    // An address can be reused before its old record was dropped
    free_table_remove_locked(ptr);

    ucb_free_meta* slot = s_free_slots;
    if (slot)
    {
        s_free_slots = slot->pool_next;
    }
    else
    {
        // Evict the oldest record to make room
        slot = s_free_oldest;
        if (!slot)
            return;
        free_table_detach_locked(slot);
    }

    slot->ptr = ptr;
    slot->size = size;
    slot->alloc_file = alloc_file;
    slot->alloc_line = alloc_line;
    slot->free_file = free_file;
    slot->free_line = free_line;
    slot->level = level;
#ifdef UCB_MEMTRACK_BACKTRACE
    if (bt)
        slot->bt = *bt;
    else
        ucb_btrace_init(&slot->bt);
#else
    UCB_UNUSED(bt);
#endif
    slot->hash_next = s_free_buckets[FREE_HASH(ptr)];
    slot->age_prev = UCB_NULL;
    slot->age_next = s_free_newest;
    slot->pool_next = UCB_NULL;

    s_free_buckets[FREE_HASH(ptr)] = slot;
    if (s_free_newest)
        s_free_newest->age_prev = slot;
    else
        s_free_oldest = slot;
    s_free_newest = slot;
    s_free_count++;
}

// Release every record and rebuild the slot pool.
static void free_table_clear_locked(void)
{
    if (!s_free_pool)
        return;

#ifdef UCB_MEMTRACK_BACKTRACE
    for (ucb_free_meta* node = s_free_newest; node; node = node->age_next)
        ucb_btrace_release(&node->bt);
#endif

    memset(s_free_buckets, 0, sizeof(s_free_buckets));
    s_free_newest = UCB_NULL;
    s_free_oldest = UCB_NULL;
    s_free_count = 0;

    s_free_slots = UCB_NULL;
    for (size_t i = 0; i < UCB_MEMTRACK_FREE_CAPACITY; i++)
    {
        s_free_pool[i].pool_next = s_free_slots;
        s_free_slots = &s_free_pool[i];
    }
}

/* -------------------------------------------------------------------------- */
/*                               Implementation                               */
/* -------------------------------------------------------------------------- */

static void ucb_mem_tracking_default_report_func(const ucb_mem_report* const report)
{
    if (!report)
        return;

    printf("*** Memory report *************************************************************\n");
    printf("Tracepoint: %s (Level %d)\n", report->name, report->level);
    printf("  Current: %zu allocs, %zu bytes\n", report->current_alloc, report->current_size);
    printf("  Peaks: %zu allocs, %zu bytes, largest block %zu\n",
           report->peak_alloc,
           report->peak_size,
           report->peak_alloc_block);
    printf("  Totals: %zu allocs, %zu bytes\n", report->total_alloc, report->total_size);

    // List leaks
    if (report->allocs)
    {
        printf("\n");
        if (report->leaks)
            printf("!! LEAKS DETECTED !!\n\n");
        printf("Current allocations:\n\n");
    }
    for (ucb_mem_alloc* alloc = report->allocs; alloc; alloc = alloc->next)
    {
        printf("    Address: %p of size %zu bytes\n", alloc->ptr, alloc->size);
        printf("    Allocated at: %s:%d\n", alloc->file, alloc->line);
        if (alloc->bt)
        {
            printf("    Backtrace:\n");
            ucb_btrace_print(alloc->bt, stdout, 8);
        }
        printf("\n");
    }

    if (report->free_count > 0)
    {
        printf("\nFreed allocations (last %zu):\n\n", report->free_count);
        for (ucb_mem_free* free_rec = report->frees; free_rec; free_rec = free_rec->next)
        {
            printf("    Address: %p of size %zu bytes\n", free_rec->ptr, free_rec->size);
            printf("    Allocated at: %s:%d\n", free_rec->alloc_file, free_rec->alloc_line);
            printf("    Freed at: %s:%d\n", free_rec->free_file, free_rec->free_line);
            if (free_rec->bt)
            {
                printf("    Free backtrace:\n");
                ucb_btrace_print(free_rec->bt, stdout, 8);
            }
            printf("\n");
        }
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

    size_t num = 0;
    size_t size = 0;

    ucb_alloc_meta* prev = UCB_NULL;
    for (ucb_alloc_meta* cur = tp->alloc; cur; cur = cur->next)
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

    ucb_mem_report* report = UCB_NULL;
    ucb_mem_alloc* last = UCB_NULL;
    ucb_mem_alloc* cur = UCB_NULL;

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
            report = (ucb_mem_report*)calloc(1, sizeof(ucb_mem_report));
            if (!report)
            {
                ucb_mutex_unlock(&s_mutex);
                UCB_FATAL(UCB_ERROR_OUT_OF_MEMORY, "Failed to allocate memory for memory report");
                from_level = -1;
                goto cleanup;
            }
            report->level = level;
            report->name = tp->name;
            report->leaks = leaks;
        }

        report->peak_alloc = UCB_MAX(report->peak_alloc, tp->peak_alloc);
        report->peak_size = UCB_MAX(report->peak_size, tp->peak_size);
        report->peak_alloc_block = UCB_MAX(report->peak_alloc_block, tp->peak_alloc_block);
        report->total_alloc += tp->total_alloc;
        report->total_size += tp->total_size;

        for (ucb_alloc_meta* alloc = tp->alloc; alloc; alloc = alloc->next)
        {
            cur = (ucb_mem_alloc*)calloc(1, sizeof(ucb_mem_alloc));
            if (!cur)
            {
                ucb_mutex_unlock(&s_mutex);
                UCB_FATAL(UCB_ERROR_OUT_OF_MEMORY, "Failed to allocate memory for memory report");
                from_level = -1;
                goto cleanup;
            }

            cur->ptr = alloc + 1;
            cur->size = alloc->size;
            cur->file = alloc->file;
            cur->line = alloc->line;

#ifdef UCB_MEMTRACK_BACKTRACE
            cur->bt = ucb_btrace_clone(&alloc->bt);
#endif

            report->current_alloc++;
            report->current_size += cur->size;

            if (last)
                last->next = cur;
            else
                report->allocs = cur;
            last = cur;
        }
    }

    // Retained free log is global, independent of the report level
    ucb_mem_free* last_free = UCB_NULL;
    for (ucb_free_meta* rec = s_free_newest; rec; rec = rec->age_next)
    {
        ucb_mem_free* node = (ucb_mem_free*)calloc(1, sizeof(ucb_mem_free));
        if (!node)
        {
            ucb_mutex_unlock(&s_mutex);
            UCB_FATAL(UCB_ERROR_OUT_OF_MEMORY, "Failed to allocate memory for memory report");
            from_level = -1;
            goto cleanup;
        }

        node->ptr = rec->ptr;
        node->size = rec->size;
        node->alloc_file = rec->alloc_file;
        node->alloc_line = rec->alloc_line;
        node->free_file = rec->free_file;
        node->free_line = rec->free_line;
        node->level = rec->level;
#ifdef UCB_MEMTRACK_BACKTRACE
        node->bt = ucb_btrace_clone(&rec->bt);
#endif

        if (last_free)
            last_free->next = node;
        else
            report->frees = node;
        last_free = node;
        report->free_count++;
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

        ucb_mem_free* free_cur = report->frees;
        while (free_cur)
        {
            ucb_mem_free* free_next = free_cur->next;
#ifdef UCB_MEMTRACK_BACKTRACE
            if (free_cur->bt)
                ucb_btrace_free(free_cur->bt);
#endif
            free(free_cur);
            free_cur = free_next;
        }
        free(report);
    }
    return from_level;
}

void ucb_mem_tracking_enable(void)
{
    if (s_tracking_enabled)
        return;

    ucb_mutex_init(&s_mutex);

    // Pre-allocate the free log pool. Raw allocation, tracking is still off and
    // the mutex is not held, so a fatal report cannot deadlock or re-enter.
    s_free_pool = (ucb_free_meta*)calloc(UCB_MEMTRACK_FREE_CAPACITY, sizeof(ucb_free_meta));
    if (!s_free_pool)
        UCB_FATAL(UCB_ERROR_OUT_OF_MEMORY,
                  "Failed to allocate memory for free tracking table, frees are not tracked");

    ucb_mutex_lock(&s_mutex);
    s_tracking_enabled = true;
    s_trace_limit_reached = false;

    free_table_clear_locked();

    s_report_func = ucb_mem_tracking_default_report_func;
    s_trace_level = 0;
    s_trace_points[0] = (ucb_tracepoint_t*)calloc(1, sizeof(ucb_tracepoint_t));
    mem_sprintf(s_trace_points[0]->name, UCB_MEMTRACK_MAX_TRACEPOINT_NAME, "%s", "all memory");

    for (int i = 0; i < UCB_MEMTRACK_MAX_TRACEPOINTS; i++)
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

            tp->peak_alloc = 0;
            tp->peak_size = 0;
            tp->peak_alloc_block = 0;
            tp->total_alloc = 0;
            tp->total_size = 0;
        }
    }
    free_table_clear_locked();
    ucb_mutex_unlock(&s_mutex);
}

ucb_mem_report_func ucb_mem_tracking_set_report_func(ucb_mem_report_func func)
{
    ucb_mem_report_func old = s_report_func;
    s_report_func = func;
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
    if (s_trace_level < UCB_MEMTRACK_MAX_TRACEPOINTS - 1)
    {
        s_trace_level++;
        ucb_tracepoint_t* tp = (ucb_tracepoint_t*)calloc(1, sizeof(ucb_tracepoint_t));
        if (name && name[0])
        {
            mem_sprintf(tp->name, UCB_MEMTRACK_MAX_TRACEPOINT_NAME, "%s", name);
        }
        else
        {
            mem_sprintf(tp->name,
                        UCB_MEMTRACK_MAX_TRACEPOINT_NAME,
                        "Tracepoint %02d",
                        s_trace_level);
        }

        s_trace_points[s_trace_level] = tp;
    }
    else if (!s_trace_limit_reached)
    {
        s_trace_limit_reached = true;
        UCB_DPRINT("Memory tracking: tracepoint limit (%d) reached, further pushes are ignored\n",
                   UCB_MEMTRACK_MAX_TRACEPOINTS);
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

    ucb_alloc_meta* entry = tp->alloc;
    ucb_alloc_meta* next;

    if (entry)
    {
        // Report leak
        ucb_mutex_unlock(&s_mutex);
        gen_tracepoint_report(s_trace_level, true);
        ucb_mutex_lock(&s_mutex);
    }

    int level_up = s_trace_level - 1;
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
        entry->prev = UCB_NULL;
        entry->next = tp_up->alloc;
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
    tp_up->peak_alloc_block = UCB_MAX(tp_up->peak_alloc_block, tp->peak_alloc_block);

    free(tp);
    s_trace_points[s_trace_level] = UCB_NULL;
    s_trace_level--;

#if VERIFY_LEVEL
    verify_level(level_up);
#endif
    ucb_mutex_unlock(&s_mutex);
}

void ucb_mem_tracking_report(bool final)
{
    if (final)
        gen_tracepoint_report(s_trace_level >= 0 ? 0 : -1, true);
    else
        gen_tracepoint_report(s_trace_level, false);
}

static inline void init_alloc(ucb_alloc_meta* entry,
                              size_t size,
                              const char* file,
                              int line,
                              ucb_btrace* bt)
{
    assert(entry);

    entry->magic = ALLOC_MAGIC;
    entry->size = size;
    entry->file = file;
    entry->line = line;

#ifdef UCB_MEMTRACK_BACKTRACE
    if (bt)
    {
        entry->bt.count = bt->count;
        entry->bt.strs = bt->strs;
    }
    else
    {
        ucb_btrace_init(&entry->bt);
        ucb_btrace_capture(&entry->bt);
    }
#else
    UCB_UNUSED(bt);
#endif
}

static inline void* register_alloc(ucb_alloc_meta* entry)
{
    assert(entry);

    ucb_mutex_lock(&s_mutex);

    // Drop any stale free record for a reused address, prevents false double frees
    free_table_remove_locked((void*)(entry + 1));

    ucb_tracepoint_t* tp = s_trace_points[s_trace_level];
    assert(tp);

    // Add as first entry for current level
    entry->level = s_trace_level;
    entry->next = tp->alloc;
    entry->prev = UCB_NULL;
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

    tp->peak_alloc = UCB_MAX(tp->current_alloc, tp->peak_alloc);
    tp->peak_size = UCB_MAX(tp->current_size, tp->peak_size);
    tp->peak_alloc_block = UCB_MAX(entry->size, tp->peak_alloc_block);

    ucb_mutex_unlock(&s_mutex);

    // Return pointer to the user data
    return (void*)(entry + 1);
}

static inline void unregister_alloc_locked(ucb_alloc_meta* entry)
{
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
}

static inline void unregister_alloc(ucb_alloc_meta* entry)
{
    ucb_mutex_lock(&s_mutex);
    unregister_alloc_locked(entry);
    ucb_mutex_unlock(&s_mutex);
}

// Record a free event. Captures the free-site backtrace outside the lock.
static void record_free(void* ptr,
                        size_t size,
                        const char* alloc_file,
                        int alloc_line,
                        const char* free_file,
                        int free_line)
{
    ucb_btrace bt = {UCB_NULL, 0};
#ifdef UCB_MEMTRACK_BACKTRACE
    ucb_btrace_init(&bt);
    ucb_btrace_capture(&bt);
#endif

    ucb_mutex_lock(&s_mutex);
    free_table_insert_locked(ptr,
                             size,
                             alloc_file,
                             alloc_line,
                             free_file,
                             free_line,
                             s_trace_level,
                             &bt);
    ucb_mutex_unlock(&s_mutex);
}

/* -------------------------------------------------------------------------- */
/*                              Memory overloads                              */
/* -------------------------------------------------------------------------- */

void* ucb_malloc_debug(size_t size, const char* file, int line)
{
    if (s_tracking_enabled && size > 0)
    {
        ucb_alloc_meta* entry = (ucb_alloc_meta*)ucb_malloc(size + META_SIZE);
        init_alloc(entry, size, file, line, UCB_NULL);
        return register_alloc(entry);
    }
    return ucb_malloc(size);
}

void* ucb_calloc_debug(size_t num, size_t size, const char* file, int line)
{
    if (s_tracking_enabled && num > 0 && size > 0)
    {
        ucb_alloc_meta* entry = (ucb_alloc_meta*)ucb_calloc(1, num * size + META_SIZE);
        init_alloc(entry, num * size, file, line, UCB_NULL);
        return register_alloc(entry);
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

    ucb_alloc_meta* entry = ((ucb_alloc_meta*)ptr) - 1;
    if (entry->magic != ALLOC_MAGIC)
    {
        UCB_FATAL(UCB_ERROR_INVALID_ALLOC,
                  "Invalid allocation, possible memory corruption at %p\n"
                  "Current realloc of %zu bytes called from: %s:%d",
                  ptr,
                  size,
                  file,
                  line);
        // If allowed to continue, reallocate and register
        if (size > 0)
        {
            entry = (ucb_alloc_meta*)ucb_realloc2(ptr, size + META_SIZE, free_on_failure);
        }
        else
        {
            entry = UCB_NULL;
            ucb_free(ptr);
        }
        if (entry)
        {
            init_alloc(entry, size, file, line, UCB_NULL);
            return register_alloc(entry);
        }
        return UCB_NULL;
    }

    ucb_alloc_meta* old_entry = entry;
    void* old_ptr = (void*)(entry + 1);
    size_t old_size = entry->size;
    const char* old_file = entry->file;
    int old_line = entry->line;

    // Always remove entry first or we may use invalid memory
    unregister_alloc(entry);
#ifdef UCB_MEMTRACK_BACKTRACE
    // Save old btrace data, in order to free it on failure
    ucb_btrace old_bt = {
        .count = entry->bt.count,
        .strs = entry->bt.strs,
    };
#endif
    if (size > 0)
    {
        entry = (ucb_alloc_meta*)ucb_realloc2(entry, size + META_SIZE, free_on_failure);
#ifdef UCB_MEMTRACK_BACKTRACE
        if (!entry && free_on_failure)
            ucb_btrace_release(&old_bt);
#endif
        if (!entry && free_on_failure)
            record_free(old_ptr, old_size, old_file, old_line, file, line);
    }
    else // Regular free
    {
        record_free(old_ptr, old_size, old_file, old_line, file, line);
#ifdef UCB_MEMTRACK_BACKTRACE
        ucb_btrace_release(&entry->bt);
#endif
        ucb_free(entry);
        entry = UCB_NULL;
    }
    if (entry)
    {
        // Carry over previous info including btrace
#ifdef UCB_MEMTRACK_BACKTRACE
        init_alloc(entry, size, file, line, &old_bt);
#else
        init_alloc(entry, size, file, line, UCB_NULL);
#endif
        return register_alloc(entry);
    }
    else if (!free_on_failure)
    {
        // Re-register the old allocation
        // The old_entry pointer is still valid and contains the old data
        register_alloc(old_entry);
    }
    return UCB_NULL;
}

void ucb_free_debug(void* ptr, const char* file, int line)
{
    if (!s_tracking_enabled)
    {
        ucb_free(ptr);
        return;
    }

    if (!ptr)
        return;

    ucb_btrace bt = {UCB_NULL, 0};
#ifdef UCB_MEMTRACK_BACKTRACE
    ucb_btrace_init(&bt);
    ucb_btrace_capture(&bt);
#endif

    ucb_alloc_meta* entry = ((ucb_alloc_meta*)ptr) - 1;

    ucb_mutex_lock(&s_mutex);

    // The free log is checked before the magic because free() overwrites the
    // header of an already freed block.
    ucb_free_meta* prev = free_table_find_locked(ptr);
    if (prev)
    {
        const char* prev_file = prev->free_file;
        int prev_line = prev->free_line;
        ucb_mutex_unlock(&s_mutex);
#ifdef UCB_MEMTRACK_BACKTRACE
        ucb_btrace_release(&bt);
#endif
        UCB_FATAL(UCB_ERROR_INVALID_ALLOC,
                  "Double free at %p, previously freed from: %s:%d, now called from: %s:%d",
                  ptr,
                  prev_file,
                  prev_line,
                  file,
                  line);
        // Do not free again, rather leak than corrupt the heap.
        return;
    }

    if (entry->magic != ALLOC_MAGIC)
    {
        ucb_mutex_unlock(&s_mutex);
#ifdef UCB_MEMTRACK_BACKTRACE
        ucb_btrace_release(&bt);
#endif
        UCB_FATAL(UCB_ERROR_INVALID_ALLOC,
                  "Invalid allocation, possible memory corruption at %p\n"
                  "Current free called from: %s:%d",
                  ptr,
                  file,
                  line);
        // In this case, rather leak than free possibly invalid memory.
        return;
    }

    size_t size = entry->size;
    const char* alloc_file = entry->file;
    int alloc_line = entry->line;

    unregister_alloc_locked(entry);

    free_table_insert_locked(ptr, size, alloc_file, alloc_line, file, line, s_trace_level, &bt);

    ucb_mutex_unlock(&s_mutex);

#ifdef UCB_MEMTRACK_BACKTRACE
    ucb_btrace_release(&entry->bt);
#endif
    ucb_free(entry);
}

#endif // NDEBUG
