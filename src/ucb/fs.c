/**
 * @file fs.c
 *
 * This file is part of the UCB project
 * - SPDX-FileCopyrightText: © 2026 Åke Svedin <ake@svedin.org>
 * - SPDX-License-Identifier: MIT
 *
 * @brief Portable filesystem API, shared platform independent logic
 */

#include "fs_private.h"

#include <ucb/dir.h>
#include <ucb/errcodes.h>
#include <ucb/error.h>
#include <ucb/file.h>
#include <ucb/memory.h>
#include <ucb/path.h>

#include <ctype.h>
#include <stdio.h>
#include <string.h>

#ifdef _WIN32
#include <process.h>
#define fs_getpid() ((unsigned)_getpid())
#else
#include <unistd.h>
#define fs_getpid() ((unsigned)getpid())
#endif

#define UCB_FS_COPY_CHUNK 65536

static bool fs_has_parent_component(const char* path);

/* -------------------------------------------------------------------------- */
/*                              Shared helpers                                */
/* -------------------------------------------------------------------------- */

static char* fs_strdup(const char* str)
{
    size_t len = strlen(str);
    char* out = ucb_malloc(len + 1);
    if (!out)
        return UCB_NULL;
    memcpy(out, str, len + 1);
    return out;
}

char* ucb_fs_canon(const char* path)
{
    if (!path)
        return UCB_NULL;

    ucb_path* parsed = ucb_path_new_c(path);
    if (!parsed)
        return UCB_NULL;

    // Canonicalization otherwise leaves '.' and '..' as written. A '..'
    // component can make a safety guard observe one path while the operating
    // system acts on another (for example '/tmp/..' names the root), so resolve
    // it lexically when present. A lone '.', which cannot escape anything, is
    // left untouched.
    if (fs_has_parent_component(ucb_path_cstr(parsed)))
    {
        if (!ucb_path_normalize(parsed))
        {
            ucb_path_free(parsed);
            return UCB_NULL;
        }
    }

    char* out = fs_strdup(ucb_path_cstr(parsed));
    ucb_path_free(parsed);
    return out;
}

char* ucb_fs_parent(const char* canon)
{
    if (!canon)
        return UCB_NULL;

    ucb_path* parsed = ucb_path_new_c(canon);
    if (!parsed)
        return UCB_NULL;

    ucb_path* parent = ucb_path_parent(parsed);
    ucb_path_free(parsed);
    if (!parent)
        return UCB_NULL;

    char* out = fs_strdup(ucb_path_cstr(parent));
    ucb_path_free(parent);
    return out;
}

char* ucb_fs_join(const char* dir, const char* name)
{
    if (!dir || !name)
        return UCB_NULL;

    size_t dir_len = strlen(dir);
    size_t name_len = strlen(name);
#ifdef _WIN32
    char sep = '\\';
    bool ends_sep = dir_len > 0 && (dir[dir_len - 1] == '\\' || dir[dir_len - 1] == '/');
#else
    char sep = '/';
    bool ends_sep = dir_len > 0 && dir[dir_len - 1] == '/';
#endif

    bool need_sep = dir_len > 0 && !ends_sep;
    char* out = ucb_malloc(dir_len + (need_sep ? 1u : 0u) + name_len + 1);
    if (!out)
        return UCB_NULL;

    memcpy(out, dir, dir_len);
    size_t off = dir_len;
    if (need_sep)
        out[off++] = sep;
    memcpy(out + off, name, name_len);
    out[off + name_len] = '\0';
    return out;
}

static bool fs_is_sep(char c)
{
    if (c == '/')
        return true;
#ifdef _WIN32
    if (c == '\\')
        return true;
#endif
    return false;
}

bool ucb_fs_is_root(const char* canon)
{
    if (!canon || !*canon)
        return false;

    size_t len = strlen(canon);

#ifdef _WIN32
    // UNC share root: \\server\share with nothing after it.
    if (len >= 2 && fs_is_sep(canon[0]) && fs_is_sep(canon[1]))
    {
        size_t i = 2;
        size_t server = i;
        while (i < len && !fs_is_sep(canon[i]))
            i++;
        if (i == server || i >= len)
            return false;
        i++;
        size_t share = i;
        while (i < len && !fs_is_sep(canon[i]))
            i++;
        if (i == share)
            return false;
        return i == len;
    }
    // Drive root: X: or X: with a trailing separator
    if (len >= 2 && isalpha((unsigned char)canon[0]) && canon[1] == ':')
    {
        if (len == 2)
            return true;
        if (len == 3 && fs_is_sep(canon[2]))
            return true;
        return false;
    }
#endif

    for (size_t i = 0; i < len; i++)
    {
        if (!fs_is_sep(canon[i]))
            return false;
    }
    return true;
}

/* -------------------------------------------------------------------------- */
/*                              Internal helpers                              */
/* -------------------------------------------------------------------------- */

static bool fs_copy_link(const char* src, const char* dst, ucb_error** perr);
static bool fs_copy_meta(const char* src, const char* dst, ucb_error** perr);

/**
 * @brief Check whether a canonical path contains a `..` component.
 *
 * Canonicalization deliberately leaves `.` and `..` untouched, so a path such
 * as `/tmp/..` still resolves to the filesystem root at the OS layer. Guards
 * that must not be bypassed test for this explicitly.
 */
static bool fs_has_parent_component(const char* path)
{
    const char* cursor = path;
    while (*cursor)
    {
        while (*cursor == '/' || *cursor == '\\')
            cursor++;

        const char* start = cursor;
        while (*cursor && *cursor != '/' && *cursor != '\\')
            cursor++;

        if ((size_t)(cursor - start) == 2 && start[0] == '.' && start[1] == '.')
            return true;
    }
    return false;
}

static bool fs_component_equal(const char* a, const char* b, size_t len)
{
#ifdef _WIN32
    for (size_t i = 0; i < len; i++)
    {
        if (tolower((unsigned char)a[i]) != tolower((unsigned char)b[i]))
            return false;
    }
    return true;
#else
    return memcmp(a, b, len) == 0;
#endif
}

/**
 * @brief Check whether @p child is @p parent or a descendant of it.
 *
 * Both inputs must be canonical so separator handling is consistent. The
 * comparison is case-insensitive on Windows.
 */
static bool fs_is_within(const char* child, const char* parent)
{
    size_t parent_len = strlen(parent);
    if (parent_len == 0 || strlen(child) < parent_len)
        return false;
    if (!fs_component_equal(child, parent, parent_len))
        return false;
    if (child[parent_len] == '\0')
        return true;
    if (parent[parent_len - 1] == '/' || parent[parent_len - 1] == '\\')
        return true;
    return child[parent_len] == '/' || child[parent_len] == '\\';
}

/**
 * @brief Allocate a temporary sibling path next to @p dst.
 *
 * The result is usable for staging an overwrite so the original destination is
 * replaced only after the new contents are complete. The caller must keep the
 * returned path on the same volume as @p dst.
 */
static char* fs_temp_sibling(const char* dst)
{
    static unsigned counter = 0;
    size_t dst_len = strlen(dst);

    for (unsigned attempt = 0; attempt < 64; attempt++)
    {
        char suffix[64];
        snprintf(suffix, sizeof(suffix), ".ucb-tmp-%u-%u", fs_getpid(), counter++);

        size_t suffix_len = strlen(suffix);
        char* tmp = ucb_malloc(dst_len + suffix_len + 1);
        if (!tmp)
            return UCB_NULL;

        memcpy(tmp, dst, dst_len); // NOLINT(bugprone-not-null-terminated-result): terminated below
        memcpy(tmp + dst_len, suffix, suffix_len + 1);

        ucb_fs_kind kind = UCB_FS_KIND_UNKNOWN;
        ucb_error* lerr = UCB_NULL;
        if (!ucb_fs_plat_kind(tmp, UCB_FS_NOFOLLOW, &kind, &lerr))
        {
            bool missing = UCB_IS_THROWN(lerr) && lerr->code == UCB_ERRSYS_ENOENT;
            ucb_error_clear(&lerr);
            if (missing)
                return tmp;
        }
        ucb_free(tmp);
    }
    return UCB_NULL;
}

static bool fs_stream_copy(const char* src, const char* dst, unsigned open_flags, ucb_error** perr)
{
    ucb_file* in = ucb_file_open(src, UCB_FILE_READ, perr);
    if (!in)
        return false;

    ucb_file* out = ucb_file_open(dst, open_flags, perr);
    if (!out)
    {
        ucb_file_free(in);
        return false;
    }

    bool ok = true;
    char chunk[UCB_FS_COPY_CHUNK];
    for (;;)
    {
        ucb_ssize n = ucb_file_read(in, chunk, sizeof(chunk), perr);
        if (n < 0)
        {
            ok = false;
            break;
        }
        if (n == 0)
            break;
        if (!ucb_file_write_full(out, chunk, (size_t)n, perr))
        {
            ok = false;
            break;
        }
    }

    ucb_file_free(out);
    ucb_file_free(in);
    return ok;
}

/**
 * @brief Write the contents of @p src to @p dst.
 *
 * Symbolic links are recreated unless @ref UCB_FS_FOLLOW is set. Metadata is
 * applied only to a copied payload, never to a recreated link.
 */
static bool fs_copy_payload(const char* src,
                            ucb_fs_kind src_kind,
                            unsigned flags,
                            const char* dst,
                            bool exclusive,
                            ucb_error** perr)
{
    if (src_kind == UCB_FS_KIND_SYMLINK && !(flags & UCB_FS_FOLLOW))
        return fs_copy_link(src, dst, perr);

    unsigned open_flags = UCB_FILE_CREATE | UCB_FILE_WRITE;
    open_flags |= exclusive ? UCB_FILE_EXCL : UCB_FILE_TRUNCATE;

    if (!fs_stream_copy(src, dst, open_flags, perr))
        return false;
    return fs_copy_meta(src, dst, perr);
}

static bool fs_check_path(const char* path, ucb_error** perr)
{
    if (!*path)
    {
        ucb_throw(perr, UCB_ERROR_INVALID_ARG, "path must be non-empty");
        return false;
    }
    return true;
}

/**
 * @brief Query a kind, treating a missing entry as a distinct outcome.
 *
 * Returns true when the query itself succeeded, with @p exists set to false for
 * a missing entry. On a real failure the error is propagated and false returned.
 */
static bool fs_kind_exists(const char* canon,
                           unsigned flags,
                           ucb_fs_kind* kind,
                           bool* exists,
                           ucb_error** perr)
{
    ucb_error* lerr = UCB_NULL;
    if (ucb_fs_plat_kind(canon, flags, kind, &lerr))
    {
        *exists = true;
        return true;
    }

    if (UCB_IS_THROWN(lerr) && lerr->code == UCB_ERRSYS_ENOENT)
    {
        ucb_error_clear(&lerr);
        *exists = false;
        return true;
    }

    if (perr)
    {
        ucb_error_clear(perr);
        *perr = lerr;
    }
    else
    {
        ucb_error_clear(&lerr);
    }
    return false;
}

static bool fs_mkdir_p(const char* canon, ucb_error** perr)
{
    ucb_error* lerr = UCB_NULL;
    if (ucb_fs_plat_mkdir(canon, &lerr))
        return true;

    if (UCB_IS_THROWN(lerr) && lerr->code == UCB_ERRSYS_EEXIST)
    {
        ucb_error_clear(&lerr);
        ucb_fs_kind kind;
        ucb_error* kerr = UCB_NULL;
        if (!ucb_fs_plat_kind(canon, UCB_FS_NOFOLLOW, &kind, &kerr))
        {
            if (perr)
                *perr = kerr;
            else
                ucb_error_clear(&kerr);
            return false;
        }
        if (kind == UCB_FS_KIND_DIR)
            return true;
        ucb_throw(perr,
                  UCB_ERRSYS_ENOTDIR,
                  "ucb_fs_mkdir_p: an existing component is not a directory");
        return false;
    }

    if (!(UCB_IS_THROWN(lerr) && lerr->code == UCB_ERRSYS_ENOENT))
    {
        if (perr)
            *perr = lerr;
        else
            ucb_error_clear(&lerr);
        return false;
    }
    ucb_error_clear(&lerr);

    char* parent = ucb_fs_parent(canon);
    if (!parent || !*parent || strcmp(parent, canon) == 0)
    {
        ucb_free(parent);
        ucb_throw(perr, UCB_ERRSYS_ENOENT, "ucb_fs_mkdir_p: could not reach a parent directory");
        return false;
    }

    bool ok = fs_mkdir_p(parent, perr);
    ucb_free(parent);
    if (!ok)
        return false;

    lerr = UCB_NULL;
    if (ucb_fs_plat_mkdir(canon, &lerr))
        return true;
    if (UCB_IS_THROWN(lerr) && lerr->code == UCB_ERRSYS_EEXIST)
    {
        ucb_error_clear(&lerr);
        return true;
    }
    if (perr)
        *perr = lerr;
    else
        ucb_error_clear(&lerr);
    return false;
}

static bool fs_copy_meta(const char* src, const char* dst, ucb_error** perr)
{
    ucb_fs_stat st;
    if (!ucb_fs_plat_stat(src, 0, &st, perr))
        return false;
    if (!ucb_fs_plat_set_mode(dst, st.mode, perr))
        return false;
    return ucb_fs_plat_set_mtime(dst, st.mtime, perr);
}

#ifdef _WIN32
static bool fs_link_target_is_dir(const char* link_path, const char* target, ucb_error** perr)
{
    UCB_UNUSED(perr);

    ucb_path* parsed = ucb_path_new_c(target);
    if (!parsed)
        return false;
    bool is_abs = ucb_path_is_absolute(parsed);
    ucb_path_free(parsed);

    char* resolved = UCB_NULL;
    if (is_abs)
    {
        resolved = fs_strdup(target);
    }
    else
    {
        char* parent = ucb_fs_parent(link_path);
        if (parent)
            resolved = ucb_fs_join(parent, target);
        ucb_free(parent);
    }
    if (!resolved)
        return false;

    ucb_fs_kind kind = UCB_FS_KIND_UNKNOWN;
    ucb_error* lerr = UCB_NULL;
    bool ok = ucb_fs_plat_kind(resolved, 0, &kind, &lerr);
    ucb_free(resolved);
    ucb_error_clear(&lerr);
    return ok && kind == UCB_FS_KIND_DIR;
}
#endif // _WIN32

static bool fs_copy_link(const char* src, const char* dst, ucb_error** perr)
{
    char* target = UCB_NULL;
    if (!ucb_fs_plat_read_link(src, &target, perr))
        return false;

    bool target_is_dir = false;
#ifdef _WIN32
    target_is_dir = fs_link_target_is_dir(src, target, perr);
#else
    UCB_UNUSED(src);
#endif

    bool ok = ucb_fs_plat_create_link(target, dst, target_is_dir, perr);
    ucb_free(target);
    return ok;
}

static bool fs_copy_dir(const char* src, const char* dst, unsigned flags, ucb_error** perr)
{
    ucb_dir* dir = ucb_dir_open(src, 0, perr);
    if (!dir)
        return false;

    bool ok = true;
    ucb_error* derr = UCB_NULL;
    ucb_dir_entry entry;
    while (ucb_dir_next(dir, &entry, &derr))
    {
        char* child_src = ucb_fs_join(src, entry.name);
        char* child_dst = ucb_fs_join(dst, entry.name);
        if (!child_src || !child_dst)
        {
            ucb_throw(perr, UCB_ERROR_OUT_OF_MEMORY, "ucb_fs_copy_tree: out of memory");
            ucb_free(child_src);
            ucb_free(child_dst);
            ok = false;
            break;
        }

        unsigned query_flags = (flags & UCB_FS_FOLLOW) ? 0u : UCB_FS_NOFOLLOW;
        ucb_fs_kind kind = UCB_FS_KIND_UNKNOWN;
        ucb_error* kerr = UCB_NULL;
        if (!ucb_fs_plat_kind(child_src, query_flags, &kind, &kerr))
        {
            if (perr)
                *perr = kerr;
            else
                ucb_error_clear(&kerr);
            ucb_free(child_src);
            ucb_free(child_dst);
            ok = false;
            break;
        }

        if (kind == UCB_FS_KIND_DIR)
        {
            ok = fs_mkdir_p(child_dst, perr) && fs_copy_dir(child_src, child_dst, flags, perr) &&
                 fs_copy_meta(child_src, child_dst, perr);
        }
        else
        {
            ok = ucb_fs_copy_file(child_src, child_dst, flags, perr);
        }

        ucb_free(child_src);
        ucb_free(child_dst);
        if (!ok)
            break;
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
    return ok;
}

static bool fs_remove_all(const char* canon, ucb_error** perr)
{
    ucb_error* lerr = UCB_NULL;
    ucb_fs_kind kind = UCB_FS_KIND_UNKNOWN;
    if (!ucb_fs_plat_kind(canon, UCB_FS_NOFOLLOW, &kind, &lerr))
    {
        if (UCB_IS_THROWN(lerr) && lerr->code == UCB_ERRSYS_ENOENT)
        {
            ucb_error_clear(&lerr);
            return true;
        }
        if (perr)
            *perr = lerr;
        else
            ucb_error_clear(&lerr);
        return false;
    }

    if (kind != UCB_FS_KIND_DIR)
        return ucb_fs_plat_remove_file(canon, perr);

    ucb_dir* dir = ucb_dir_open(canon, 0, perr);
    if (!dir)
        return false;

    bool ok = true;
    ucb_error* derr = UCB_NULL;
    ucb_dir_entry entry;
    while (ucb_dir_next(dir, &entry, &derr))
    {
        char* child = ucb_fs_join(canon, entry.name);
        if (!child)
        {
            ucb_throw(perr, UCB_ERROR_OUT_OF_MEMORY, "ucb_fs_remove_all: out of memory");
            ok = false;
            break;
        }
        ok = fs_remove_all(child, perr);
        ucb_free(child);
        if (!ok)
            break;
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
        return false;

    return ucb_fs_plat_remove_dir(canon, perr);
}

/* -------------------------------------------------------------------------- */
/*                             Simple predicates                              */
/* -------------------------------------------------------------------------- */

static bool fs_predicate_kind(const char* path, unsigned flags, ucb_fs_kind wanted)
{
    if (!*path)
        return false;

    char* canon = ucb_fs_canon(path);
    if (!canon)
        return false;

    ucb_fs_kind kind = UCB_FS_KIND_UNKNOWN;
    bool exists = false;
    bool ok = fs_kind_exists(canon, flags, &kind, &exists, UCB_NULL);
    ucb_free(canon);
    return ok && exists && kind == wanted;
}

bool ucb_fs_exists(const char* path)
{
    UCB_VERIFY_ARGS(path);
    if (!*path)
        return false;

    char* canon = ucb_fs_canon(path);
    if (!canon)
        return false;

    ucb_fs_kind kind = UCB_FS_KIND_UNKNOWN;
    bool exists = false;
    bool ok = fs_kind_exists(canon, 0, &kind, &exists, UCB_NULL);
    ucb_free(canon);
    return ok && exists;
}

bool ucb_fs_is_file(const char* path)
{
    UCB_VERIFY_ARGS(path);
    return fs_predicate_kind(path, 0, UCB_FS_KIND_FILE);
}

bool ucb_fs_is_dir(const char* path)
{
    UCB_VERIFY_ARGS(path);
    return fs_predicate_kind(path, 0, UCB_FS_KIND_DIR);
}

bool ucb_fs_is_symlink(const char* path)
{
    UCB_VERIFY_ARGS(path);
    return fs_predicate_kind(path, UCB_FS_NOFOLLOW, UCB_FS_KIND_SYMLINK);
}

/* -------------------------------------------------------------------------- */
/*                              Error-aware query                             */
/* -------------------------------------------------------------------------- */

bool ucb_fs_get_kind(const char* path, unsigned flags, ucb_fs_kind* out, ucb_error** perr)
{
    UCB_VERIFY_ARGS(path);
    UCB_VERIFY_ARGS(out);
    if (!fs_check_path(path, perr))
        return false;

    char* canon = ucb_fs_canon(path);
    if (!canon)
    {
        ucb_throw(perr, UCB_ERROR_OUT_OF_MEMORY, "ucb_fs_kind: out of memory");
        return false;
    }

    bool ok = ucb_fs_plat_kind(canon, flags, out, perr);
    ucb_free(canon);
    return ok;
}

bool ucb_fs_get_stat(const char* path, unsigned flags, ucb_fs_stat* out, ucb_error** perr)
{
    UCB_VERIFY_ARGS(path);
    UCB_VERIFY_ARGS(out);
    if (!fs_check_path(path, perr))
        return false;

    char* canon = ucb_fs_canon(path);
    if (!canon)
    {
        ucb_throw(perr, UCB_ERROR_OUT_OF_MEMORY, "ucb_fs_stat: out of memory");
        return false;
    }

    bool ok = ucb_fs_plat_stat(canon, flags, out, perr);
    ucb_free(canon);
    return ok;
}

bool ucb_fs_get_stat_ns(const char* path, unsigned flags, ucb_fs_stat_ns* out, ucb_error** perr)
{
    UCB_VERIFY_ARGS(path);
    UCB_VERIFY_ARGS(out);
    if (!fs_check_path(path, perr))
        return false;

    char* canon = ucb_fs_canon(path);
    if (!canon)
    {
        ucb_throw(perr, UCB_ERROR_OUT_OF_MEMORY, "ucb_fs_stat_ns: out of memory");
        return false;
    }

    bool ok = ucb_fs_plat_stat_ns(canon, flags, out, perr);
    ucb_free(canon);
    return ok;
}

/* -------------------------------------------------------------------------- */
/*                                Modification                                */
/* -------------------------------------------------------------------------- */

bool ucb_fs_mkdir(const char* path, ucb_error** perr)
{
    UCB_VERIFY_ARGS(path);
    if (!fs_check_path(path, perr))
        return false;

    char* canon = ucb_fs_canon(path);
    if (!canon)
    {
        ucb_throw(perr, UCB_ERROR_OUT_OF_MEMORY, "ucb_fs_mkdir: out of memory");
        return false;
    }

    bool ok = ucb_fs_plat_mkdir(canon, perr);
    ucb_free(canon);
    return ok;
}

bool ucb_fs_mkdir_p(const char* path, ucb_error** perr)
{
    UCB_VERIFY_ARGS(path);
    if (!fs_check_path(path, perr))
        return false;

    char* canon = ucb_fs_canon(path);
    if (!canon)
    {
        ucb_throw(perr, UCB_ERROR_OUT_OF_MEMORY, "ucb_fs_mkdir_p: out of memory");
        return false;
    }

    bool ok = fs_mkdir_p(canon, perr);
    ucb_free(canon);
    return ok;
}

bool ucb_fs_remove(const char* path, ucb_error** perr)
{
    UCB_VERIFY_ARGS(path);
    if (!fs_check_path(path, perr))
        return false;

    char* canon = ucb_fs_canon(path);
    if (!canon)
    {
        ucb_throw(perr, UCB_ERROR_OUT_OF_MEMORY, "ucb_fs_remove: out of memory");
        return false;
    }

    ucb_fs_kind kind = UCB_FS_KIND_UNKNOWN;
    bool ok = ucb_fs_plat_kind(canon, UCB_FS_NOFOLLOW, &kind, perr);
    if (ok)
    {
        if (kind == UCB_FS_KIND_DIR)
            ok = ucb_fs_plat_remove_dir(canon, perr);
        else
            ok = ucb_fs_plat_remove_file(canon, perr);
    }
    ucb_free(canon);
    return ok;
}

bool ucb_fs_remove_all(const char* path, ucb_error** perr)
{
    UCB_VERIFY_ARGS(path);
    if (!fs_check_path(path, perr))
        return false;

    char* canon = ucb_fs_canon(path);
    if (!canon)
    {
        ucb_throw(perr, UCB_ERROR_OUT_OF_MEMORY, "ucb_fs_remove_all: out of memory");
        return false;
    }

    if (ucb_fs_is_root(canon))
    {
        ucb_throw(perr, UCB_ERROR_INVALID_ARG, "ucb_fs_remove_all: refusing to remove a root");
        ucb_free(canon);
        return false;
    }

    bool ok = fs_remove_all(canon, perr);
    ucb_free(canon);
    return ok;
}

bool ucb_fs_copy_file(const char* src, const char* dst, unsigned flags, ucb_error** perr)
{
    UCB_VERIFY_ARGS(src);
    UCB_VERIFY_ARGS(dst);
    if (!fs_check_path(src, perr) || !fs_check_path(dst, perr))
        return false;

    char* canon_src = ucb_fs_canon(src);
    char* canon_dst = ucb_fs_canon(dst);
    if (!canon_src || !canon_dst)
    {
        ucb_throw(perr, UCB_ERROR_OUT_OF_MEMORY, "ucb_fs_copy_file: out of memory");
        ucb_free(canon_src);
        ucb_free(canon_dst);
        return false;
    }

    bool ok = false;

    ucb_fs_kind src_kind = UCB_FS_KIND_UNKNOWN;
    bool src_exists = false;
    if (!fs_kind_exists(canon_src, UCB_FS_NOFOLLOW, &src_kind, &src_exists, perr))
        goto done;
    if (!src_exists)
    {
        ucb_throw(perr, UCB_ERRSYS_ENOENT, "ucb_fs_copy_file: source does not exist");
        goto done;
    }

    ucb_fs_kind dst_kind = UCB_FS_KIND_UNKNOWN;
    bool dst_exists = false;
    if (!fs_kind_exists(canon_dst, UCB_FS_NOFOLLOW, &dst_kind, &dst_exists, perr))
        goto done;
    if (dst_exists && dst_kind == UCB_FS_KIND_DIR)
    {
        ucb_throw(perr, UCB_ERRSYS_EISDIR, "ucb_fs_copy_file: destination is a directory");
        goto done;
    }
    if (dst_exists && !(flags & UCB_FS_OVERWRITE))
    {
        ucb_throw(perr, UCB_ERRSYS_EEXIST, "ucb_fs_copy_file: destination exists");
        goto done;
    }
    if (src_kind == UCB_FS_KIND_DIR)
    {
        ucb_throw(perr, UCB_ERRSYS_EISDIR, "ucb_fs_copy_file: source is a directory");
        goto done;
    }

    if (dst_exists)
    {
        // Stage next to the destination and rename over it, so the original is
        // preserved until the new copy is complete.
        char* tmp = fs_temp_sibling(canon_dst);
        if (!tmp)
        {
            ucb_throw(perr, UCB_ERROR_OUT_OF_MEMORY, "ucb_fs_copy_file: out of memory");
            goto done;
        }

        ok = fs_copy_payload(canon_src, src_kind, flags, tmp, true, perr);
        if (ok)
            ok = ucb_fs_plat_rename(tmp, canon_dst, UCB_FS_OVERWRITE, perr);
        if (!ok)
            (void)ucb_fs_plat_remove_file(tmp, UCB_NULL);
        ucb_free(tmp);
    }
    else
    {
        ok = fs_copy_payload(canon_src, src_kind, flags, canon_dst, false, perr);
        if (!ok)
            (void)ucb_fs_plat_remove_file(canon_dst, UCB_NULL);
    }

done:
    ucb_free(canon_src);
    ucb_free(canon_dst);
    return ok;
}

bool ucb_fs_copy_tree(const char* src, const char* dst, unsigned flags, ucb_error** perr)
{
    UCB_VERIFY_ARGS(src);
    UCB_VERIFY_ARGS(dst);
    if (!fs_check_path(src, perr) || !fs_check_path(dst, perr))
        return false;

    char* canon_src = ucb_fs_canon(src);
    char* canon_dst = ucb_fs_canon(dst);
    if (!canon_src || !canon_dst)
    {
        ucb_throw(perr, UCB_ERROR_OUT_OF_MEMORY, "ucb_fs_copy_tree: out of memory");
        ucb_free(canon_src);
        ucb_free(canon_dst);
        return false;
    }

    if (fs_is_within(canon_dst, canon_src))
    {
        ucb_throw(perr,
                  UCB_ERROR_INVALID_ARG,
                  "ucb_fs_copy_tree: destination is inside the source");
        ucb_free(canon_src);
        ucb_free(canon_dst);
        return false;
    }

    bool ok = false;

    ucb_fs_kind src_kind = UCB_FS_KIND_UNKNOWN;
    if (!ucb_fs_plat_kind(canon_src, 0, &src_kind, perr))
        goto done;
    if (src_kind != UCB_FS_KIND_DIR)
    {
        ucb_throw(perr, UCB_ERRSYS_ENOTDIR, "ucb_fs_copy_tree: source is not a directory");
        goto done;
    }

    ucb_fs_kind dst_kind = UCB_FS_KIND_UNKNOWN;
    bool dst_exists = false;
    if (!fs_kind_exists(canon_dst, UCB_FS_NOFOLLOW, &dst_kind, &dst_exists, perr))
        goto done;

    if (dst_exists)
    {
        if (dst_kind != UCB_FS_KIND_DIR)
        {
            ucb_throw(perr, UCB_ERRSYS_ENOTDIR, "ucb_fs_copy_tree: destination is not a directory");
            goto done;
        }
        if (!(flags & UCB_FS_OVERWRITE))
        {
            ucb_throw(perr, UCB_ERRSYS_EEXIST, "ucb_fs_copy_tree: destination exists");
            goto done;
        }
    }
    else if (!fs_mkdir_p(canon_dst, perr))
    {
        goto done;
    }

    ok = fs_copy_dir(canon_src, canon_dst, flags, perr);
    if (ok)
        ok = fs_copy_meta(canon_src, canon_dst, perr);

done:
    ucb_free(canon_src);
    ucb_free(canon_dst);
    return ok;
}

bool ucb_fs_move(const char* src, const char* dst, unsigned flags, ucb_error** perr)
{
    UCB_VERIFY_ARGS(src);
    UCB_VERIFY_ARGS(dst);
    if (!fs_check_path(src, perr) || !fs_check_path(dst, perr))
        return false;

    char* canon_src = ucb_fs_canon(src);
    char* canon_dst = ucb_fs_canon(dst);
    if (!canon_src || !canon_dst)
    {
        ucb_throw(perr, UCB_ERROR_OUT_OF_MEMORY, "ucb_fs_move: out of memory");
        ucb_free(canon_src);
        ucb_free(canon_dst);
        return false;
    }

    bool ok = false;

    ucb_fs_kind src_kind = UCB_FS_KIND_UNKNOWN;
    bool src_exists = false;
    if (!fs_kind_exists(canon_src, UCB_FS_NOFOLLOW, &src_kind, &src_exists, perr))
        goto done;
    if (!src_exists)
    {
        ucb_throw(perr, UCB_ERRSYS_ENOENT, "ucb_fs_move: source does not exist");
        goto done;
    }

    ucb_fs_kind dst_kind = UCB_FS_KIND_UNKNOWN;
    bool dst_exists = false;
    if (!fs_kind_exists(canon_dst, UCB_FS_NOFOLLOW, &dst_kind, &dst_exists, perr))
        goto done;
    if (dst_exists)
    {
        if (dst_kind == UCB_FS_KIND_DIR)
        {
            ucb_throw(perr, UCB_ERRSYS_EEXIST, "ucb_fs_move: destination is a directory");
            goto done;
        }
        if (src_kind == UCB_FS_KIND_DIR)
        {
            ucb_throw(perr, UCB_ERRSYS_ENOTDIR, "ucb_fs_move: cannot move a directory onto a file");
            goto done;
        }
        if (!(flags & UCB_FS_OVERWRITE))
        {
            ucb_throw(perr, UCB_ERRSYS_EEXIST, "ucb_fs_move: destination exists");
            goto done;
        }
    }

    {
        ucb_error* lerr = UCB_NULL;
        if (ucb_fs_plat_rename(canon_src, canon_dst, flags, &lerr))
        {
            ok = true;
            goto done;
        }

        bool cross_device = UCB_IS_THROWN(lerr) && lerr->code == UCB_ERRSYS_EXDEV;
        if (!cross_device)
        {
            if (perr)
                *perr = lerr;
            else
                ucb_error_clear(&lerr);
            goto done;
        }
        ucb_error_clear(&lerr);
    }

    if (src_kind == UCB_FS_KIND_DIR)
        ok = ucb_fs_copy_tree(canon_src, canon_dst, flags, perr);
    else
        ok = ucb_fs_copy_file(canon_src, canon_dst, flags, perr);
    if (!ok)
        goto done;

    if (src_kind == UCB_FS_KIND_DIR)
        ok = ucb_fs_remove_all(canon_src, perr);
    else
        ok = ucb_fs_remove(canon_src, perr);

done:
    ucb_free(canon_src);
    ucb_free(canon_dst);
    return ok;
}

/* -------------------------------------------------------------------------- */
/*                             Working directory                              */
/* -------------------------------------------------------------------------- */

ucb_str* ucb_fs_cwd(void)
{
    return ucb_fs_plat_cwd();
}

bool ucb_fs_chdir(const char* path, ucb_error** perr)
{
    UCB_VERIFY_ARGS(path);
    if (!fs_check_path(path, perr))
        return false;

    char* canon = ucb_fs_canon(path);
    if (!canon)
    {
        ucb_throw(perr, UCB_ERROR_OUT_OF_MEMORY, "ucb_fs_chdir: out of memory");
        return false;
    }

    bool ok = ucb_fs_plat_chdir(canon, perr);
    ucb_free(canon);
    return ok;
}
