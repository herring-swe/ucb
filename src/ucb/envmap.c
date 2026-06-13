/**
 * @file envmap.c
 * This file is part of the UCB project
 * - SPDX-FileCopyrightText: © 2026 Åke Svedin <ake@svedin.org>
 * - SPDX-License-Identifier: MIT
 *
 * @brief Environment map implementation
 */

#include "ucb/envmap.h"

#include "ucb/container/impl/vector_ptr.h"
#include "ucb/env.h"
#include "ucb/string.h"
#include "ucb/types.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifdef _WIN32
#include <windows.h>
#else
#include <errno.h>
#endif

struct ucb_envmap
{
    ucb_vector_ptr* entries;
};

typedef struct ucb_envmap_entry
{
    ucb_str* name;
    ucb_str* value;
} ucb_envmap_entry;

/* ---- Helper: find entry index by name, returns -1 if not found ---- */

static size_t envmap_find(const ucb_envmap* map, const char* name)
{
    if (!map || !name || !map->entries)
        return UCB_NPOS;

    for (size_t i = 0; i < ucb_vector_ptr_size(map->entries); ++i)
    {
        ucb_envmap_entry* entry = (ucb_envmap_entry*)ucb_vector_ptr_get(map->entries, i);
        if (entry && entry->name && strcmp(ucb_str_cstr(entry->name), name) == 0)
        {
            return i;
        }
    }
    return UCB_NPOS;
}

/* ---- Helper: free a single entry ---- */

static void envmap_entry_free(ucb_envmap_entry* entry)
{
    if (!entry)
        return;
    ucb_str_free(entry->name);
    ucb_str_free(entry->value);
    free(entry);
}

/* ---- Helper: free all entries in the map ---- */

static void envmap_entries_free(ucb_envmap* map)
{
    if (!map || !map->entries)
        return;

    for (size_t i = 0; i < ucb_vector_ptr_size(map->entries); ++i)
    {
        ucb_envmap_entry* entry = (ucb_envmap_entry*)ucb_vector_ptr_get(map->entries, i);
        envmap_entry_free(entry);
    }

    ucb_vector_ptr_free(map->entries);
    map->entries = UCB_NULL;
}

/* ---- Helper: split "NAME=VALUE" string into name/value entry ---- */

static ucb_envmap_entry* envmap_entry_from_nv(const ucb_str* nv_str)
{
    /* Find the '=' sign in the NV string */
    const char* cstr = ucb_str_cstr(nv_str);
    const char* eq = strchr(cstr, '=');
    if (!eq)
        return UCB_NULL;

    size_t name_len = (size_t)(eq - cstr);
    if (name_len == 0)
        return UCB_NULL;

    ucb_envmap_entry* entry = (ucb_envmap_entry*)malloc(sizeof(ucb_envmap_entry));
    if (!entry)
        return UCB_NULL;

    /* Create name and value from the C string with known byte lengths. */
    entry->name = ucb_str_new(cstr, name_len);
    if (!entry->name)
    {
        free(entry);
        return UCB_NULL;
    }

    /* Value starts after '=', length is total - name_len - 1 (for '=') */
    size_t val_len = ucb_str_len(nv_str) - name_len - 1;
    entry->value = ucb_str_new(eq + 1, val_len);
    if (!entry->value)
    {
        ucb_str_free(entry->name);
        free(entry);
        return UCB_NULL;
    }

    return entry;
}

/* ---- Helper: create a new entry from name/value C strings ---- */

static ucb_envmap_entry* envmap_entry_new(const char* name, const char* value)
{
    ucb_envmap_entry* entry = (ucb_envmap_entry*)malloc(sizeof(ucb_envmap_entry));
    if (!entry)
        return UCB_NULL;

    entry->name = ucb_str_new_c(name ? name : "");
    if (!entry->name)
    {
        free(entry);
        return UCB_NULL;
    }

    entry->value = ucb_str_new_c(value ? value : "");
    if (!entry->value)
    {
        ucb_str_free(entry->name);
        free(entry);
        return UCB_NULL;
    }

    return entry;
}

/* ---- Helper: clone a single entry ---- */

static ucb_envmap_entry* envmap_entry_clone(const ucb_envmap_entry* src)
{
    if (!src || !src->name || !src->value)
        return UCB_NULL;

    ucb_envmap_entry* entry = (ucb_envmap_entry*)malloc(sizeof(ucb_envmap_entry));
    if (!entry)
        return UCB_NULL;

    entry->name = ucb_str_clone(src->name);
    if (!entry->name)
    {
        free(entry);
        return UCB_NULL;
    }

    entry->value = ucb_str_clone(src->value);
    if (!entry->value)
    {
        ucb_str_free(entry->name);
        free(entry);
        return UCB_NULL;
    }

    return entry;
}

/* ===================================================================== */
/*  Public API                                                           */
/* ===================================================================== */

ucb_envmap* ucb_envmap_new(void)
{
    ucb_envmap* map = (ucb_envmap*)malloc(sizeof(ucb_envmap));
    if (!map)
        return UCB_NULL;

    map->entries = ucb_vector_ptr_new();
    if (!map->entries)
    {
        free(map);
        return UCB_NULL;
    }

    return map;
}

void ucb_envmap_free(ucb_envmap* map)
{
    if (!map)
        return;
    envmap_entries_free(map);
    free(map);
}

void ucb_envmap_init_from_current(ucb_envmap* map)
{
    if (!map || !map->entries)
        return;

#ifdef _WIN32
    wchar_t* env_block = GetEnvironmentStringsW();
    if (env_block)
    {
        wchar_t* curr = env_block;
        while (*curr)
        {
            /* Convert the entire "NAME=VALUE" wide string to UTF-8 */
            ucb_str* nv = ucb_str_from_wchar(curr, 0, UCB_NULL);
            if (nv)
            {
                ucb_envmap_entry* entry = envmap_entry_from_nv(nv);
                if (entry)
                    ucb_vector_ptr_push_back(map->entries, entry);

                ucb_str_free(nv);
            }

            /* Advance to next entry (each is null-terminated) */
            curr += wcslen(curr) + 1;
        }
        FreeEnvironmentStringsW(env_block);
    }
#else
    /* POSIX: iterate over the external environ array */
    extern char** environ;
    if (environ)
    {
        for (char** env = environ; *env != UCB_NULL; ++env)
        {
            /* Wrap the entire "NAME=VALUE" string */
            ucb_str nv = ucb_str_make();
            ucb_str_init_wrap_c(&nv, *env);

            ucb_envmap_entry* entry = envmap_entry_from_nv(&nv);
            if (entry)
                ucb_vector_ptr_push_back(map->entries, entry);

            ucb_str_release(&nv);
        }
    }
#endif
}

void ucb_envmap_reserve(ucb_envmap* map, size_t count)
{
    if (!map || !map->entries)
        return;

    ucb_vector_ptr_reserve(map->entries, count);
}

ucb_envmap* ucb_envmap_clone(const ucb_envmap* src)
{
    if (!src)
        return UCB_NULL;

    ucb_envmap* dst = ucb_envmap_new();
    if (!dst)
        return UCB_NULL;

    ucb_envmap_copy(dst, src);
    return dst;
}

void ucb_envmap_copy(ucb_envmap* dst, const ucb_envmap* src)
{
    if (!dst || !src || !src->entries)
        return;

    /* Clear existing entries in dst */
    envmap_entries_free(dst);

    dst->entries = ucb_vector_ptr_new();
    if (!dst->entries)
        return;

    for (size_t i = 0; i < ucb_vector_ptr_size(src->entries); ++i)
    {
        const ucb_envmap_entry* src_entry =
            (const ucb_envmap_entry*)ucb_vector_ptr_get(src->entries, i);
        ucb_envmap_entry* cloned = envmap_entry_clone(src_entry);
        if (cloned)
            ucb_vector_ptr_push_back(dst->entries, cloned);
    }
}

bool ucb_envmap_has(const ucb_envmap* map, const char* name)
{
    return envmap_find(map, name) != UCB_NPOS;
}

const char* ucb_envmap_get(const ucb_envmap* map, const char* name)
{
    size_t idx = envmap_find(map, name);
    if (idx == UCB_NPOS)
        return UCB_NULL;

    ucb_envmap_entry* entry = (ucb_envmap_entry*)ucb_vector_ptr_get(map->entries, (size_t)idx);
    if (!entry || !entry->value)
        return UCB_NULL;

    return ucb_str_cstr(entry->value);
}

bool ucb_envmap_set(ucb_envmap* map, const char* name, const char* value)
{
    if (!map || !name || !map->entries)
        return false;

    size_t idx = envmap_find(map, name);
    if (idx != UCB_NPOS)
    {
        /* Update existing entry's value */
        ucb_envmap_entry* entry = (ucb_envmap_entry*)ucb_vector_ptr_get(map->entries, (size_t)idx);
        return ucb_str_assign_c(entry->value, value);
    }

    /* Insert new entry */
    ucb_envmap_entry* entry = envmap_entry_new(name, value);
    if (!entry)
        return false;

    ucb_vector_ptr_push_back(map->entries, entry);
    return true;
}

bool ucb_envmap_unset(ucb_envmap* map, const char* name)
{
    if (!map || !name || !map->entries)
        return false;

    size_t idx = envmap_find(map, name);
    if (idx == UCB_NPOS)
        return false;

    void* removed = ucb_vector_ptr_remove(map->entries, (size_t)idx);
    envmap_entry_free((ucb_envmap_entry*)removed);
    return true;
}

bool ucb_envmap_append(ucb_envmap* map, const char* name, const char* value, const char* sep)
{
    if (!map || !name || !value || !map->entries)
        return false;

    size_t idx = envmap_find(map, name);
    if (idx != UCB_NPOS)
    {
        /* Variable exists — append with optional separator */
        ucb_envmap_entry* entry = (ucb_envmap_entry*)ucb_vector_ptr_get(map->entries, (size_t)idx);

        if (sep && !ucb_str_is_empty(entry->value))
            ucb_str_append_cstr(entry->value, sep, 0);

        ucb_str_append_cstr(entry->value, value, 0);
        return true;
    }

    /* Variable does not exist — create without separator */
    return ucb_envmap_set(map, name, value);
}

bool ucb_envmap_prepend(ucb_envmap* map, const char* name, const char* value, const char* sep)
{
    if (!map || !name || !value || !map->entries)
        return false;

    size_t idx = envmap_find(map, name);
    if (idx != UCB_NPOS)
    {
        /* Variable exists — prepend with optional separator */
        ucb_envmap_entry* entry = (ucb_envmap_entry*)ucb_vector_ptr_get(map->entries, (size_t)idx);

        /* Build new value: value + sep + old_value */
        ucb_str new_val;
        ucb_str_init_wrap_c(&new_val, value);

        if (sep && !ucb_str_is_empty(entry->value))
            ucb_str_append_c(&new_val, sep);
        ucb_str_append(&new_val, entry->value);

        bool ok = ucb_str_copy(entry->value, &new_val);
        ucb_str_release(&new_val);
        return ok;
    }

    /* Variable does not exist — create without separator */
    return ucb_envmap_set(map, name, value);
}

void ucb_envmap_apply(const ucb_envmap* map)
{
    if (!map || !map->entries)
        return;

    for (size_t i = 0; i < ucb_vector_ptr_size(map->entries); ++i)
    {
        ucb_envmap_entry* entry = (ucb_envmap_entry*)ucb_vector_ptr_get(map->entries, i);
        if (!entry || !entry->name || !entry->value)
            continue;

        const char* name = ucb_str_cstr(entry->name);
        const char* value = ucb_str_cstr(entry->value);

        if (ucb_str_is_empty(entry->value))
        {
            ucb_env_unset(name);
        }
        else
        {
            ucb_env_set(name, value, true);
        }
    }
}
