/**
 * @file dir.c
 *
 * This file is part of the UCB project
 * - SPDX-FileCopyrightText: © 2026 Åke Svedin <ake@svedin.org>
 * - SPDX-License-Identifier: MIT
 *
 * @brief Directory iterator, shared platform independent logic
 */

#include "dir_private.h"
#include "fs_private.h"

#include <ucb/errcodes.h>
#include <ucb/error.h>
#include <ucb/memory.h>

ucb_dir* ucb_dir_open(const char* path, unsigned flags, ucb_error** perr)
{
    UCB_VERIFY_ARGS(path);
    if (!*path)
    {
        ucb_throw(perr, UCB_ERROR_INVALID_ARG, "ucb_dir_open: path must be non-empty");
        return UCB_NULL;
    }

    char* canon = ucb_fs_canon(path);
    if (!canon)
    {
        ucb_throw(perr, UCB_ERROR_OUT_OF_MEMORY, "ucb_dir_open: out of memory");
        return UCB_NULL;
    }

    ucb_dir* dir = ucb_calloc_type(1, ucb_dir);
    if (!dir)
    {
        ucb_free(canon);
        return UCB_NULL;
    }

    dir->path = canon;
    dir->flags = flags;
    dir->closed = false;
    dir->state = UCB_NULL;
    dir->name = UCB_NULL;

    if (!ucb_dir_plat_open(dir, perr))
    {
        ucb_dir_free(dir);
        return UCB_NULL;
    }

    return dir;
}

bool ucb_dir_next(ucb_dir* dir, ucb_dir_entry* out, ucb_error** perr)
{
    UCB_VERIFY_ARGS(dir);
    UCB_VERIFY_ARGS(out);

    if (dir->closed)
    {
        ucb_throw(perr, UCB_ERROR_INVALID_STATE, "ucb_dir_next: iterator is closed");
        return false;
    }

    return ucb_dir_plat_next(dir, out, perr);
}

bool ucb_dir_close(ucb_dir* dir, ucb_error** perr)
{
    if (!dir || dir->closed)
        return true;

    bool ok = ucb_dir_plat_close(dir, perr);
    dir->closed = true;
    return ok;
}

void ucb_dir_free(ucb_dir* dir)
{
    if (!dir)
        return;

    if (!dir->closed)
        (void)ucb_dir_plat_close(dir, UCB_NULL);

    ucb_free(dir->name);
    ucb_free(dir->path);
    ucb_free(dir);
}

bool ucb_dir_list(const char* path, ucb_vector_str** out, ucb_error** perr)
{
    UCB_VERIFY_ARGS(out);
    *out = UCB_NULL;

    ucb_dir* dir = ucb_dir_open(path, 0, perr);
    if (!dir)
        return false;

    ucb_vector_str* names = ucb_vector_str_new();
    if (!names)
    {
        ucb_throw(perr, UCB_ERROR_OUT_OF_MEMORY, "ucb_dir_list: out of memory");
        ucb_dir_free(dir);
        return false;
    }

    bool ok = true;
    ucb_error* derr = UCB_NULL;
    ucb_dir_entry entry;
    while (ucb_dir_next(dir, &entry, &derr))
    {
        ucb_str* name = ucb_str_new_c(entry.name);
        if (!name)
        {
            ucb_throw(perr, UCB_ERROR_OUT_OF_MEMORY, "ucb_dir_list: out of memory");
            ok = false;
            break;
        }
        ucb_vector_str_push_back(names, name);
    }

    if (ok && UCB_IS_THROWN(derr))
    {
        if (perr)
            *perr = derr;
        else
            ucb_error_clear(&derr);
        ok = false;
    }
    else
    {
        ucb_error_clear(&derr);
    }

    ucb_dir_free(dir);

    if (!ok)
    {
        ucb_vector_str_free_full(names);
        return false;
    }

    *out = names;
    return true;
}
