/**
 * @file fs_tmp.cpp
 *
 * This file is part of the UCB project
 * - SPDX-FileCopyrightText: © 2026 Åke Svedin <ake@svedin.org>
 * - SPDX-License-Identifier: MIT
 *
 * @brief Shared temporary-directory helper implementation
 */

#include "fs_tmp.h"

#include <ucb/buffer.h>
#include <ucb/cstring.h>
#include <ucb/error.h>
#include <ucb/file.h>
#include <ucb/fs.h>
#include <ucb/memory.h>

#include <cstdio>
#include <cstdlib>

#ifdef _WIN32
#include <process.h>
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <Windows.h>
#else
#include <unistd.h>
#endif

static unsigned fs_tmp_pid(void)
{
#ifdef _WIN32
    return static_cast<unsigned>(_getpid());
#else
    return static_cast<unsigned>(getpid());
#endif
}

static std::string fs_tmp_root(void)
{
#ifdef _WIN32
    const char* root = std::getenv("TEMP");
    if (!root)
        root = std::getenv("TMP");
    if (!root)
        root = ".";
#else
    const char* root = std::getenv("TMPDIR");
    if (!root)
        root = "/tmp";
#endif
    return root;
}

FsTmp::FsTmp(const char* prefix)
{
    static unsigned counter = 0;
    char buf[256];
    std::snprintf(buf,
                  sizeof(buf),
                  "%s/%s_%u_%u",
                  fs_tmp_root().c_str(),
                  prefix,
                  fs_tmp_pid(),
                  counter++);

    m_dir = buf;

    // Clear a stale leftover from a previous crash, then create.
    (void)ucb_fs_remove_all(m_dir.c_str(), UCB_NULL);
    (void)ucb_fs_mkdir_p(m_dir.c_str(), UCB_NULL);
}

FsTmp::~FsTmp()
{
    (void)ucb_fs_remove_all(m_dir.c_str(), UCB_NULL);
}

std::string FsTmp::path(const char* leaf) const
{
    if (!leaf || !*leaf)
        return m_dir;
    return m_dir + "/" + leaf;
}

bool FsTmp::write(const char* leaf, const std::string& data) const
{
    std::string p = path(leaf);
    ucb_error* err = UCB_NULL;
    ucb_file* file =
        ucb_file_open(p.c_str(), UCB_FILE_CREATE | UCB_FILE_TRUNCATE | UCB_FILE_WRITE, &err);
    if (!file)
    {
        ucb_error_clear(&err);
        return false;
    }

    bool ok = ucb_file_write_full(file, data.data(), data.size(), &err);
    ucb_error_clear(&err);
    ucb_file_free(file);
    return ok;
}

bool FsTmp::read(const char* leaf, std::string& out) const
{
    std::string p = path(leaf);
    ucb_error* err = UCB_NULL;
    ucb_file* file = ucb_file_open(p.c_str(), UCB_FILE_READ, &err);
    if (!file)
    {
        ucb_error_clear(&err);
        return false;
    }

    ucb_buffer* buf = ucb_buffer_new_heap(64);
    bool ok = buf && ucb_file_read_buffer(file, buf, &err);
    if (ok)
        out.assign(buf->data, buf->size);

    ucb_buffer_free(buf);
    ucb_error_clear(&err);
    ucb_file_free(file);
    return ok;
}

bool fs_create_symlink(const char* target, const char* linkpath, bool target_is_dir)
{
#ifdef _WIN32
    wchar_t* wtarget = ucb_cstr_to_wchar(target, 0, UCB_NULL, UCB_NULL);
    wchar_t* wlink = ucb_cstr_to_wchar(linkpath, 0, UCB_NULL, UCB_NULL);
    if (!wtarget || !wlink)
    {
        ucb_free(wtarget);
        ucb_free(wlink);
        return false;
    }

    DWORD flags = 0x2; // SYMBOLIC_LINK_FLAG_ALLOW_UNPRIVILEGED_CREATE
    if (target_is_dir)
        flags |= 0x1; // SYMBOLIC_LINK_FLAG_DIRECTORY

    bool ok = CreateSymbolicLinkW(wlink, wtarget, flags) != 0;
    ucb_free(wtarget);
    ucb_free(wlink);
    return ok;
#else
    (void)target_is_dir;
    return symlink(target, linkpath) == 0;
#endif
}
