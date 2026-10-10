/**
 * @file path.c
 *
 * This file is part of the UCB project
 * - SPDX-FileCopyrightText: © 2026 Åke Svedin <ake@svedin.org>
 * - SPDX-License-Identifier: MIT
 *
 * @brief Path type and related functions implementation
 */

#include "ucb/path.h"

#include "ucb/errcodes.h"
#include "ucb/error.h"
#include "ucb/memory.h"
#include "ucb/types.h"
#include "ucb/ucb.h"

#include <ctype.h>
#include <stdint.h>
#include <string.h>

/* -------------------------------------------------------------------------- */
/*                              Internal helpers                              */
/* -------------------------------------------------------------------------- */

/**
 * Internal. A separator accepted on input. `/` is always accepted and `\` is
 * additionally accepted on Windows builds.
 */
static inline bool path_is_sep(char c)
{
    if (c == '/')
        return true;
#ifdef _WIN32
    if (c == '\\')
        return true;
#endif
    return false;
}

/**
 * Internal. True if @p c is a separator for the stored/composed form: either an
 * input separator or the resolved output separator @p extra_sep. This makes
 * style-aware operations work even when the output separator is not accepted
 * on input for the current platform (e.g. `\` on a POSIX build).
 */
static inline bool path_sep_match(char c, char extra_sep)
{
    return path_is_sep(c) || (extra_sep != 0 && c == extra_sep);
}

/**
 * Internal. Resolve a style to a concrete one using the thread-local config.
 */
static ucb_path_style path_resolve_style(ucb_path_style style)
{
    if (style == UCB_PATH_STYLE_DEFAULT)
    {
        ucb_path_style def = ucb_conf_get().default_path_style;
        if (def == UCB_PATH_STYLE_DEFAULT)
            def = UCB_PATH_STYLE_NATIVE;
        return def;
    }
    return style;
}

/**
 * Internal. Output separator character for a (resolved) style.
 */
static char path_sep_of(ucb_path_style style)
{
    switch (path_resolve_style(style))
    {
    case UCB_PATH_STYLE_POSIX:
        return '/';
    case UCB_PATH_STYLE_WINDOWS:
        return '\\';
    default:
        break;
    }

#ifdef _WIN32
    return '\\';
#else
    return '/';
#endif
}

typedef enum path_anchor_kind
{
    PATH_ANCHOR_NONE = 0,
    PATH_ANCHOR_ROOT,  ///< A single leading separator (`/` or `//` on POSIX)
    PATH_ANCHOR_DRIVE, ///< `X:`
    PATH_ANCHOR_UNC,   ///< `\\server\share`
} path_anchor_kind;

/**
 * Internal. Detect the leading anchor of @p cstr[0, len) and store its byte
 * length in @p out_len. Returns the anchor kind or @ref PATH_ANCHOR_NONE.
 */
static path_anchor_kind path_detect_anchor(const char* cstr,
                                           size_t len,
                                           char extra_sep,
                                           size_t* out_len)
{
    *out_len = 0;
    if (len == 0 || cstr == UCB_NULL)
        return PATH_ANCHOR_NONE;

#ifdef _WIN32
    if (len >= 2 && path_sep_match(cstr[0], extra_sep) && path_sep_match(cstr[1], extra_sep))
    {
        size_t i = 2;
        while (i < len && !path_sep_match(cstr[i], extra_sep))
            i++;
        if (i < len)
        {
            i++; // consume separator
            size_t share_start = i;
            while (i < len && !path_sep_match(cstr[i], extra_sep))
                i++;
            if (i > share_start)
            {
                *out_len = i;
                return PATH_ANCHOR_UNC;
            }
        }
        // Not a valid UNC anchor: fall through to a plain leading separator
    }
    if (len >= 2 && cstr[1] == ':' && isalpha((unsigned char)cstr[0]) &&
        (len == 2 || path_sep_match(cstr[2], extra_sep)))
    {
        *out_len = 2;
        return PATH_ANCHOR_DRIVE;
    }
    if (path_sep_match(cstr[0], extra_sep))
    {
        *out_len = 1;
        return PATH_ANCHOR_ROOT;
    }
#else
    if (path_sep_match(cstr[0], extra_sep))
    {
        if (len >= 2 && path_sep_match(cstr[1], extra_sep) &&
            (len < 3 || !path_sep_match(cstr[2], extra_sep)))
        {
            *out_len = 2;
            return PATH_ANCHOR_ROOT;
        }
        *out_len = 1;
        return PATH_ANCHOR_ROOT;
    }
#endif

    return PATH_ANCHOR_NONE;
}

/**
 * Internal. True if @p ptr points into one of the owned allocations of @p path.
 */
static bool path_str_aliases(const ucb_str* str, const char* ptr)
{
    return str->alloc > 0 && ptr != UCB_NULL && ptr >= str->data && ptr < str->data + str->alloc;
}

static bool path_aliases(const ucb_path* path, const char* ptr)
{
    if (ptr == UCB_NULL)
        return false;
    return path_str_aliases(&path->dir, ptr) || path_str_aliases(&path->stem, ptr) ||
           path_str_aliases(&path->ext, ptr) || path_str_aliases(&path->_name, ptr) ||
           path_str_aliases(&path->_cache, ptr);
}

/**
 * Internal. Report a user error (aborts) when an explicit-length string
 * contains an embedded null character.
 */
static void path_check_no_nul(const char* cstr, size_t len)
{
    if (len && memchr(cstr, '\0', len) != UCB_NULL)
        UCB_REPORT(UCB_ERROR_INVALID_ARG, "Embedded null character");
}

/**
 * Internal. Ensure @p str has room for @p extra more bytes. Grows
 * geometrically so repeated appends amortize, and works around
 * ucb_str_reserve() reporting "no reallocation happened" as failure.
 */
static bool path_reserve(ucb_str* str, size_t extra)
{
    size_t need = str->size + extra + 1;
    if (need <= str->alloc)
        return true;

    size_t grow = str->alloc ? str->alloc * 2 : need;
    if (grow > need)
        extra = grow - str->size - 1;

    ucb_str_reserve(str, extra);
    return str->alloc >= need;
}

/**
 * Internal. Append @p len bytes to @p str, growing it as needed.
 */
static bool path_append_cstr(ucb_str* str, const char* cstr, size_t len)
{
    if (len == 0)
        return true;
    if (!path_reserve(str, len))
        return false;
    memcpy(str->data + str->size, cstr, len);
    str->size += len;
    str->data[str->size] = '\0';
    return true;
}

/**
 * Internal. Append a directory segment, inserting a separator at the junction
 * unless @p dir is empty or already ends with one.
 */
static bool path_append_segment(ucb_str* dir, const char* seg, size_t len, char sep)
{
    if (len == 0)
        return true;
    bool need_sep = dir->size > 0 && dir->data[dir->size - 1] != sep;
    if (!path_reserve(dir, (need_sep ? 1u : 0u) + len))
        return false;
    size_t off = dir->size;
    if (need_sep)
        dir->data[off++] = sep;
    memcpy(dir->data + off, seg, len);
    off += len;
    dir->data[off] = '\0';
    dir->size = off;
    return true;
}

/**
 * Internal. Normalize a raw directory region into @p out: collapse repeated
 * separators, strip a trailing separator unless the value is a root, and
 * normalize the anchor.
 */
static bool path_build_dir(ucb_str* out, const char* cstr, size_t len, char sep)
{
    size_t anchor_len = 0;
    path_anchor_kind ak = path_detect_anchor(cstr, len, 0, &anchor_len);

    ucb_str tmp = ucb_str_make();
    bool ok = true;

    for (size_t i = 0; i < anchor_len && ok; i++)
    {
        char c = (char)(path_is_sep(cstr[i]) ? sep : cstr[i]);
        ok = path_append_cstr(&tmp, &c, 1);
    }

    bool pending_sep = false;
    bool has_content = false;
    for (size_t i = anchor_len; i < len && ok; i++)
    {
        char c = cstr[i];
        if (path_is_sep(c))
        {
            pending_sep = true;
        }
        else
        {
            if (pending_sep && tmp.size > 0)
                ok = path_append_cstr(&tmp, &sep, 1);
            if (ok)
                ok = path_append_cstr(&tmp, &c, 1);
            has_content = true;
            pending_sep = false;
        }
    }

    // A root without any further content keeps a single trailing separator,
    // except a UNC anchor which is stored without one.
    if (ok && anchor_len > 0 && !has_content && ak != PATH_ANCHOR_UNC)
    {
        if (tmp.size == 0 || tmp.data[tmp.size - 1] != sep)
            ok = path_append_cstr(&tmp, &sep, 1);
    }

    if (ok)
        ok = ucb_str_copy(out, &tmp);
    ucb_str_release(&tmp);
    return ok;
}

/**
 * Internal. Split a filename into stem/extension and store them in @p path.
 */
static bool path_parse_filename(ucb_path* path, const char* cstr, size_t len)
{
    size_t start = 0;
    while (start < len && cstr[start] == '.')
        start++;

    size_t dot = UCB_NPOS;
    if (len >= 2)
    {
        for (size_t i = len - 1; i-- > 0;)
        {
            if (i < start)
                break;
            if (i == 0)
                break;
            if (cstr[i] == '.')
            {
                dot = i;
                break;
            }
        }
    }

    bool ok;
    if (dot == UCB_NPOS)
    {
        ok = (len == 0) ? ucb_str_assign(&path->stem, UCB_NULL, 0)
                        : ucb_str_assign(&path->stem, cstr, len);
        ok = ucb_str_assign(&path->ext, UCB_NULL, 0) && ok;
    }
    else
    {
        ok = (dot == 0) ? ucb_str_assign(&path->stem, UCB_NULL, 0)
                        : ucb_str_assign(&path->stem, cstr, dot);
        ok = ucb_str_assign(&path->ext, cstr + dot, len - dot) && ok;
    }
    return ok;
}

/**
 * Internal. Parse @p cstr[0, len) into @p path using @p style.
 */
static bool path_parse(ucb_path* path, const char* cstr, size_t len, ucb_path_style style)
{
    char sep = path_sep_of(style);
    path->style = path_resolve_style(style);

    if (cstr == UCB_NULL)
        len = 0;

    if (len == 0)
    {
        bool ok = ucb_str_assign(&path->dir, UCB_NULL, 0);
        ok = ucb_str_assign(&path->stem, UCB_NULL, 0) && ok;
        ok = ucb_str_assign(&path->ext, UCB_NULL, 0) && ok;
        path->_dirty = true;
        return ok;
    }

    size_t last = UCB_NPOS;
    for (size_t i = len; i-- > 0;)
    {
        if (path_is_sep(cstr[i]))
        {
            last = i;
            break;
        }
    }

    size_t raw_dir_len = (last == UCB_NPOS) ? 0 : last + 1;

    bool ok = path_build_dir(&path->dir, cstr, raw_dir_len, sep);

    const char* fname;
    size_t flen;
    if (last == UCB_NPOS)
    {
        fname = cstr;
        flen = len;
    }
    else
    {
        fname = cstr + last + 1;
        flen = len - last - 1;
    }
    if (ok)
        ok = path_parse_filename(path, fname, flen);

    path->_dirty = true;
    return ok;
}

static bool path_init_ex(ucb_path* path, const char* cstr, size_t len, ucb_path_style style)
{
    UCB_VERIFY_ARGS(path);

    *path = ucb_path_make();

    if (cstr == UCB_NULL)
    {
        path->style = path_resolve_style(style);
        path->_dirty = true;
        return true;
    }

    if (len == 0)
        len = strlen(cstr);
    else
        path_check_no_nul(cstr, len);

    return path_parse(path, cstr, len, style);
}

/**
 * Internal. Build @c stem + @c ext into @p out.
 */
static bool path_build_name(const ucb_path* path, ucb_str* out)
{
    ucb_str_clear(out);
    bool ok = path_append_cstr(out, path->stem.data, path->stem.size);
    if (ok)
        ok = path_append_cstr(out, path->ext.data, path->ext.size);
    return ok;
}

/**
 * Internal. Compute the composed size of @p path. Optionally reports the
 * resolved separator and whether a junction separator is required. This is the
 * single source of truth for both @ref path_compose_to and @ref path_length.
 */
static size_t path_compose_info(const ucb_path* path, char* sep_out, bool* need_sep_out)
{
    char sep = path_sep_of(path->style);
    size_t dir_len = path->dir.size;
    bool has_name = path->stem.size > 0 || path->ext.size > 0;
    bool need_sep = dir_len > 0 && has_name && path->dir.data[dir_len - 1] != sep;
    if (sep_out)
        *sep_out = sep;
    if (need_sep_out)
        *need_sep_out = need_sep;
    return dir_len + (need_sep ? 1u : 0u) + path->stem.size + path->ext.size;
}

/**
 * Internal. Compose @p path into @p out. Never relies on the caches.
 */
static bool path_compose_to(const ucb_path* path, ucb_str* out)
{
    char sep = 0;
    bool need_sep = false;
    size_t total = path_compose_info(path, &sep, &need_sep);
    size_t dir_len = path->dir.size;

    ucb_str_clear(out);
    if (total == 0)
        return true;

    if (!path_reserve(out, total))
        return false;

    size_t off = 0;
    if (dir_len)
    {
        memcpy(out->data, path->dir.data, dir_len);
        off += dir_len;
    }
    if (need_sep)
        out->data[off++] = sep;
    if (path->stem.size)
    {
        memcpy(out->data + off, path->stem.data, path->stem.size);
        off += path->stem.size;
    }
    if (path->ext.size)
    {
        memcpy(out->data + off, path->ext.data, path->ext.size);
        off += path->ext.size;
    }
    out->data[off] = '\0';
    out->size = off;
    return true;
}

/**
 * Internal. Rebuild the private caches if needed.
 */
static void path_update(ucb_path* path)
{
    if (!path->_dirty)
        return;
    bool ok = path_build_name(path, &path->_name);
    if (ok)
        ok = path_compose_to(path, &path->_cache);
    path->_dirty = !ok;
}

/**
 * Internal. Byte length of the composed path.
 */
static size_t path_length(const ucb_path* path)
{
    return path_compose_info(path, UCB_NULL, UCB_NULL);
}

/**
 * Internal. Build the anchor sub-string of a directory into @p out.
 */
static bool path_build_anchor(const char* dir,
                              path_anchor_kind ak,
                              size_t anchor_len,
                              char sep,
                              ucb_str* out)
{
    ucb_str_clear(out);
    for (size_t i = 0; i < anchor_len; i++)
    {
        char c = (char)(path_sep_match(dir[i], sep) ? sep : dir[i]);
        if (!path_append_cstr(out, &c, 1))
            return false;
    }
    if (ak == PATH_ANCHOR_DRIVE)
    {
        if (!path_append_cstr(out, &sep, 1))
            return false;
    }
    return true;
}

/**
 * Internal. Lexically normalize @p path[0, len) into @p out.
 */
static bool path_normalize_lexical(ucb_str* out, const char* path, size_t len, char sep)
{
    ucb_str_clear(out);

    size_t anchor_len = 0;
    path_anchor_kind ak = path_detect_anchor(path, len, sep, &anchor_len);

    for (size_t i = 0; i < anchor_len; i++)
    {
        char c = (char)(path_sep_match(path[i], sep) ? sep : path[i]);
        if (!path_append_cstr(out, &c, 1))
            return false;
    }

    size_t* starts = ucb_malloc((len + 1) * sizeof(size_t));
    if (!starts)
        return false;

    bool ok = true;
    size_t depth = 0;
    size_t base = 0;
    size_t i = anchor_len;
    while (i < len)
    {
        while (i < len && path_sep_match(path[i], sep))
            i++;
        if (i >= len)
            break;

        size_t s = i;
        while (i < len && !path_sep_match(path[i], sep))
            i++;
        size_t seg_len = i - s;
        const char* seg = path + s;

        if (seg_len == 1 && seg[0] == '.')
            continue;

        if (seg_len == 2 && seg[0] == '.' && seg[1] == '.')
        {
            if (depth > base)
            {
                depth--;
                out->size = starts[depth];
                out->data[out->size] = '\0';
            }
            else if (ak == PATH_ANCHOR_NONE)
            {
                starts[depth] = out->size;
                if (out->size > 0 && out->data[out->size - 1] != sep)
                {
                    if (!path_append_cstr(out, &sep, 1))
                    {
                        ok = false;
                        break;
                    }
                }
                if (!path_append_cstr(out, seg, seg_len))
                {
                    ok = false;
                    break;
                }
                depth++;
                base++;
            }
            continue;
        }

        starts[depth] = out->size;
        if (out->size > 0 && out->data[out->size - 1] != sep)
        {
            if (!path_append_cstr(out, &sep, 1))
            {
                ok = false;
                break;
            }
        }
        if (!path_append_cstr(out, seg, seg_len))
        {
            ok = false;
            break;
        }
        depth++;
    }

    ucb_free(starts);
    if (!ok)
        return false;

    // A drive that resolved down to its anchor keeps the root separator.
    if (ak == PATH_ANCHOR_DRIVE && depth == 0 && out->size > 0 && out->data[out->size - 1] != sep)
    {
        if (!path_append_cstr(out, &sep, 1))
            return false;
    }

    // An empty relative result denotes the current directory.
    if (out->size == 0 && len > 0 && ak == PATH_ANCHOR_NONE)
    {
        if (!path_append_cstr(out, ".", 1))
            return false;
    }
    return true;
}

/* -------------------------------------------------------------------------- */
/*                                Construction                                */
/* -------------------------------------------------------------------------- */

ucb_path* ucb_path_new(const char* cstr, size_t len)
{
    ucb_path* path = ucb_malloc_type(1, ucb_path);
    if (!path)
        return UCB_NULL;
    *path = ucb_path_make();
    if (!ucb_path_init(path, cstr, len))
    {
        ucb_path_free(path);
        return UCB_NULL;
    }
    return path;
}

ucb_path* ucb_path_new_style(const char* cstr, ucb_path_style style)
{
    ucb_path* path = ucb_malloc_type(1, ucb_path);
    if (!path)
        return UCB_NULL;
    *path = ucb_path_make();
    if (!ucb_path_init_style(path, cstr, style))
    {
        ucb_path_free(path);
        return UCB_NULL;
    }
    return path;
}

ucb_path* ucb_path_clone(const ucb_path* src)
{
    UCB_VERIFY_ARGS(src);

    ucb_path* dst = ucb_malloc_type(1, ucb_path);
    if (!dst)
        return UCB_NULL;
    *dst = ucb_path_make();
    if (!ucb_path_copy(dst, src))
    {
        ucb_path_free(dst);
        return UCB_NULL;
    }
    return dst;
}

bool ucb_path_init(ucb_path* path, const char* cstr, size_t len)
{
    return path_init_ex(path, cstr, len, UCB_PATH_STYLE_DEFAULT);
}

bool ucb_path_init_style(ucb_path* path, const char* cstr, ucb_path_style style)
{
    return path_init_ex(path, cstr, 0, style);
}

/* -------------------------------------------------------------------------- */
/*                                 Destruction                                */
/* -------------------------------------------------------------------------- */

void ucb_path_release(ucb_path* path)
{
    UCB_VERIFY_ARGS(path);

    ucb_str_release(&path->dir);
    ucb_str_release(&path->stem);
    ucb_str_release(&path->ext);
    ucb_str_release(&path->_name);
    ucb_str_release(&path->_cache);
    memset(path, 0, sizeof(ucb_path));
}

void ucb_path_free(ucb_path* path)
{
    UCB_VERIFY_ARGS(path);

    ucb_str_release(&path->dir);
    ucb_str_release(&path->stem);
    ucb_str_release(&path->ext);
    ucb_str_release(&path->_name);
    ucb_str_release(&path->_cache);
    ucb_free(path);
}

/* -------------------------------------------------------------------------- */
/*                                   Assign                                   */
/* -------------------------------------------------------------------------- */

bool ucb_path_copy(ucb_path* dst, const ucb_path* src)
{
    UCB_VERIFY_ARGS(dst && src);

    if (dst == src)
        return true;

    bool ok = ucb_str_copy(&dst->dir, &src->dir);
    ok = ucb_str_copy(&dst->stem, &src->stem) && ok;
    ok = ucb_str_copy(&dst->ext, &src->ext) && ok;
    dst->style = src->style;
    dst->_dirty = true;
    return ok;
}

bool ucb_path_assign(ucb_path* path, const char* cstr, size_t len)
{
    UCB_VERIFY_ARGS(path);

    if (cstr)
    {
        if (len == 0)
            len = strlen(cstr);
        else
            path_check_no_nul(cstr, len);
    }

    if (path_aliases(path, cstr))
    {
        char* tmp = UCB_NULL;
        if (len)
        {
            tmp = ucb_malloc(len + 1);
            if (!tmp)
                return false;
            memcpy(tmp, cstr, len);
            tmp[len] = '\0';
        }
        bool ok = path_parse(path, tmp, len, path->style);
        ucb_free(tmp);
        return ok;
    }
    return path_parse(path, cstr, len, path->style);
}

void ucb_path_clear(ucb_path* path)
{
    UCB_VERIFY_ARGS(path);

    ucb_str_clear(&path->dir);
    ucb_str_clear(&path->stem);
    ucb_str_clear(&path->ext);
    ucb_str_clear(&path->_name);
    ucb_str_clear(&path->_cache);
    path->_dirty = true;
}

/* -------------------------------------------------------------------------- */
/*                            Querying / composing                            */
/* -------------------------------------------------------------------------- */

const char* ucb_path_cstr(ucb_path* path)
{
    UCB_VERIFY_ARGS(path);
    path_update(path);
    return path->_cache.data;
}

ucb_str* ucb_path_to_str(const ucb_path* path)
{
    UCB_VERIFY_ARGS(path);

    ucb_str* out = ucb_str_new_empty();
    if (!out)
        return UCB_NULL;
    if (!path_compose_to(path, out))
    {
        ucb_str_free(out);
        return UCB_NULL;
    }
    return out;
}

size_t ucb_path_len(const ucb_path* path)
{
    UCB_VERIFY_ARGS(path);
    return path_length(path);
}

bool ucb_path_is_empty(const ucb_path* path)
{
    UCB_VERIFY_ARGS(path);
    return path->dir.size == 0 && path->stem.size == 0 && path->ext.size == 0;
}

bool ucb_path_is_absolute(const ucb_path* path)
{
    UCB_VERIFY_ARGS(path);

    const char* d = path->dir.data;
    size_t n = path->dir.size;
    if (n == 0)
        return false;
    char sep = path_sep_of(path->style);
#ifdef _WIN32
    if (path_sep_match(d[0], sep))
        return true;
    if (n >= 2 && d[1] == ':' && isalpha((unsigned char)d[0]))
        return true;
    return false;
#else
    return path_sep_match(d[0], sep);
#endif
}

bool ucb_path_is_relative(const ucb_path* path)
{
    return !ucb_path_is_absolute(path);
}

void ucb_path_set_style(ucb_path* path, ucb_path_style style)
{
    UCB_VERIFY_ARGS(path);

    ucb_path_style ns = path_resolve_style(style);
    char old_sep = path_sep_of(path->style);
    char new_sep = path_sep_of(ns);

    if (old_sep != new_sep && path->dir.size)
    {
        for (size_t i = 0; i < path->dir.size; i++)
        {
            if (path_sep_match(path->dir.data[i], old_sep))
                path->dir.data[i] = new_sep;
        }
    }

    path->style = ns;
    path->_dirty = true;
}

ucb_path_style ucb_path_get_style(const ucb_path* path)
{
    UCB_VERIFY_ARGS(path);
    return path->style;
}

char ucb_path_sep(void)
{
    return path_sep_of(UCB_PATH_STYLE_DEFAULT);
}

const char* ucb_path_sep_cstr(void)
{
    return ucb_path_sep() == '/' ? "/" : "\\";
}

const ucb_str* ucb_path_dir(const ucb_path* path)
{
    UCB_VERIFY_ARGS(path);
    return &path->dir;
}

const ucb_str* ucb_path_stem(const ucb_path* path)
{
    UCB_VERIFY_ARGS(path);
    return &path->stem;
}

const ucb_str* ucb_path_ext(const ucb_path* path)
{
    UCB_VERIFY_ARGS(path);
    return &path->ext;
}

const ucb_str* ucb_path_filename(ucb_path* path)
{
    UCB_VERIFY_ARGS(path);
    path_update(path);
    return &path->_name;
}

ucb_str* ucb_path_dir_cstyle(const char* cstr, ucb_path_style style)
{
    ucb_path path = ucb_path_make();
    if (!path_init_ex(&path, cstr, 0, style))
    {
        ucb_path_release(&path);
        return UCB_NULL;
    }
    ucb_str* result = ucb_str_clone(&path.dir);
    ucb_path_release(&path);
    return result;
}

ucb_str* ucb_path_dir_c(const char* cstr)
{
    return ucb_path_dir_cstyle(cstr, UCB_PATH_STYLE_DEFAULT);
}

ucb_str* ucb_path_stem_c(const char* cstr)
{
    ucb_path path = ucb_path_make();
    if (!path_init_ex(&path, cstr, 0, UCB_PATH_STYLE_DEFAULT))
    {
        ucb_path_release(&path);
        return UCB_NULL;
    }
    ucb_str* result = ucb_str_clone(&path.stem);
    ucb_path_release(&path);
    return result;
}

ucb_str* ucb_path_ext_c(const char* cstr)
{
    ucb_path path = ucb_path_make();
    if (!path_init_ex(&path, cstr, 0, UCB_PATH_STYLE_DEFAULT))
    {
        ucb_path_release(&path);
        return UCB_NULL;
    }
    ucb_str* result = ucb_str_clone(&path.ext);
    ucb_path_release(&path);
    return result;
}

ucb_str* ucb_path_filename_c(const char* cstr)
{
    ucb_path path = ucb_path_make();
    if (!path_init_ex(&path, cstr, 0, UCB_PATH_STYLE_DEFAULT))
    {
        ucb_path_release(&path);
        return UCB_NULL;
    }
    ucb_str* result = ucb_str_clone(ucb_path_filename(&path));
    ucb_path_release(&path);
    return result;
}

/* -------------------------------------------------------------------------- */
/*                                  Setters                                   */
/* -------------------------------------------------------------------------- */

bool ucb_path_set_dir(ucb_path* path, const ucb_str* dir)
{
    UCB_VERIFY_ARGS(path && dir);

    ucb_str tmp = ucb_str_make();
    bool ok = ucb_str_copy(&tmp, dir);
    if (ok)
        ok = path_build_dir(&path->dir, tmp.data, tmp.size, path_sep_of(path->style));
    ucb_str_release(&tmp);

    path->_dirty = true;
    return ok;
}

bool ucb_path_set_stem(ucb_path* path, const ucb_str* stem)
{
    UCB_VERIFY_ARGS(path && stem);

    path->_dirty = true;
    return ucb_str_copy(&path->stem, stem);
}

bool ucb_path_set_ext(ucb_path* path, const ucb_str* ext)
{
    UCB_VERIFY_ARGS(path && ext);

    const char* s = ext->data;
    size_t n = ext->size;
    size_t i = 0;
    while (i < n && s[i] == '.')
        i++;

    bool ok;
    if (i >= n)
    {
        ok = ucb_str_assign(&path->ext, UCB_NULL, 0);
    }
    else
    {
        ucb_str tmp = ucb_str_make();
        ok = path_reserve(&tmp, (n - i) + 1);
        if (ok)
        {
            tmp.data[0] = '.';
            memcpy(tmp.data + 1, s + i, n - i);
            tmp.size = (n - i) + 1;
            tmp.data[tmp.size] = '\0';
            ok = ucb_str_copy(&path->ext, &tmp);
        }
        ucb_str_release(&tmp);
    }

    path->_dirty = true;
    return ok;
}

bool ucb_path_set_filename(ucb_path* path, const ucb_str* name)
{
    UCB_VERIFY_ARGS(path && name);

    ucb_str tmp = ucb_str_make();
    bool ok = ucb_str_copy(&tmp, name);
    if (ok)
        ok = path_parse_filename(path, tmp.data, tmp.size);
    ucb_str_release(&tmp);

    path->_dirty = true;
    return ok;
}

bool ucb_path_replace_ext(ucb_path* path, const ucb_str* ext)
{
    return ucb_path_set_ext(path, ext);
}

/* -------------------------------------------------------------------------- */
/*                                 Operations                                 */
/* -------------------------------------------------------------------------- */

bool ucb_path_append(ucb_path* path, const ucb_path* other)
{
    UCB_VERIFY_ARGS(path && other);

    if (path == other)
    {
        ucb_path* copy = ucb_path_clone(other);
        if (!copy)
            return false;
        bool ok = ucb_path_append(path, copy);
        ucb_path_free(copy);
        return ok;
    }

    if (ucb_path_is_absolute(other))
        return ucb_path_copy(path, other);

    char sep = path_sep_of(path->style);
    bool ok = true;

    // The base filename becomes a directory segment when joining.
    if (path->stem.size || path->ext.size)
    {
        bool need_sep = path->dir.size > 0 && path->dir.data[path->dir.size - 1] != sep;
        if (!path_reserve(&path->dir, (need_sep ? 1u : 0u) + path->stem.size + path->ext.size))
            return false;

        size_t off = path->dir.size;
        if (need_sep)
            path->dir.data[off++] = sep;
        if (path->stem.size)
        {
            memcpy(path->dir.data + off, path->stem.data, path->stem.size);
            off += path->stem.size;
        }
        if (path->ext.size)
        {
            memcpy(path->dir.data + off, path->ext.data, path->ext.size);
            off += path->ext.size;
        }
        path->dir.data[off] = '\0';
        path->dir.size = off;
    }

    if (other->dir.size)
        ok = path_append_segment(&path->dir, other->dir.data, other->dir.size, sep);

    ok = ucb_str_copy(&path->stem, &other->stem) && ok;
    ok = ucb_str_copy(&path->ext, &other->ext) && ok;

    path->_dirty = true;
    return ok;
}

ucb_path* ucb_path_join(const ucb_path* base, const ucb_path* other)
{
    UCB_VERIFY_ARGS(base && other);

    ucb_path* result = ucb_path_clone(base);
    if (!result)
        return UCB_NULL;
    if (!ucb_path_append(result, other))
    {
        ucb_path_free(result);
        return UCB_NULL;
    }
    return result;
}

ucb_path* ucb_path_join_c(const ucb_path* base, const char* other)
{
    UCB_VERIFY_ARGS(base);

    ucb_path tmp = ucb_path_make();
    if (!path_init_ex(&tmp, other, 0, base->style))
    {
        ucb_path_release(&tmp);
        return UCB_NULL;
    }
    ucb_path* result = ucb_path_join(base, &tmp);
    ucb_path_release(&tmp);
    return result;
}

bool ucb_path_append_c(ucb_path* path, const char* other)
{
    UCB_VERIFY_ARGS(path);

    ucb_path tmp = ucb_path_make();
    if (!path_init_ex(&tmp, other, 0, path->style))
    {
        ucb_path_release(&tmp);
        return false;
    }
    bool ok = ucb_path_append(path, &tmp);
    ucb_path_release(&tmp);
    return ok;
}

bool ucb_path_normalize(ucb_path* path)
{
    UCB_VERIFY_ARGS(path);

    char sep = path_sep_of(path->style);
    ucb_str cur = ucb_str_make();
    bool ok = path_compose_to(path, &cur);
    if (ok)
    {
        ucb_str norm = ucb_str_make();
        ok = path_normalize_lexical(&norm, cur.data, cur.size, sep);
        if (ok)
            ok = path_parse(path, norm.data, norm.size, path->style);
        ucb_str_release(&norm);
    }
    ucb_str_release(&cur);
    path->_dirty = true;
    return ok;
}

ucb_str* ucb_path_normalize_cstyle(const char* cstr, ucb_path_style style)
{
    ucb_path path = ucb_path_make();
    if (!path_init_ex(&path, cstr, 0, style))
    {
        ucb_path_release(&path);
        return UCB_NULL;
    }
    if (!ucb_path_normalize(&path))
    {
        ucb_path_release(&path);
        return UCB_NULL;
    }
    ucb_str* result = ucb_path_to_str(&path);
    ucb_path_release(&path);
    return result;
}

ucb_str* ucb_path_normalize_c(const char* cstr)
{
    return ucb_path_normalize_cstyle(cstr, UCB_PATH_STYLE_DEFAULT);
}

bool ucb_path_equals(const ucb_path* a, const ucb_path* b)
{
    UCB_VERIFY_ARGS(a && b);

    ucb_str sa = ucb_str_make();
    ucb_str sb = ucb_str_make();
    bool ok = path_compose_to(a, &sa);
    if (ok)
        ok = path_compose_to(b, &sb);
    bool result = ok && ucb_str_equal(&sa, &sb);
    ucb_str_release(&sa);
    ucb_str_release(&sb);
    return result;
}

int ucb_path_comp(const ucb_path* a, const ucb_path* b)
{
    UCB_VERIFY_ARGS(a && b);

    ucb_str sa = ucb_str_make();
    ucb_str sb = ucb_str_make();
    bool ok = path_compose_to(a, &sa);
    if (ok)
        ok = path_compose_to(b, &sb);
    int result = ok ? ucb_str_comp(&sa, &sb) : 0;
    ucb_str_release(&sa);
    ucb_str_release(&sb);
    return result;
}

int ucb_path_icomp(const ucb_path* a, const ucb_path* b)
{
    UCB_VERIFY_ARGS(a && b);

    ucb_str sa = ucb_str_make();
    ucb_str sb = ucb_str_make();
    bool ok = path_compose_to(a, &sa);
    if (ok)
        ok = path_compose_to(b, &sb);
    int result = ok ? ucb_str_icomp(&sa, &sb) : 0;
    ucb_str_release(&sa);
    ucb_str_release(&sb);
    return result;
}

bool ucb_path_equals_c(const char* a, const char* b)
{
    ucb_path pa = ucb_path_make();
    ucb_path pb = ucb_path_make();
    bool ok = path_init_ex(&pa, a, 0, UCB_PATH_STYLE_DEFAULT);
    if (ok)
        ok = path_init_ex(&pb, b, 0, UCB_PATH_STYLE_DEFAULT);
    bool result = ok && ucb_path_equals(&pa, &pb);
    ucb_path_release(&pa);
    ucb_path_release(&pb);
    return result;
}

/**
 * Internal. True if @p inner is a proper descendant of @p outer. Compares the
 * part sequences; equal or shorter paths are not inside.
 */
static bool path_inside_one(const ucb_path* inner, const ucb_path* outer)
{
    size_t ni = ucb_path_num_parts(inner);
    size_t no = ucb_path_num_parts(outer);
    if (ni == 0 || no == 0 || ni <= no)
        return false;

    for (size_t i = 0; i < no; i++)
    {
        ucb_str* pi = ucb_path_part(inner, i);
        ucb_str* po = ucb_path_part(outer, i);
        bool same = pi && po && ucb_str_equal(pi, po);
        ucb_str_free(pi);
        ucb_str_free(po);
        if (!same)
            return false;
    }
    return true;
}

bool ucb_path_is_inside(const ucb_path* a, const ucb_path* b)
{
    UCB_VERIFY_ARGS(a && b);

    return path_inside_one(a, b) || path_inside_one(b, a);
}

int ucb_path_comp_c(const char* a, const char* b)
{
    ucb_path pa = ucb_path_make();
    ucb_path pb = ucb_path_make();
    bool ok = path_init_ex(&pa, a, 0, UCB_PATH_STYLE_DEFAULT);
    if (ok)
        ok = path_init_ex(&pb, b, 0, UCB_PATH_STYLE_DEFAULT);
    int result = ok ? ucb_path_comp(&pa, &pb) : 0;
    ucb_path_release(&pa);
    ucb_path_release(&pb);
    return result;
}

int ucb_path_icomp_c(const char* a, const char* b)
{
    ucb_path pa = ucb_path_make();
    ucb_path pb = ucb_path_make();
    bool ok = path_init_ex(&pa, a, 0, UCB_PATH_STYLE_DEFAULT);
    if (ok)
        ok = path_init_ex(&pb, b, 0, UCB_PATH_STYLE_DEFAULT);
    int result = ok ? ucb_path_icomp(&pa, &pb) : 0;
    ucb_path_release(&pa);
    ucb_path_release(&pb);
    return result;
}

/* -------------------------------------------------------------------------- */
/*                              Parts and parents                             */
/* -------------------------------------------------------------------------- */

size_t ucb_path_num_parts(const ucb_path* path)
{
    UCB_VERIFY_ARGS(path);

    char sep = path_sep_of(path->style);
    const char* d = path->dir.data;
    size_t n = path->dir.size;
    size_t anchor_len = 0;
    path_anchor_kind ak = path_detect_anchor(d, n, sep, &anchor_len);

    size_t count = 0;
    if (ak != PATH_ANCHOR_NONE)
        count++;

    size_t i = anchor_len;
    while (i < n)
    {
        while (i < n && path_sep_match(d[i], sep))
            i++;
        if (i >= n)
            break;
        while (i < n && !path_sep_match(d[i], sep))
            i++;
        count++;
    }

    if (path->stem.size || path->ext.size)
        count++;
    return count;
}

ucb_str* ucb_path_part(const ucb_path* path, size_t index)
{
    UCB_VERIFY_ARGS(path);

    char sep = path_sep_of(path->style);
    const char* d = path->dir.data;
    size_t n = path->dir.size;
    size_t anchor_len = 0;
    path_anchor_kind ak = path_detect_anchor(d, n, sep, &anchor_len);

    size_t pos = 0;
    if (ak != PATH_ANCHOR_NONE)
    {
        if (index == pos)
        {
            ucb_str* result = ucb_str_new_empty();
            if (result && !path_build_anchor(d, ak, anchor_len, sep, result))
            {
                ucb_str_free(result);
                result = UCB_NULL;
            }
            return result;
        }
        pos++;
    }

    size_t i = anchor_len;
    while (i < n)
    {
        while (i < n && path_sep_match(d[i], sep))
            i++;
        if (i >= n)
            break;
        size_t s = i;
        while (i < n && !path_sep_match(d[i], sep))
            i++;
        if (index == pos)
            return ucb_str_new(d + s, i - s);
        pos++;
    }

    if (path->stem.size || path->ext.size)
    {
        if (index == pos)
        {
            ucb_str* result = ucb_str_new_empty();
            if (!result)
                return UCB_NULL;
            bool ok = path_append_cstr(result, path->stem.data, path->stem.size);
            if (ok)
                ok = path_append_cstr(result, path->ext.data, path->ext.size);
            if (!ok)
            {
                ucb_str_free(result);
                return UCB_NULL;
            }
            return result;
        }
        pos++;
    }

    UCB_REPORT(UCB_ERROR_OUT_OF_BOUNDS, "Part index %zu out of range", index);
}

ucb_vector_str* ucb_path_parts(const ucb_path* path)
{
    UCB_VERIFY_ARGS(path);

    ucb_vector_str* vec = ucb_vector_str_new();
    if (!vec)
        return UCB_NULL;

    size_t n = ucb_path_num_parts(path);
    for (size_t i = 0; i < n; i++)
    {
        ucb_str* part = ucb_path_part(path, i);
        if (!part)
        {
            ucb_vector_str_free_full(vec);
            return UCB_NULL;
        }
        ucb_vector_str_push_back(vec, part);
    }
    return vec;
}

ucb_vector_str* ucb_path_parts_cstyle(const char* cstr, ucb_path_style style)
{
    ucb_path path = ucb_path_make();
    if (!path_init_ex(&path, cstr, 0, style))
    {
        ucb_path_release(&path);
        return UCB_NULL;
    }
    ucb_vector_str* result = ucb_path_parts(&path);
    ucb_path_release(&path);
    return result;
}

ucb_vector_str* ucb_path_parts_c(const char* cstr)
{
    return ucb_path_parts_cstyle(cstr, UCB_PATH_STYLE_DEFAULT);
}

ucb_path* ucb_path_parent(const ucb_path* path)
{
    UCB_VERIFY_ARGS(path);

    ucb_path* result = ucb_path_clone(path);
    if (!result)
        return UCB_NULL;

    if (result->stem.size || result->ext.size)
    {
        ucb_str_clear(&result->stem);
        ucb_str_clear(&result->ext);
        result->_dirty = true;
        return result;
    }

    char sep = path_sep_of(result->style);
    const char* d = result->dir.data;
    size_t n = result->dir.size;
    size_t anchor_len = 0;
    path_anchor_kind ak = path_detect_anchor(d, n, sep, &anchor_len);

    size_t cut = UCB_NPOS;
    for (size_t i = n; i-- > anchor_len;)
    {
        if (path_sep_match(d[i], sep))
        {
            cut = i;
            break;
        }
    }

    if (cut == UCB_NPOS || cut <= anchor_len)
    {
        if (ak != PATH_ANCHOR_NONE)
        {
            ucb_str tmp = ucb_str_make();
            bool ok = path_build_anchor(d, ak, anchor_len, sep, &tmp);
            if (ok)
                ok = ucb_str_copy(&result->dir, &tmp);
            ucb_str_release(&tmp);
            if (!ok)
            {
                ucb_path_free(result);
                return UCB_NULL;
            }
        }
        else
        {
            ucb_str_clear(&result->dir);
        }
    }
    else
    {
        result->dir.size = cut;
        result->dir.data[cut] = '\0';
    }

    result->_dirty = true;
    return result;
}

ucb_str* ucb_path_parent_cstyle(const char* cstr, ucb_path_style style)
{
    ucb_path path = ucb_path_make();
    if (!path_init_ex(&path, cstr, 0, style))
    {
        ucb_path_release(&path);
        return UCB_NULL;
    }
    ucb_path* parent = ucb_path_parent(&path);
    ucb_path_release(&path);
    if (!parent)
        return UCB_NULL;
    ucb_str* result = ucb_path_to_str(parent);
    ucb_path_free(parent);
    return result;
}

ucb_str* ucb_path_parent_c(const char* cstr)
{
    return ucb_path_parent_cstyle(cstr, UCB_PATH_STYLE_DEFAULT);
}

ucb_vector_str* ucb_path_parents(const ucb_path* path)
{
    UCB_VERIFY_ARGS(path);

    ucb_vector_str* vec = ucb_vector_str_new();
    if (!vec)
        return UCB_NULL;

    ucb_path* cur = ucb_path_clone(path);
    if (!cur)
    {
        ucb_vector_str_free(vec);
        return UCB_NULL;
    }

    while (!ucb_path_is_empty(cur))
    {
        ucb_path* parent = ucb_path_parent(cur);
        if (!parent)
        {
            ucb_vector_str_free_full(vec);
            ucb_path_free(cur);
            return UCB_NULL;
        }

        if (ucb_path_is_empty(parent) || ucb_path_equals(cur, parent))
        {
            ucb_path_free(parent);
            break;
        }

        ucb_str* str = ucb_path_to_str(parent);
        if (!str)
        {
            ucb_path_free(parent);
            ucb_vector_str_free_full(vec);
            ucb_path_free(cur);
            return UCB_NULL;
        }
        ucb_vector_str_push_back(vec, str);

        ucb_path_free(cur);
        cur = parent;
    }

    ucb_path_free(cur);
    return vec;
}

ucb_vector_str* ucb_path_parents_cstyle(const char* cstr, ucb_path_style style)
{
    ucb_path path = ucb_path_make();
    if (!path_init_ex(&path, cstr, 0, style))
    {
        ucb_path_release(&path);
        return UCB_NULL;
    }
    ucb_vector_str* result = ucb_path_parents(&path);
    ucb_path_release(&path);
    return result;
}

ucb_vector_str* ucb_path_parents_c(const char* cstr)
{
    return ucb_path_parents_cstyle(cstr, UCB_PATH_STYLE_DEFAULT);
}
