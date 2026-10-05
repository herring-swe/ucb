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
#include "ucb/memdbg.h"
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
 * Internal helper. Returns true if @p cstr with explicit @p len contains an
 * embedded null character. ucb_str never carries interior nulls, so input
 * containing one is invalid. Used by @ref ucb_str_cstr_validate.
 */
static bool ucb_str_contains_nul(const char* cstr, size_t len)
{
    return len && memchr(cstr, '\0', len) != UCB_NULL;
}

/**
 * Internal helper. Maps a 0-based character @p index to a byte offset in
 * @p str. The valid range is <tt>0 <= index <= ucb_str_num_char(str)</tt>,
 * where @p index equal to the character count maps to the end of the string
 * (a byte offset of <tt>str->size</tt>) and is used as the append position.
 * Returns true and sets @p pos on success. On failure returns false and sets
 * @p oob to true when @p str is valid UTF-8 but @p index is past the last
 * character (an argument error), and to false when @p str is not valid UTF-8
 * (a data error). ucb_uc_char_index only works on validated strings and
 * asserts on invalid lead bytes, so validation must run first.
 */
static bool ucb_str_map_char_pos(const ucb_str* str, size_t index, size_t* pos, bool* oob)
{
    ucb_error* err = UCB_NULL;
    bool valid = ucb_uc_validate(str->data, str->size, &err);
    ucb_error_clear(&err);
    if (!valid)
    {
        *oob = false;
        return false;
    }

    if (index > ucb_uc_num_char(str->data, str->size))
    {
        *oob = true;
        return false;
    }

    if (index == 0)
    {
        *pos = 0;
        return true;
    }

    // For 1 <= index <= num_char, ucb_uc_char_index returns the byte offset
    // just past the index-th character. At index == num_char this is str->size.
    *pos = ucb_uc_char_index(str->data, str->size, index);
    return true;
}

/**
 * Internal helper. Returns the exact validation error for @p cstr with explicit
 * @p len, or UCB_STR_CSTR_OK. A UCB_NULL cstr is the interned empty string and
 * is valid. When @p len is zero the string is measured with strlen. This is the
 * single source of truth for @ref ucb_str_is_valid and the verifier below.
 */
static ucb_str_cstr_error ucb_str_cstr_validate(const char* cstr, size_t len)
{
    if (!cstr)
        return UCB_STR_CSTR_OK;

    if (!len)
        len = strlen(cstr);
    else if (ucb_str_contains_nul(cstr, len))
        return UCB_STR_CSTR_ERROR_EMBEDDED_NUL;

    if (!ucb_uc_validate(cstr, len, UCB_NULL))
        return UCB_STR_CSTR_ERROR_INVALID_UTF8;

    return UCB_STR_CSTR_OK;
}

/**
 * Internal helper. Human readable help text for an enum value, used to build
 * the argument error reported by the ucb_str functions.
 */
static const char* ucb_str_cstr_error_msg(ucb_str_cstr_error etype)
{
    switch (etype)
    {
    case UCB_STR_CSTR_OK:
        return "is valid";
    case UCB_STR_CSTR_ERROR_EMBEDDED_NUL:
        return "must not contain an embedded null character";
    case UCB_STR_CSTR_ERROR_INVALID_UTF8:
        return "must be valid UTF-8";
    }
    return "is invalid";
}

/**
 * Internal helper. Aborts with a descriptive argument error when @p cstr is not
 * valid input. See @ref ucb_str_is_valid.
 */
static void ucb_str_verify_cstr(const char* cstr, size_t len, const char* param)
{
    ucb_str_cstr_error etype = ucb_str_cstr_validate(cstr, len);
    UCB_VERIFY(etype == UCB_STR_CSTR_OK,
               UCB_ERROR_INVALID_ARG,
               "%s %s",
               param,
               ucb_str_cstr_error_msg(etype));
}

bool ucb_str_is_valid(const char* cstr, size_t len, ucb_str_cstr_error* etype)
{
    ucb_str_cstr_error result = ucb_str_cstr_validate(cstr, len);
    if (etype)
        *etype = result;
    return result == UCB_STR_CSTR_OK;
}

/* -------------------------------------------------------------------------- */
/*                                Construction                                */
/* -------------------------------------------------------------------------- */

/**
 * Internal function.
 * str must have been verified (non-null)
 *
 * If cstr is UCB_NULL, str is set to the interned empty string. Otherwise an
 * owned allocation is made. The input must follow the rules of @ref
 * ucb_str_is_valid: no embedded null character and valid UTF-8, otherwise it is
 * API misuse and aborts. Returns false only when the allocation fails.
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

    ucb_str_verify_cstr(cstr, len, "cstr");

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

void ucb_str_adopt(ucb_str* str, char* cstr, size_t len, size_t alloc)
{
    UCB_VERIFY_ARGS(str && cstr);

    if (UCB_MEMTRACK_IS_ENABLED())
    {
        size_t tracked_alloc = 0;
        UCB_VERIFY(UCB_MEM_IS_ALLOC(cstr, &tracked_alloc),
                   UCB_ERROR_INVALID_ARG,
                   "cstr must be a UCB allocation");
        UCB_VERIFY(alloc == tracked_alloc,
                   UCB_ERROR_INVALID_ARG,
                   "alloc doesn't match the allocated size (%zu != %zu)",
                   alloc,
                   tracked_alloc);
    }

    if (!len)
        len = strlen(cstr);

    ucb_str_verify_cstr(cstr, len, cstr);

    UCB_VERIFY(alloc >= len + 1, UCB_ERROR_INVALID_ARG, "alloc must be at least len + 1");
    UCB_VERIFY(cstr[len] == '\0',
               UCB_ERROR_INVALID_ARG,
               "cstr must be null-terminated at cstr[len]");

    ucb_str_release_common(str);

    str->data = cstr;
    str->size = len;
    str->alloc = alloc;
}

void ucb_str_adopt_c(ucb_str* str, char* cstr)
{
    UCB_VERIFY_ARGS(str && cstr);

    size_t len = strlen(cstr);
    ucb_str_adopt(str, cstr, len, len + 1);
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

bool ucb_str_append_cp(ucb_str* str, const ucb_cp* cp, size_t num_cp, ucb_error** perr)
{
    UCB_VERIFY_ARGS(str && cp);
    bool valid = false;

    for (size_t i = 0; i < num_cp; i++)
    {
        UCB_VERIFY(cp[i] != 0,
                   UCB_ERROR_INVALID_CODEPOINT,
                   "Zero codepoint is not allowed, at index %zu",
                   i);
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
            valid = true;
        }
        ucb_buffer_release(&buffer);
    }
    return valid;
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

    ucb_str_verify_cstr(cstr, len, "cstr");

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

/**
 * Internal helper. Inserts @p len bytes from @p cstr at byte offset @p pos in
 * @p str. @p pos must already be a validated byte boundary in the range
 * [0, str->size]; this function performs no position or encoding validation.
 * It is safe when @p cstr aliases @p str.
 */
static void ucb_str_insert_at(ucb_str* str, size_t pos, const char* cstr, size_t len)
{
    if (!len)
        return;

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
        memmove(str->data + pos + len, str->data + pos, str->size - pos);
        memcpy(str->data + pos, cstr, len);
        str->size += len;
        str->data[str->size] = '\0';
    }
    ucb_free(tmp);
}

bool ucb_str_insert_cp(ucb_str* str,
                       size_t index,
                       const ucb_cp* cp,
                       size_t num_cp,
                       ucb_error** perr)
{
    UCB_VERIFY_ARGS(str && (cp || !num_cp));

    for (size_t i = 0; i < num_cp; i++)
    {
        UCB_VERIFY(cp[i] != 0,
                   UCB_ERROR_INVALID_CODEPOINT,
                   "Zero codepoint is not allowed, at index %zu",
                   i);
    }

    if (!num_cp)
        return true;

    // Resolve the character index to a byte position once. Out of range is an
    // argument error and invalid UTF-8 in str is a data error; both are
    // reported through perr here because this function does not abort on them.
    size_t pos;
    bool oob;
    if (!ucb_str_map_char_pos(str, index, &pos, &oob))
    {
        if (oob)
            ucb_throw_format(perr,
                             UCB_ERROR_INVALID_ARG,
                             "Character position %zu out of range",
                             index);
        else
            ucb_throw_format(perr, UCB_ERROR_INVALID_UTF8, "Invalid UTF-8 in string");
        return false;
    }

    size_t max_size = 4 * num_cp;
    ucb_buffer buffer;

    ucb_buffer_init_heap(&buffer, max_size);
    bool valid = ucb_uc_encode_codepoints(&buffer, cp, num_cp, perr);
    if (valid)
        ucb_str_insert_at(str, pos, buffer.data, buffer.size);
    ucb_buffer_release(&buffer);
    return valid;
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

    ucb_str_verify_cstr(cstr, len, "cstr");

    // Resolve the character index to a byte position once. An index past the
    // last character is an argument error and aborts; invalid UTF-8 in str is
    // a data error and inserts nothing.
    size_t pos;
    bool oob;
    if (!ucb_str_map_char_pos(str, index, &pos, &oob))
    {
        if (oob)
            UCB_REPORT(UCB_ERROR_INVALID_ARG, "Character position out of range");
        return;
    }

    ucb_str_insert_at(str, pos, cstr, len);
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

/**
 * Internal helper. Adopts the mapped result on success; on failure clears
 * the produced error (encoding error or out of memory) and leaves @p str
 * unchanged.
 */
static bool ucb_str_adopt_uc_result(ucb_str* str, ucb_uc_result res, ucb_error* err)
{
    if (!res.data)
    {
        ucb_error_clear(&err);
        return false;
    }
    ucb_str_adopt(str, res.data, res.size, res.size + 1);
    return true;
}

bool ucb_str_to_lower(ucb_str* str)
{
    UCB_VERIFY_ARGS(str);

    ucb_error* err = UCB_NULL;
    ucb_uc_result res = ucb_uc_to_lower(str->data, str->size, &err);
    return ucb_str_adopt_uc_result(str, res, err);
}

bool ucb_str_to_upper(ucb_str* str)
{
    UCB_VERIFY_ARGS(str);

    ucb_error* err = UCB_NULL;
    ucb_uc_result res = ucb_uc_to_upper(str->data, str->size, &err);
    return ucb_str_adopt_uc_result(str, res, err);
}

bool ucb_str_to_title(ucb_str* str)
{
    UCB_VERIFY_ARGS(str);

    ucb_error* err = UCB_NULL;
    ucb_uc_result res = ucb_uc_to_title(str->data, str->size, &err);
    return ucb_str_adopt_uc_result(str, res, err);
}

bool ucb_str_casefold(ucb_str* str)
{
    UCB_VERIFY_ARGS(str);

    ucb_error* err = UCB_NULL;
    ucb_uc_result res = ucb_uc_casefold(str->data, str->size, &err);
    return ucb_str_adopt_uc_result(str, res, err);
}

bool ucb_str_normalize(ucb_str* str, ucb_norm_form form)
{
    UCB_VERIFY_ARGS(str && form >= UCB_NORM_NFD && form <= UCB_NORM_NFKC);

    ucb_error* err = UCB_NULL;
    ucb_uc_result res = ucb_uc_normalize(str->data, str->size, form, &err);
    return ucb_str_adopt_uc_result(str, res, err);
}
