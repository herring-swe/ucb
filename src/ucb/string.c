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

#define STR_WRAP true
#define STR_OWN false

/* -------------------------------------------------------------------------- */
/*                                Construction                                */
/* -------------------------------------------------------------------------- */

/**
 * Internal function.
 * str must have been verified (non-null)
 * cstr is allowed to be NULL
 * len is allowed to be 0
 */
static bool ucb_str_init_common(ucb_str* str, bool wrap, const char* cstr, size_t len)
{
    if (!cstr)
    {
        cstr = "";
        len = 0;
    }
    else if (len == 0)
    {
        // If the string is ment to be larger than size_t then
        // then I'll eat my hat
        len = strlen(cstr);
    }

    if (wrap)
    {
        str->data = (char*)cstr;
        str->size = len;
        str->alloc = 0;
    }
    else
    {
        str->data = ucb_malloc(len + 1);
        if (str->data)
        {
            memcpy(str->data, cstr, len);
            str->data[str->size] = '\0';

            str->size = len;
            str->alloc = len + 1;
        }
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
    if (str && !ucb_str_init_common(str, STR_OWN, cstr, len))
    {
        ucb_free(str);
        str = UCB_NULL;
    }
    return str;
}

ucb_str* ucb_str_new_wrap(const char* cstr, size_t len)
{
    ucb_str* str = ucb_malloc_type(1, ucb_str);
    if (str && !ucb_str_init_common(str, STR_WRAP, cstr, len))
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
    return ucb_str_init_common(str, STR_OWN, cstr, len);
}

void ucb_str_init_wrap(ucb_str* str, const char* cstr, size_t len)
{
    UCB_VERIFY_ARGS(str);
    ucb_str_init_common(str, STR_WRAP, cstr, len);
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
    UCB_VERIFY_ARGS(dst);
    ucb_str_release_common(dst);
    return ucb_str_init_common(dst, STR_OWN, src->data, src->size);
}

bool ucb_str_assign(ucb_str* str, const char* cstr, size_t len)
{
    UCB_VERIFY_ARGS(str);
    ucb_str_release_common(str);
    return ucb_str_init_common(str, STR_OWN, cstr, len);
}

bool ucb_str_detach(ucb_str* str)
{
    UCB_VERIFY_ARGS(str);

    bool modified = false;
    if (str->alloc == 0)
    {
        char* data = ucb_malloc(str->size + 1);
        if (data)
        {
            memcpy(data, str->data, str->size);
            data[str->size] = '\0';

            str->data = data;
            str->alloc = str->size + 1;
            modified = true;
        }
    }
    return modified;
}

bool ucb_str_fit(ucb_str* str)
{
    UCB_VERIFY_ARGS(str);

    bool modified = false;
    if (str->alloc)
    {
        UCB_ASSERT(str->size <= str->alloc, UCB_ERROR_INVALID_STATE,
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

bool ucb_str_ensure(ucb_str* str, size_t size)
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
            UCB_ASSERT(str->size <= str->alloc, UCB_ERROR_INVALID_STATE,
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
        // detach
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

void ucb_str_wrap(ucb_str* str, const char* cstr, size_t len)
{
    UCB_VERIFY_ARGS(str);
    ucb_str_release_common(str);
    ucb_str_init_common(str, STR_WRAP, cstr, len);
}

void ucb_str_adopt(ucb_str* str, char* data, size_t len, size_t alloc)
{
    UCB_VERIFY_ARGS(str && data);

    ucb_str_release_common(str);

    if (!len)
        len = strlen(data);
    if (!alloc)
        alloc = len;
    else
        UCB_VERIFY(alloc >= len, UCB_ERROR_INVALID_ARG, "alloc < len");

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

bool ucb_str_is_owned(const ucb_str* str)
{
    UCB_VERIFY_ARGS(str);
    return str->alloc > 0;
}

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
        UCB_ASSERT(str->size <= str->alloc, UCB_ERROR_INVALID_STATE,
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

size_t ucb_str_size(const ucb_str* str)
{
    UCB_VERIFY_ARGS(str);
    return str->size;
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

size_t ucb_str_num_chars(const ucb_str* str)
{
    UCB_VERIFY_ARGS(str);
    return ucb_uc_num_chars(str->data, str->size);
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
    if (pos + substr->size > str->size)
        return (size_t)-1;

    const char* end = str->data + (str->size - substr->size);
    for (const char* p = str->data + pos; p <= end; p++)
    {
        if (memcmp(p, substr->data, substr->size) == 0)
            return (size_t)(p - str->data);
    }
    return (size_t)-1;
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

void ucb_str_append(ucb_str* str, const ucb_str* append)
{
    UCB_VERIFY_ARGS(str && append);
    ucb_str_append_utf8(str, str->data, str->size);
}

void ucb_str_append_cp(ucb_str* str, const ucb_cp* cp, size_t num_cp, ucb_error** perr)
{
    UCB_VERIFY_ARGS(str && cp);

    size_t max_size = 4 * num_cp;
    if (ucb_str_ensure(str, max_size))
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

void ucb_str_append_utf8(ucb_str* str, const char* data, size_t size)
{
    UCB_VERIFY_ARGS(str && (data || !size));

    if (size == 0)
        return;

    if (ucb_str_ensure(str, size))
    {
        memcpy(str->data + str->size, data, size);
        str->size += size;
        str->data[str->size] = '\0';
    }
}

void ucb_str_insert(ucb_str* str, size_t pos, const ucb_str* istr)
{
    UCB_VERIFY_ARGS(str && istr);

    ucb_str_insert_utf8(str, pos, istr->data, istr->size);
}

void ucb_str_insert_cp(ucb_str* str,
                       size_t index,
                       const ucb_cp* cp,
                       size_t num_cp,
                       ucb_error** perr)
{
    UCB_VERIFY_ARGS(str && (cp || !num_cp));

    if (num_cp)
    {
        size_t max_size = 4 * num_cp;
        ucb_buffer buffer;

        ucb_buffer_init_heap(&buffer, max_size);
        if (ucb_uc_encode_codepoints(&buffer, cp, num_cp, perr))
        {
            ucb_str_insert_utf8(str, index, buffer.data, buffer.size);
        }
        ucb_buffer_release(&buffer);
    }
}

void ucb_str_insert_utf8(ucb_str* str, size_t index, const char* cstr, size_t len)
{
    UCB_VERIFY_ARGS(str && (cstr || !len));

    if (!len)
    {
        // noop
    }
    else if (index == str->size || index == UCB_NPOS)
    {
        ucb_str_append_utf8(str, cstr, len);
    }
    else
    {
        if (index > 0)
        {
            index = ucb_uc_char_index(str->data, str->size, index);
            UCB_VERIFY(index > 0 && index < str->size, UCB_ERROR_INVALID_ARG,
                       "Invalid character position or invalid UTF-8");
        }
        if (ucb_str_ensure(str, len))
        {
            memmove(str->data + index + len, str->data + index, str->size - index);
            memcpy(str->data + index, cstr, len);
            str->size += len;
            str->data[str->size] = '\0';
        }
    }
}

ucb_str* ucb_str_concat(const ucb_str* str1, const ucb_str* str2)
{
    UCB_VERIFY_ARGS(str1 && str2);

    size_t size = str1->size + str2->size;
    ucb_str* dst = ucb_malloc_type(1, ucb_str);
    if (dst)
    {
        if (size)
        {
            dst->data = ucb_malloc(size + 1);
            if (dst->data)
            {
                memcpy(dst->data, str1->data, str1->size);
                memcpy(dst->data + str1->size, str2->data, str2->size);
                dst->data[size] = '\0';
                dst->alloc = size + 1;
                dst->size = size;
            }
            else
            {
                ucb_free(dst);
                dst = UCB_NULL;
            }
        }
        else
        {
            dst->data = "";
            dst->size = 0;
            dst->alloc = 0;
        }
    }
    return dst;
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

ucb_str* ucb_str_substr_wrapped(const ucb_str* str, size_t start, size_t end)
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
        dst = ucb_str_new_wrap(str->data + start, end - start);
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
