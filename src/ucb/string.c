/**
 * @file string.c
 *
 * This file is part of the UCB project
 * - SPDX-FileCopyrightText: © 2026 Åke Svedin <ake@svedin.org>
 * - SPDX-License-Identifier: MIT
 *
 * @brief String type and related functions implementation
 */

#include "ucb/string.h"

#include "ucb/cstring.h"
#include "ucb/debug.h"
#include "ucb/defines.h"
#include "ucb/errcodes.h"
#include "ucb/error.h"
#include "ucb/memory.h"
#include "ucb/unicode.h"

#include <limits.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

/**
 * Internal helper. Returns true if @p ptr points into the owned allocation of
 * @p str. Used to make mutating operations safe when the source aliases the
 * destination (e.g. self-append or self-assign).
 */
static inline bool ucb_str_aliases(const ucb_str* str, const char* ptr)
{
    return str->alloc > 0 && ptr != UCB_NULL && ptr >= str->data && ptr < str->data + str->alloc;
}

/**
 * Internal helper. Reports a user error (which aborts) when an explicit-length
 * string contains an embedded null character. ucb_str never carries interior
 * nulls, so this is always misuse.
 */
static void ucb_str_check_no_nul(const char* cstr, size_t len)
{
    if (len && memchr(cstr, '\0', len) != UCB_NULL)
        UCB_REPORT(UCB_ERROR_INVALID_ARG, "Embedded null character");
}

/* -------------------------------------------------------------------------- */
/*                                Construction                                */
/* -------------------------------------------------------------------------- */

/**
 * Internal function.
 * str must have been verified (non-null)
 *
 * If cstr is UCB_NULL, str is set to the interned empty string. Otherwise an
 * owned allocation is made. When len is non-zero it must not contain an
 * embedded null character.
 */
static bool ucb_str_init_common(ucb_str* str, const char* cstr, size_t len)
{
    if (!cstr)
    {
        // Interned empty string: no allocation, never freed
        str->data = (char*)"";
        str->size = 0;
        str->alloc = 0;
        return true;
    }

    if (len == 0)
    {
        // If the string is ment to be larger than size_t then
        // then I'll eat my hat
        len = strlen(cstr);
    }
    else
    {
        ucb_str_check_no_nul(cstr, len);
    }

    str->data = ucb_malloc(len + 1);
    if (str->data)
    {
        memcpy(str->data, cstr, len);
        str->data[len] = '\0';

        str->size = len;
        str->alloc = len + 1;
    }
    return str->data != UCB_NULL;
}

/**
 * str must have been verified (non-null)
 */
static inline void ucb_str_release_common(ucb_str* str)
{
    if (str->alloc)
        ucb_free(str->data);
}

ucb_str* ucb_str_new(const char* cstr, size_t len)
{
    ucb_str* str = ucb_malloc_type(1, ucb_str);
    if (str && !ucb_str_init_common(str, cstr, len))
    {
        ucb_free(str);
        str = UCB_NULL;
    }
    return str;
}

ucb_str* ucb_str_clone(const ucb_str* src)
{
    UCB_VERIFY_ARGS(src);
    return ucb_str_new(src->data, src->size);
}

bool ucb_str_init(ucb_str* str, const char* cstr, size_t len)
{
    UCB_VERIFY_ARGS(str);
    return ucb_str_init_common(str, cstr, len);
}

/* -------------------------------------------------------------------------- */
/*                                 Destruction                                */
/* -------------------------------------------------------------------------- */

void ucb_str_release(ucb_str* str)
{
    UCB_VERIFY_ARGS(str);

    ucb_str_release_common(str);
    memset(str, 0, sizeof(ucb_str));
}

void ucb_str_free(ucb_str* str)
{
    UCB_VERIFY_ARGS(str);

    ucb_str_release_common(str);
    ucb_free(str);
}

/* -------------------------------------------------------------------------- */
/*                                   Assign                                   */
/* -------------------------------------------------------------------------- */

bool ucb_str_copy(ucb_str* dst, const ucb_str* src)
{
    UCB_VERIFY_ARGS(dst && src);

    // Copying onto itself is a no-op and must not release the source data.
    if (dst == src)
        return true;

    ucb_str_release_common(dst);
    return ucb_str_init_common(dst, src->data, src->size);
}

bool ucb_str_assign(ucb_str* str, const char* cstr, size_t len)
{
    UCB_VERIFY_ARGS(str);

    if (cstr && len)
        ucb_str_check_no_nul(cstr, len);

    // If the source points into the destination's own allocation, copy it out
    // before releasing the old data.
    if (ucb_str_aliases(str, cstr))
    {
        size_t real_len = len ? len : strlen(cstr);
        char* tmp = UCB_NULL;
        if (real_len)
        {
            tmp = ucb_malloc(real_len);
            if (!tmp)
                return false;
            memcpy(tmp, cstr, real_len);
        }

        ucb_str_release_common(str);
        bool ok = ucb_str_init_common(str, tmp, real_len);
        ucb_free(tmp);
        return ok;
    }

    ucb_str_release_common(str);
    return ucb_str_init_common(str, cstr, len);
}

bool ucb_str_fit(ucb_str* str)
{
    UCB_VERIFY_ARGS(str);

    bool modified = false;
    if (str->alloc)
    {
        UCB_ASSERT(str->size <= str->alloc,
                   UCB_ERROR_INVALID_STATE,
                   "Current string size exceeds allocation");

        if (str->alloc > str->size + 1)
        {
            char* data = ucb_realloc(str->data, str->size + 1);
            if (data)
            {
                str->data = data;
                str->data[str->size] = '\0';
                str->alloc = str->size + 1;
                modified = true;
            }
        }
    }
    return modified;
}

bool ucb_str_reserve(ucb_str* str, size_t size)
{
    UCB_VERIFY_ARGS(str);

    bool has_free = false;
    if (str->alloc)
    {
        if (size == 0)
        {
            has_free = true;
        }
        else
        {
            UCB_ASSERT(str->size <= str->alloc,
                       UCB_ERROR_INVALID_STATE,
                       "Current string size exceeds allocation");

            size_t new_size = size + str->size + 1;
            if (str->alloc < new_size)
            {
                char* data = ucb_realloc(str->data, new_size);
                if (data)
                {
                    str->data = data;
                    str->data[str->size] = '\0';
                    str->alloc = new_size;
                    has_free = true;
                }
            }
        }
    }
    else
    {
        // Interned empty string: allocate now that it needs storage
        char* data = ucb_malloc(str->size + size + 1);
        if (data)
        {
            memcpy(data, str->data, str->size);
            str->data = data;
            str->data[str->size] = '\0';
            str->alloc = str->size + size + 1;
            has_free = true;
        }
    }
    return has_free;
}

void ucb_str_adopt(ucb_str* str, char* data, size_t len, size_t alloc)
{
    UCB_VERIFY_ARGS(str && data);

    if (!len)
        len = strlen(data);
    else
        ucb_str_check_no_nul(data, len);

    UCB_VERIFY(alloc >= len + 1, UCB_ERROR_INVALID_ARG, "alloc must be at least len + 1");
    UCB_VERIFY(data[len] == '\0',
               UCB_ERROR_INVALID_ARG,
               "data must be null-terminated at data[len]");

    ucb_str_release_common(str);

    str->data = data;
    str->size = len;
    str->alloc = alloc;
}

void ucb_str_adopt_c(ucb_str* str, char* cstr)
{
    UCB_VERIFY_ARGS(str && cstr);

    ucb_str_release_common(str);

    str->data = cstr;
    str->size = strlen(cstr);
    str->alloc = str->size + 1;
}

bool ucb_str_abandon(ucb_str* str, char** data, size_t* len, size_t* alloc)
{
    UCB_VERIFY_ARGS(str && data);

    bool owned = str->alloc > 0;

    *data = str->data;
    if (len)
        *len = str->size;
    if (alloc)
        *alloc = str->alloc;

    memset(str, 0, sizeof(ucb_str));
    return owned;
}

/* -------------------------------------------------------------------------- */
/*                                 Conversion                                 */
/* -------------------------------------------------------------------------- */

// Implemented in string_win32.c

/* -------------------------------------------------------------------------- */
/*                                  Querying                                  */
/* -------------------------------------------------------------------------- */

bool ucb_str_is_empty(const ucb_str* str)
{
    UCB_VERIFY_ARGS(str);
    return str->size == 0;
}

size_t ucb_str_capacity(const ucb_str* str)
{
    UCB_VERIFY_ARGS(str);
    return str->alloc;
}

size_t ucb_str_used(const ucb_str* str)
{
    UCB_VERIFY_ARGS(str);
    size_t used = 0;
    if (str->alloc)
    {
        UCB_ASSERT(str->size <= str->alloc,
                   UCB_ERROR_INVALID_STATE,
                   "String size exceeds allocation");
        used = str->size;
        if (str->alloc >= used + 1 && str->data[used] == '\0')
            used++;
    }
    return used;
}

size_t ucb_str_avail(const ucb_str* str)
{
    UCB_VERIFY_ARGS(str);
    return str->alloc - ucb_str_used(str);
}

const char* ucb_str_cstr(const ucb_str* str)
{
    UCB_VERIFY_ARGS(str);
    return str->data;
}

size_t ucb_str_len(const ucb_str* str)
{
    UCB_VERIFY_ARGS(str);
    return str->size;
}

// size_t ucb_str_num_cp(const ucb_str* str)
// {
//     UCB_VERIFY_ARGS(str, 0);
//     return ucb_uc_num_cp(str->data, str->size);
// }

size_t ucb_str_num_char(const ucb_str* str)
{
    UCB_VERIFY_ARGS(str);
    return ucb_uc_num_char(str->data, str->size);
}

/* -------------------------------------------------------------------------- */
/*                            Comparison and lookup                           */
/* -------------------------------------------------------------------------- */

bool ucb_str_equal(const ucb_str* str1, const ucb_str* str2)
{
    if (str1->size != str2->size)
        return false;
    return memcmp(str1->data, str2->data, str1->size) == 0;
}

int ucb_str_comp(const ucb_str* str1, const ucb_str* str2)
{
    size_t min_len = (str1->size < str2->size) ? str1->size : str2->size;
    int cmp = memcmp(str1->data, str2->data, min_len);
    if (cmp != 0)
        return cmp;
    return (int)(str1->size - str2->size); // Longer string is "greater"
}

int ucb_str_icomp(const ucb_str* str1, const ucb_str* str2)
{
    return ucb_uc_icomp(str1->data, str1->size, str2->data, str2->size);
}

bool ucb_str_startswith(const ucb_str* str, const ucb_str* prefix)
{
    if (prefix->size > str->size)
        return false;
    return memcmp(str->data, prefix->data, prefix->size) == 0;
}

bool ucb_str_endswith(const ucb_str* str, const ucb_str* suffix)
{
    if (suffix->size > str->size)
        return false;
    return memcmp(str->data + (str->size - suffix->size), suffix->data, suffix->size) == 0;
}
size_t ucb_str_find(const ucb_str* str, const ucb_str* substr, size_t pos)
{
    if (substr->size == 0)
        return 0; // Empty substring
    if (pos > str->size || substr->size > str->size - pos)
        return UCB_NPOS;

    const char* end = str->data + (str->size - substr->size);
    for (const char* p = str->data + pos; p <= end; p++)
    {
        if (memcmp(p, substr->data, substr->size) == 0)
            return (size_t)(p - str->data);
    }
    return UCB_NPOS;
}

size_t ucb_str_next_char(const ucb_str* str, size_t from_byte)
{
    UCB_VERIFY_ARGS(str);
    return ucb_uc_next_char(str->data, str->size, from_byte);
}

/* -------------------------------------------------------------------------- */
/*                                Modification                                */
/* -------------------------------------------------------------------------- */

void ucb_str_clear(ucb_str* str)
{
    UCB_VERIFY_ARGS(str);
    ucb_str_release_common(str);

    str->data = "";
    str->size = 0;
    str->alloc = 0;
}

void ucb_str_append(ucb_str* str, const ucb_str* astr)
{
    UCB_VERIFY_ARGS(str && astr);
    ucb_str_append_cstr(str, astr->data, astr->size);
}

void ucb_str_append_cp(ucb_str* str, const ucb_cp* cp, size_t num_cp, ucb_error** perr)
{
    UCB_VERIFY_ARGS(str && cp);

    for (size_t i = 0; i < num_cp; i++)
    {
        if (cp[i] == 0)
            UCB_REPORT(UCB_ERROR_INVALID_ARG, "Zero codepoint is not allowed");
    }

    size_t max_size = 4 * num_cp;
    if (ucb_str_reserve(str, max_size))
    {
        ucb_buffer buffer;
        ucb_buffer_init_static(&buffer, str->data + str->size, max_size);
        if (ucb_uc_encode_codepoints(&buffer, cp, num_cp, perr))
        {
            str->size += buffer.size;
            str->data[str->size] = '\0';
        }
        ucb_buffer_release(&buffer);
    }
}

void ucb_str_append_cstr(ucb_str* str, const char* cstr, size_t len)
{
    UCB_VERIFY_ARGS(str && (cstr || !len));

    if (!cstr)
        return;

    if (len == 0)
    {
        len = strlen(cstr);
        if (!len)
            return;
    }
    else
    {
        ucb_str_check_no_nul(cstr, len);
    }

    // Appending a slice of the string to itself: copy it out first so the
    // reserve/realloc below cannot invalidate the source.
    if (ucb_str_aliases(str, cstr))
    {
        char* tmp = ucb_malloc(len);
        if (!tmp)
            return;
        memcpy(tmp, cstr, len);
        if (ucb_str_reserve(str, len))
        {
            memcpy(str->data + str->size, tmp, len);
            str->size += len;
            str->data[str->size] = '\0';
        }
        ucb_free(tmp);
        return;
    }

    if (ucb_str_reserve(str, len))
    {
        memcpy(str->data + str->size, cstr, len);
        str->size += len;
        str->data[str->size] = '\0';
    }
}

void ucb_str_insert(ucb_str* str, size_t pos, const ucb_str* istr)
{
    UCB_VERIFY_ARGS(str && istr);

    ucb_str_insert_cstr(str, pos, istr->data, istr->size);
}

void ucb_str_insert_cp(ucb_str* str,
                       size_t index,
                       const ucb_cp* cp,
                       size_t num_cp,
                       ucb_error** perr)
{
    UCB_VERIFY_ARGS(str && (cp || !num_cp));

    for (size_t i = 0; i < num_cp; i++)
    {
        if (cp[i] == 0)
            UCB_REPORT(UCB_ERROR_INVALID_ARG, "Zero codepoint is not allowed");
    }

    if (num_cp)
    {
        size_t max_size = 4 * num_cp;
        ucb_buffer buffer;

        ucb_buffer_init_heap(&buffer, max_size);
        if (ucb_uc_encode_codepoints(&buffer, cp, num_cp, perr))
        {
            ucb_str_insert_cstr(str, index, buffer.data, buffer.size);
        }
        ucb_buffer_release(&buffer);
    }
}

void ucb_str_insert_cstr(ucb_str* str, size_t index, const char* cstr, size_t len)
{
    UCB_VERIFY_ARGS(str && (cstr || !len));

    if (!cstr)
        return;

    if (!len)
    {
        len = strlen(cstr);
        if (!len)
            return;
    }
    else
    {
        ucb_str_check_no_nul(cstr, len);
    }

    if (index == str->size || index == UCB_NPOS)
    {
        ucb_str_append_cstr(str, cstr, len);
    }
    else
    {
        if (index > 0)
        {
            index = ucb_uc_char_index(str->data, str->size, index);
            UCB_VERIFY(index > 0 && index < str->size,
                       UCB_ERROR_INVALID_ARG,
                       "Invalid character position or invalid UTF-8");
        }

        // Inserting a slice of the string into itself: copy it out before the
        // reserve/memmove below can invalidate or shift the source.
        char* tmp = UCB_NULL;
        if (ucb_str_aliases(str, cstr))
        {
            tmp = ucb_malloc(len);
            if (!tmp)
                return;
            memcpy(tmp, cstr, len);
            cstr = tmp;
        }

        if (ucb_str_reserve(str, len))
        {
            memmove(str->data + index + len, str->data + index, str->size - index);
            memcpy(str->data + index, cstr, len);
            str->size += len;
            str->data[str->size] = '\0';
        }
        ucb_free(tmp);
    }
}

ucb_str* ucb_str_concatv(const ucb_str* str, va_list args)
{
    UCB_VERIFY_ARGS(str);

    ucb_str* next;
    size_t size = str->size;

    va_list args_copy;
    va_copy(args_copy, args);
    next = va_arg(args_copy, ucb_str*);
    while (next)
    {
        size += next->size;
        next = va_arg(args_copy, ucb_str*);
    }

    if (!size)
        return ucb_str_new_empty();

    ucb_str* dst = ucb_malloc_type(1, ucb_str);
    if (!dst)
        return UCB_NULL;

    dst->data = ucb_malloc(size + 1);
    if (!dst->data)
    {
        ucb_free(dst);
        return UCB_NULL;
    }

    dst->size = size;
    dst->alloc = size + 1;

    memcpy(dst->data, str->data, str->size);

    size_t offset = str->size;
    next = va_arg(args, ucb_str*);
    while (next)
    {
        memcpy(dst->data + offset, next->data, next->size);
        offset += next->size;
        next = va_arg(args, ucb_str*);
    }
    dst->data[dst->size] = '\0';
    return dst;
}

ucb_str* ucb_str_concat(const ucb_str* str, ...)
{
    va_list args;
    va_start(args, str);

    ucb_str* result = ucb_str_concatv(str, args);

    va_end(args);

    return result;
}

ucb_str* ucb_str_substr(const ucb_str* str, size_t start, size_t end)
{
    UCB_VERIFY_ARGS(str && start <= end && (end <= str->size || end == UCB_NPOS));

    if (end == UCB_NPOS)
        end = str->size;

    ucb_str* dst;
    if (start > end)
    {
        dst = ucb_str_new_empty();
    }
    else
    {
        dst = ucb_str_new(str->data + start, end - start);
    }
    return dst;
}

bool ucb_str_to_lower(ucb_str* str)
{
    UCB_VERIFY_ARGS(str);

    ucb_error* err = UCB_NULL;
    ucb_uc_result res = ucb_uc_to_lower(str->data, str->size, &err);
    UCB_VERIFY_ERROR(res.data, err);
    ucb_str_adopt(str, res.data, res.size, res.size + 1);
    return res.data != UCB_NULL;
}

bool ucb_str_to_upper(ucb_str* str)
{
    UCB_VERIFY_ARGS(str);

    ucb_error* err = UCB_NULL;
    ucb_uc_result res = ucb_uc_to_upper(str->data, str->size, &err);
    UCB_VERIFY_ERROR(res.data, err);
    ucb_str_adopt(str, res.data, res.size, res.size + 1);
    return res.data != UCB_NULL;
}

bool ucb_str_to_title(ucb_str* str)
{
    UCB_VERIFY_ARGS(str);

    ucb_error* err = UCB_NULL;
    ucb_uc_result res = ucb_uc_to_title(str->data, str->size, &err);
    UCB_VERIFY_ERROR(res.data, err);
    ucb_str_adopt(str, res.data, res.size, res.size + 1);
    return res.data != UCB_NULL;
}

bool ucb_str_casefold(ucb_str* str)
{
    UCB_VERIFY_ARGS(str);

    ucb_error* err = UCB_NULL;
    ucb_uc_result res = ucb_uc_casefold(str->data, str->size, &err);
    UCB_VERIFY_ERROR(res.data, err);
    ucb_str_adopt(str, res.data, res.size, res.size + 1);
    return res.data != UCB_NULL;
}

bool ucb_str_normalize(ucb_str* str, ucb_norm_form form)
{
    UCB_VERIFY_ARGS(str && form >= UCB_NORM_NFD && form <= UCB_NORM_NFKC);

    ucb_error* err = UCB_NULL;
    ucb_uc_result res = ucb_uc_normalize(str->data, str->size, form, &err);
    UCB_VERIFY_ERROR(res.data, err);
    ucb_str_adopt(str, res.data, res.size, res.size + 1);
    return res.data != UCB_NULL;
}
