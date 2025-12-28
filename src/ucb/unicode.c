/**
 * This file is part of the UCB project
 * SPDX-FileCopyrightText: 2025 Åke Svedin <ake@svedin.org>
 * SPDX-License-Identifier: MIT
 *
 * @brief Contains general unicode string implementations
 */

#include "ucb/unicode.h"

#include "ucb/bufutil.h"
#include "ucb/cstring.h"
#include "ucb/defines.h"
#include "ucb/errcodes.h"
#include "ucb/memory.h"

#include "unicode_combine.h"
#include "unicode_decomp.h"
#include "unicode_defines.h"
#include "unicode_mapping.h"
#include "unicode_props.h"

#include <assert.h>
#include <math.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

/* -------------------------------------------------------------------------- */
/*                               Property lookup                              */
/* -------------------------------------------------------------------------- */

static const ucb_uc_prop_t* ucb_uc_get_prop(uint32_t cp)
{
    assert(cp <= 0x110000);
    unsigned int offset = s_ucb_uc_stage1_table[cp / UCB_UC_BLOCK_SIZE] * UCB_UC_BLOCK_SIZE;
    unsigned int index  = s_ucb_uc_stage2_table[offset + cp % UCB_UC_BLOCK_SIZE];
    assert(index < UCB_UC_NUM_PROPERTIES);
    return s_ucb_uc_prop_table + index;
}

static const ucb_uc_mapping_t* ucb_uc_get_mapping(const ucb_uc_prop_t* prop)
{
    if (prop && prop->mapping_idx)
    {
        assert(prop->mapping_idx > 0 && prop->mapping_idx <= UCB_UC_NUM_MAPPING);
        return s_ucb_uc_mapping_table + prop->mapping_idx - 1;
    }
    return UCB_NULL;
}

static const ucb_uc_decomp_t* ucb_uc_get_decomp(const ucb_uc_prop_t* prop)
{
    if (prop && prop->decomp_idx)
    {
        assert(prop->decomp_idx > 0 && prop->decomp_idx <= UCB_UC_NUM_DECOMP);
        return s_ucb_uc_decomp_table + prop->decomp_idx - 1;
    }
    return UCB_NULL;
}

static const ucb_uc_combiners_t* ucb_uc_get_combiners(const ucb_uc_prop_t* prop)
{
    if (prop && prop->combiner_idx)
    {
        assert(prop->combiner_idx > 0 && prop->combiner_idx <= UCB_UC_NUM_COMBINER);
        return s_ucb_uc_combiner_table + prop->combiner_idx - 1;
    }
    return UCB_NULL;
}

static bool is_combining_mark(ucb_uc_prop_t* prop)
{
    return prop && prop->ccc > 0;
}

/* -------------------------------------------------------------------------- */
/*                                 Validation                                 */
/* -------------------------------------------------------------------------- */

static inline uint32_t ucb_uc_fetch2(const unsigned char* bytes)
{
    return ((bytes[0] & 0x1Fu) << 6) //
           | (bytes[1] & 0x3Fu);
}

static inline uint32_t ucb_uc_fetch3(const unsigned char* bytes)
{
    return ((bytes[0] & 0x0Fu) << 12)  //
           | ((bytes[1] & 0x3Fu) << 6) //
           | (bytes[2] & 0x3Fu);       //
}

static inline uint32_t ucb_uc_fetch4(const unsigned char* bytes)
{
    return ((bytes[0] & 0x07u) << 18)   //
           | ((bytes[1] & 0x3Fu) << 12) //
           | ((bytes[2] & 0x3Fu) << 6)  //
           | (bytes[3] & 0x3Fu);        //
}

/**
 * @brief UTF-8 string iterator, only works on validated string
 * @param iter pointer to current position, will increment to next valid position
 * @return uint32_t, might be NULL
 */
static inline uint32_t ucb_uc_next_valid(const unsigned char** iter)
{
    const unsigned char* bytes = *iter;
    uint32_t cp;
    if (bytes[0] < 0x80u)
    {
        cp    = bytes[0];
        *iter = bytes + 1;
    }
    else if ((bytes[0] & 0xE0) == 0xC0u)
    {
        cp    = ucb_uc_fetch2(bytes);
        *iter = bytes + 2;
    }
    else if ((bytes[0] & 0xF0) == 0xE0u)
    {
        cp    = ucb_uc_fetch3(bytes);
        *iter = bytes + 3;
    }
    else // 4-byte sequence
    {
        cp    = ucb_uc_fetch4(bytes);
        *iter = bytes + 4;
    }
    return cp;
}

uint32_t ucb_uc_iter_utf8(const unsigned char** iter)
{
    return ucb_uc_next_valid(iter);
}

// static inline size_t ucb_uc_iter_pos(const unsigned char* iter, const char* src)
// {
//     return (size_t)(iter - (const unsigned char*)src);
// }

ucb_ecode ucb_uc_validate(const char* str, size_t* size)
{
    if (!str)
        return UCB_ERROR_INVALID_ARG;

    size_t s = strlen(str);
    if (size)
        *size = s;
    return ucb_uc_validate_buf(str, s);
}

// NOTE:
// See implementation notes here:
// https://unicode.org/mail-arch/unicode-ml/y2003-m02/att-0467/01-The_Algorithm_to_Valide_an_UTF-8_String
ucb_ecode ucb_uc_validate_buf(const char* str, size_t size)
{
    if (str == NULL)
        return UCB_ERROR_INVALID_ARG;

    const unsigned char* bytes = (const unsigned char*)str;
    uint32_t cp;
    size_t i = 0;
    while (i < size)
    {
        unsigned char c = bytes[i];
        if (c < 0x80) // < 0xxx xxxx
        {
            i++;
            continue;
        }
        else if ((c & 0xE0u) == 0xC0u) // 110x xxxx, 2 bytes
        {
            if (i + 1 >= size || (bytes[i + 1] & 0xC0u) != 0x80u)
                return UCB_ERROR_INVALID_UTF8;
            cp = ucb_uc_fetch2(bytes + i);
            if (cp < 0x80u) // Overlong
                return UCB_ERROR_INVALID_UTF8;
            i += 2;
        }
        else if ((c & 0xF0u) == 0xE0u) // 1110 xxxx, 3 bytes
        {
            if (i + 2 >= size || (bytes[i + 1] & 0xC0u) != 0x80u || (bytes[i + 2] & 0xC0u) != 0x80u)
                return UCB_ERROR_INVALID_UTF8;
            cp = ucb_uc_fetch3(bytes + i);
            if (cp < 0x800u) // Overlong
                return UCB_ERROR_INVALID_UTF8;
            if (cp >= 0xD800u && cp <= 0xDFFFu) // Surrogate
                return UCB_ERROR_INVALID_UTF8;
            i += 3;
        }
        else if ((c & 0xF8u) == 0xF0u) // 1111 xxxx, 4 bytes
        {
            if (i + 3 >= size || (bytes[i + 1] & 0xC0u) != 0x80u ||
                (bytes[i + 2] & 0xC0u) != 0x80u || (bytes[i + 3] & 0xC0u) != 0x80u)
                return UCB_ERROR_INVALID_UTF8;
            cp = ucb_uc_fetch4(bytes + i);
            if (cp < 0x10000u || cp > 0x10FFFFu) // Out of range
                return UCB_ERROR_INVALID_UTF8;
            i += 4;
        }
        else
            return UCB_ERROR_INVALID_UTF8;
    }
    return UCB_OK;
}

/* -------------------------------------------------------------------------- */
/*                                    Enum                                    */
/* -------------------------------------------------------------------------- */

const char* ucb_uc_norm_form_to_str(ucb_norm_form_t form)
{
    switch (form)
    {
    case UCB_UC_NORM_NFC:
        return "NFC";
    case UCB_UC_NORM_NFD:
        return "NFD";
    case UCB_UC_NORM_NFKC:
        return "NFKC";
    case UCB_UC_NORM_NFKD:
        return "NFKD";
    case UCB_UC_NORM_INVALID:
        break;
    default:
        break;
    }
    return "";
}

ucb_norm_form_t ucb_uc_norm_form_from_str(const char* str)
{
    if (ucb_cstr_icomp(str, "NFC") == 0)
        return UCB_UC_NORM_NFC;
    else if (ucb_cstr_icomp(str, "NFD") == 0)
        return UCB_UC_NORM_NFD;
    else if (ucb_cstr_icomp(str, "NFKC") == 0)
        return UCB_UC_NORM_NFKC;
    else if (ucb_cstr_icomp(str, "NFKD") == 0)
        return UCB_UC_NORM_NFKD;
    else
        return UCB_UC_NORM_INVALID;
}

/* -------------------------------------------------------------------------- */
/*                             Buffer and encoder                             */
/* -------------------------------------------------------------------------- */

/**
 * Encodes an array of unicode code points into UTF-8 and appends them
 * to the buffer.
 * Assumes that the buffer codepoints and len are valid.
 */
ucb_ecode ucb_uc_encode_codepoints(ucb_buffer_t* buf, const uint32_t* codepoints, size_t len)
{
    // Minimum chunks to work with
    uint8_t bytes[5 * UCB_UC_MAX_MULTI_LEN];
    size_t bytes_len = 0;
    uint32_t cp;
    ucb_ecode err;
    for (size_t i = 0; i < len; i++)
    {
        cp = codepoints[i];

        if (cp <= 0x7F)
        {
            // 1-byte sequence (0xxxxxxx)
            bytes[bytes_len++] = (uint8_t)cp;
        }
        else if (cp <= 0x7FF)
        {
            // 2-byte sequence (110xxxxx 10xxxxxx)
            bytes[bytes_len++] = (uint8_t)(0xC0 | (cp >> 6));
            bytes[bytes_len++] = (uint8_t)(0x80 | (cp & 0x3F));
        }
        else if (cp <= 0xFFFF)
        {
            // 3-byte sequence (1110xxxx 10xxxxxx 10xxxxxx)
            bytes[bytes_len++] = (uint8_t)(0xE0 | (cp >> 12));
            bytes[bytes_len++] = (uint8_t)(0x80 | ((cp >> 6) & 0x3F));
            bytes[bytes_len++] = (uint8_t)(0x80 | (cp & 0x3F));
        }
        else if (cp <= 0x10FFFF)
        {
            // 4-byte sequence (11110xxx 10xxxxxx 10xxxxxx 10xxxxxx)
            bytes[bytes_len++] = (uint8_t)(0xF0 | (cp >> 18));
            bytes[bytes_len++] = (uint8_t)(0x80 | ((cp >> 12) & 0x3F));
            bytes[bytes_len++] = (uint8_t)(0x80 | ((cp >> 6) & 0x3F));
            bytes[bytes_len++] = (uint8_t)(0x80 | (cp & 0x3F));
        }
        else
        {
            // Invalid code point (outside Unicode range)
            return UCB_ERROR_INVALID_CODEPOINT;
        }

        // Push to buffer if next pass might overflow
        if (bytes_len >= 4 * UCB_UC_MAX_MULTI_LEN)
        {
            err = ucb_buffer_push(buf, bytes, bytes_len);
            if (err)
                return err;
            bytes_len = 0;
        }
    }
    // Push remaining bytes
    if (bytes_len)
        return ucb_buffer_push(buf, bytes, bytes_len);
    return UCB_OK;
}

/* -------------------------------------------------------------------------- */
/*                                 Characters                                 */
/* -------------------------------------------------------------------------- */

size_t ucb_uc_num_cp(const char* str)
{
    size_t num = 0;

    const unsigned char* iter = (const unsigned char*)str;
    while (ucb_uc_next_valid(&iter))
    {
        num++;
    }
    return num;
}

size_t ucb_uc_num_chars(const char* str)
{
    // Simple implementation before implementing grapheme clusters
    size_t num = 0;
    uint32_t cp;
    const ucb_uc_prop_t* prop;

    const unsigned char* iter = (const unsigned char*)str;
    while ((cp = ucb_uc_next_valid(&iter)))
    {
        prop = ucb_uc_get_prop(cp);
        if (UCB_UC_IS_MARK(prop->category))
            continue;
        num++;
    }
    return num;
}

/* -------------------------------------------------------------------------- */
/*                               Case conversion                              */
/* -------------------------------------------------------------------------- */

typedef enum ucb_uc_case_op
{
    UCB_UC_CASE_UPPER,
    UCB_UC_CASE_LOWER,
    UCB_UC_CASE_TITLE,
    UCB_UC_CASE_FOLD,
} ucb_uc_case_op_t;

// Returns true if the code point is a word separator for titlecase purposes.
static bool ucb_uc_is_word_separator(uint32_t cp, const ucb_uc_prop_t* prop)
{
    if (cp == 0x27)
        return false; // Apostrophe (') is not a separator
    // if (cp == 0x2D)
    //     return true; // Hyphen (-) is a separator (optional)

    switch (prop->category)
    {
    // Whitespace
    case UCB_UC_GC_ZS: // Space separator
    case UCB_UC_GC_ZL: // Line separator
    case UCB_UC_GC_ZP: // Paragraph separator
    // Punctuation (excluding apostrophes)
    case UCB_UC_GC_PO: // Other punctuation (e.g., !, ?, .)
    case UCB_UC_GC_PS: // Opening punctuation (e.g., (
    case UCB_UC_GC_PE: // Closing punctuation (e.g., ))
    // Controls
    case UCB_UC_GC_CC: // Control characters (e.g., \n, \t)
        return true;
    default:
        return false;
    }
}

typedef struct
{
    size_t out_len;
    uint32_t buf[UCB_UC_MAX_MULTI_LEN];
    ucb_uc_case_op_t op;
    const ucb_uc_prop_t* last_prop;
    uint32_t last_cp;
} casemap_ctx_t;

static ucb_ecode ucb_uc_case_map_cp(casemap_ctx_t* ctx)
{
    ctx->out_len = 1; // By default, keep value sent in
    if (!ctx->buf[0]) // Null terminator
        return UCB_OK;

    const ucb_uc_prop_t* prop = ucb_uc_get_prop(ctx->buf[0]);
    if (!prop)
        return UCB_ERROR_INVALID_CODEPOINT;

    const ucb_uc_mapping_t* mapping = ucb_uc_get_mapping(prop);
    if (!mapping)
    {
        ctx->last_cp   = ctx->buf[0];
        ctx->last_prop = prop;
        return UCB_OK;
    }

    uint16_t ref;
    switch (ctx->op)
    {
    case UCB_UC_CASE_UPPER:
        ref = mapping->to_upper;
        break;
    case UCB_UC_CASE_LOWER:
        ref = mapping->to_lower;
        break;
    case UCB_UC_CASE_TITLE: {
        if (!ctx->last_prop || ucb_uc_is_word_separator(ctx->last_cp, ctx->last_prop))
            ref = mapping->to_title;
        else
            ref = mapping->to_lower;
        break;
    }
    case UCB_UC_CASE_FOLD:
        ref = mapping->to_casefold;
        break;
    default:
        return UCB_ERROR_INVALID_ARG;
    }

    // No more use for last prop
    ctx->last_prop = prop;
    ctx->last_cp   = ctx->buf[0];

    if (!ref)
        return UCB_OK;

    if (ref <= UCB_UC_NUM_SIMPLE_MAP)
    {
        int idx     = ref - 1;
        ctx->buf[0] = s_ucb_uc_smap_table[idx].value;
    }
    else
    {
        int idx = ref - UCB_UC_NUM_SIMPLE_MAP - 1;
        if (idx > UCB_UC_NUM_MULTI_MAP)
            return UCB_ERROR_INTERNAL;
        const ucb_uc_mmap_t* entry = &s_ucb_uc_mmap_table[idx];

        size_t len = 0;
        for (; len < UCB_UC_MAX_MULTI_LEN && entry->value[len] != UCB_UC_NO_VALUE; len++)
        {
            ctx->buf[len] = entry->value[len];
        }
        ctx->out_len = len;
    }
    return UCB_OK;
}

/**
 * Instead of working on a string, work on a buffer object
 * Setting codepoint of different sizes + UTF-8 encoding may result in different string
 * lengths.
 */
static ucb_unicode_result_t ucb_uc_case_map(const char* str, size_t size, ucb_uc_case_op_t op,
                                            ucb_buffer_t* dstbuf)
{
    const unsigned char* iter = (const unsigned char*)str;
    uint32_t cp;
    ucb_ecode err;
    bool own_buffer = false;
    casemap_ctx_t ctx;
    ctx.last_cp   = UCB_UC_NO_VALUE;
    ctx.last_prop = UCB_NULL;
    ctx.op        = op;

    if (!dstbuf)
    {
        dstbuf = ucb_buffer_new_heap(size + 1);
        if (!dstbuf)
            return (ucb_unicode_result_t){.error = UCB_ERROR_UNKNOWN};
        dstbuf->grow_func = ucb_buffer_grow_double;
        own_buffer        = true;
    }

    err = ucb_buffer_ensure(dstbuf, size + 1);
    if (err != UCB_OK)
        return (ucb_unicode_result_t){.error = err};

    while ((cp = ucb_uc_next_valid(&iter)))
    {
        if (!cp)
            break;
        ctx.buf[0] = cp;

        err = ucb_uc_case_map_cp(&ctx);
        if (err != UCB_OK)
        {
            return (ucb_unicode_result_t){
                .error     = err,
                .error_pos = (const char*)iter,
            };
        }
        assert(ctx.out_len > 0 && ctx.out_len <= UCB_UC_MAX_MULTI_LEN);

        err = ucb_uc_encode_codepoints(dstbuf, ctx.buf, ctx.out_len);
        if (err != UCB_OK)
        {
            return (ucb_unicode_result_t){
                .error     = err,
                .error_pos = (const char*)iter,
            };
        }
    }
    err = ucb_buffer_push(dstbuf, "\0", 1);
    if (err != UCB_OK)
    {
        return (ucb_unicode_result_t){
            .error     = err,
            .error_pos = (const char*)iter,
        };
    }

    ucb_unicode_result_t ret = {.error = UCB_OK};
    if (own_buffer)
    {
        ucb_buffer_fit(dstbuf);
        ucb_buffer_transfer(dstbuf, (void**)&ret.data, &ret.size, UCB_NULL);
        ucb_buffer_free(dstbuf);
        ret.size -= 1; // Do not count null-terminator
    }
    else
    {
        ret.data = ucb_malloc_type(dstbuf->used, char);
        ret.size = dstbuf->used - 1;
        memcpy(ret.data, dstbuf->data, dstbuf->used);
    }
    return ret;
}

ucb_unicode_result_t ucb_uc_to_upper(const char* str, size_t size)
{
    return ucb_uc_case_map(str, size, UCB_UC_CASE_UPPER, UCB_NULL);
}

ucb_unicode_result_t ucb_uc_to_lower(const char* str, size_t size)
{
    return ucb_uc_case_map(str, size, UCB_UC_CASE_LOWER, UCB_NULL);
}

ucb_unicode_result_t ucb_uc_to_title(const char* str, size_t size)
{
    return ucb_uc_case_map(str, size, UCB_UC_CASE_TITLE, UCB_NULL);
}

ucb_unicode_result_t ucb_uc_casefold(const char* str, size_t size)
{
    return ucb_uc_case_map(str, size, UCB_UC_CASE_FOLD, UCB_NULL);
}

/* -------------------------------------------------------------------------- */
/*                                Normalization                               */
/* -------------------------------------------------------------------------- */

#define NORM_CTX_BUFSIZE 18
#define UC_FLAGS_COMPOSE 0x01
#define UC_FLAGS_COMPAT  0x02

typedef struct
{
    ucb_buffer_t cp; // Codepoint buffer
    uint32_t cp_buf[NORM_CTX_BUFSIZE];
    uint8_t ccc_buf[NORM_CTX_BUFSIZE];
    size_t len;
    char* errpos;
    uint8_t flags;
} norm_ctx_t;

static inline ucb_ecode norm_ctx_add(norm_ctx_t* ctx, uint32_t cp, uint8_t ccc)
{
    if (ctx->len == NORM_CTX_BUFSIZE)
        return UCB_ERROR_INTERNAL;

    ctx->cp_buf[ctx->len]  = cp;
    ctx->ccc_buf[ctx->len] = ccc;
    ctx->len++;

    // If this is a combining mark (CCC > 0), insert it in the correct position
    if (ccc > 0 && ctx->len > 1)
    {
        size_t i = ctx->len - 1;
        // Bubble it leftward past any combining marks with higher CCC
        while (i > 0 && ctx->ccc_buf[i - 1] > ccc)
        {
            // Swap with the previous mark
            ctx->cp_buf[i]      = ctx->cp_buf[i - 1];
            ctx->ccc_buf[i]     = ctx->ccc_buf[i - 1];
            ctx->cp_buf[i - 1]  = cp;
            ctx->ccc_buf[i - 1] = ccc;
            i--;
        }
    }

    return UCB_OK;
}

static ucb_ecode norm_ctx_flush(norm_ctx_t* ctx)
{
    if (ctx->len == 0)
        return UCB_OK;

    ucb_ecode err;
    if (ctx->flags & UC_FLAGS_COMPOSE)
    {
        // Flush as codepoints
        err = ucb_buffer_push(&ctx->cp, ctx->cp_buf, ctx->len * sizeof(uint32_t));
    }
    else
    {
        // Flush as UTF-8
        err = ucb_uc_encode_codepoints(&ctx->cp, ctx->cp_buf, ctx->len);
    }
    ctx->len = 0;
    return err;
}

static inline bool is_hangul_syllable(uint32_t c)
{
    return (c >= 0xAC00 && c <= 0xD7A3); // was 0xD7AF
}
static inline bool is_leading_jamo(uint32_t c)
{
    return (c >= 0x1100 && c <= 0x1112);
}
static inline bool is_vowel_jamo(uint32_t c)
{
    return (c >= 0x1161 && c <= 0x1175);
}
static inline bool is_trailing_jamo(uint32_t c)
{
    return (c >= 0x11A8 && c <= 0x11C2);
}

static ucb_ecode norm_decompose_hangul(norm_ctx_t* ctx, uint32_t syllable)
{
    assert(is_hangul_syllable(syllable));

    const uint32_t S      = syllable - 0xAC00;
    const uint32_t T_idx1 = S % 28;
    const uint32_t V_idx0 = ((S - T_idx1) % 588) / 28; // was = (S / 28) % 21;
    const uint32_t L_idx0 = S / 588;                   // was = S / (28 * 21);

    // No need to check error since buffer is assured.
    norm_ctx_add(ctx, 0x1100 + L_idx0, 0);
    norm_ctx_add(ctx, 0x1161 + V_idx0, 0);
    if (T_idx1 != 0)
        norm_ctx_add(ctx, 0x11A7 + T_idx1, 0);

    return UCB_OK;
}

static inline uint32_t compose_hangul(uint32_t L, uint32_t V, uint32_t T)
{
    const uint32_t L_idx0 = L - 0x1100;
    const uint32_t V_idx0 = V - 0x1161;
    const uint32_t T_idx1 = (T != 0) ? T - 0x11A7 : 0; // Note: SUBTRACT 0x11A8 - 1
    return 0xAC00 + (L_idx0 * 21 + V_idx0) * 28 + T_idx1;
}

static ucb_ecode norm_decompose_cp(norm_ctx_t* ctx, uint32_t cp, bool must_decomp)
{
    ucb_ecode ret = UCB_OK;

    uint8_t ccc                   = 0;
    const ucb_uc_prop_t* prop     = ucb_uc_get_prop(cp);
    const ucb_uc_decomp_t* decomp = UCB_NULL;
    if (prop)
    {
        ccc = prop->ccc;
        if (must_decomp)
            decomp = ucb_uc_get_decomp(prop);
    }
#ifdef UCB_UC_WITH_VERIF
    assert(!decomp || cp == decomp->cp);
#endif
    if (!decomp || !(decomp->type == UCB_UC_DC_CANON || ctx->flags & UC_FLAGS_COMPAT))
    {
        if (ccc == 0)
            norm_ctx_flush(ctx);
        // No decomposition or wrong type: append as-is
        return norm_ctx_add(ctx, cp, ccc);
    }

    // Recursively decompose each codepoint
    for (size_t i = 0; i < decomp->len && ret == UCB_OK; i++)
    {
        uint32_t next_cp = (decomp->len <= 2) ? decomp->vals[i] : decomp->ptr[i];
        ret              = norm_decompose_cp(ctx, next_cp, must_decomp);
    }
    return ret;
}

static uint32_t norm_compose_cp(uint32_t starter, uint32_t combiner,
                                const ucb_uc_combiners_t* combiners)
{
    if (!combiners)
        return 0;
#ifdef UCB_UC_WITH_VERIF
    UCB_UNUSED(starter);
    assert(starter == combiners->starter);
#endif
    const combiner_entry_t* entries = combiners->entries;

    // Binary search for the combiner
    size_t left  = 0;
    size_t right = combiners->len;
    while (left < right)
    {
        size_t mid = (left + right) / 2;
        if (entries[mid].combiner < combiner)
            left = mid + 1;
        else if (entries[mid].combiner > combiner)
            right = mid;
        else
        {
            assert(entries[mid].combiner == combiner);
            return entries[mid].composed; // Found
        }
    }
    return 0; // Not found
}

static size_t norm_compose(uint32_t* cps, size_t count)
{
    if (count < 2)
        return count; // Nothing to compose

    const ucb_uc_prop_t* prop;
    const ucb_uc_prop_t* prev_prop;
    const ucb_uc_combiners_t* combiners = UCB_NULL;
    // size_t last_starter_idx             = 0; // Only valid if combiners != NULL
    for (size_t i = 1; i < count; i++)
    {
        if (is_leading_jamo(cps[i - 1]) && is_vowel_jamo(cps[i]))
        {
            uint32_t T = (i + 1 < count) ? cps[i + 1] : 0;
            if (!is_trailing_jamo(T))
                T = 0;

            cps[i - 1] = compose_hangul(cps[i - 1], cps[i], T);
            // Remove 1 or 2 codepoints and shift the rest
            size_t shift = (T != 0) ? 2 : 1;
            memmove(&cps[i], &cps[i + shift], (count - i - shift) * sizeof(uint32_t));
            count -= shift;
            i--; // recheck same index
            continue;
        }

        // Seek back left to find last starting character
        // int due to possible overflow
        prop = ucb_uc_get_prop(cps[i]);
        for (int j = (int)i - 1; j >= 0; j--)
        {
            prev_prop         = ucb_uc_get_prop(cps[j]);
            combiners         = prev_prop ? ucb_uc_get_combiners(prev_prop) : UCB_NULL;
            uint32_t composed = 0;
            if (combiners)
                composed = norm_compose_cp(cps[j], cps[i], combiners);
            if (composed != 0)
            {
                cps[j] = composed;
                // Remove current combiner and shift the rest
                memmove(&cps[i], &cps[i + 1], (count - i - 1) * sizeof(uint32_t));
                count--;
                i--; // recheck same index
                break;
            }
            else
            {
                // If we can't compose with current j, we must check
                // That it doesn't block the composition chain
                uint8_t ccc_i = prop ? prop->ccc : 0;
                uint8_t ccc_j = prev_prop ? prev_prop->ccc : 0;
                if (ccc_j == 0 || ccc_j >= ccc_i)
                    break;
            }
        }
    }
    return count;
}

static ucb_ecode normalize(norm_ctx_t* ctx, const char* str, bool must_decomp)
{
    uint32_t cp;
    const unsigned char* iter = (const unsigned char*)str;
    ucb_ecode err           = UCB_OK;

    while ((cp = ucb_uc_next_valid(&iter)))
    {
        if (is_hangul_syllable(cp))
        {
            err = norm_decompose_hangul(ctx, cp);
            norm_ctx_flush(ctx);
        }
        else
        {
            err = norm_decompose_cp(ctx, cp, must_decomp);
            // Only flush if last is a starter
            if (ctx->ccc_buf[ctx->len - 1] == 0)
                norm_ctx_flush(ctx);
        }
        if (err != UCB_OK)
            return err;
    }
    norm_ctx_flush(ctx);

    if (ctx->flags & UC_FLAGS_COMPOSE)
    {
        uint32_t* cps = (uint32_t*)ctx->cp.data;
        size_t count  = ctx->cp.used / sizeof(uint32_t);

        count = norm_compose(cps, count);

        ucb_buffer_clear(&ctx->cp);
        err = ucb_uc_encode_codepoints(&ctx->cp, cps, count);
        if (err != UCB_OK)
            return err;
    }

    return ucb_buffer_push(&ctx->cp, "\0", 1);
}

static size_t ucb_uc_check_norm(const char* str, bool* is_latin1, bool* must_decomp)
{
    size_t num = 0;

    bool check_latin1 = true;
    bool check_decomp = must_decomp ? false : true;
    uint32_t cp;
    const unsigned char* iter = (const unsigned char*)str;
    while ((cp = ucb_uc_next_valid(&iter)))
    {
        num++;
        if (cp > 0x7F)
            check_latin1 = false;
        if (!check_decomp)
        {
            const ucb_uc_prop_t* prop = ucb_uc_get_prop(cp);
            if (!prop)
                continue;
            if (prop->ccc > 0 || (prop->flags & UCB_UC_PROP_COMPEXCL))
            {
                // Fast check failed, we need to do full normalization
                check_decomp = true;
            }
        }
    }
    if (is_latin1)
        *is_latin1 = check_latin1;
    if (must_decomp)
        *must_decomp = check_decomp;
    return num;
}

ucb_unicode_result_t ucb_uc_normalize(const char* str, size_t size, ucb_norm_form_t form)
{
    UCB_UNUSED(size);
    norm_ctx_t ctx = {0};

    bool must_decomp = true;

    switch (form)
    {
    case UCB_UC_NORM_NFD:
        break;
    case UCB_UC_NORM_NFKD:
        ctx.flags |= UC_FLAGS_COMPAT;
        break;
    case UCB_UC_NORM_NFC:
        ctx.flags |= UC_FLAGS_COMPOSE;
        break;
    case UCB_UC_NORM_NFKC:
        ctx.flags |= UC_FLAGS_COMPAT | UC_FLAGS_COMPOSE;
        break;
    case UCB_UC_NORM_INVALID:
    default:
        return (ucb_unicode_result_t){.error = UCB_ERROR_INVALID_ARG};
    }

    if (!str)
    {
        return (ucb_unicode_result_t){.error = UCB_ERROR_INVALID_ARG};
    }

    // Find out how many codepoints we deal with and do fast checks
    bool is_latin1 = false;
    size_t numcp   = ucb_uc_check_norm(str, &is_latin1, must_decomp ? NULL : &must_decomp);

    // FIXME: Implement quick check according to:
    // https://unicode.org/reports/tr15/#Detecting_Normalization_Forms
    // And parsing DerivedNormalizationProps.txt
    // if ((ctx.flags & UC_FLAGS_COMPOSE) && is_latin1 && !must_decomp)
    // {
    //     char* res = ucb_malloc_type(size + 1, char);
    //     memcpy(res, str, size);
    //     res[size] = '\0';
    //     return (ucb_unicode_result_t){.error = UCB_OK, .data = res, .size = size};
    // }

    ucb_ecode err;

    // Allocate for worst case decompose. This can be adjusted once
    // we can quick check the input.
    err = ucb_buffer_init_heap(&ctx.cp, 4 * numcp * sizeof(uint32_t) + 1);
    if (err != UCB_OK)
        return (ucb_unicode_result_t){.error = err};
    ctx.cp.grow_func = ucb_buffer_grow_double;

    err = normalize(&ctx, str, must_decomp);
    if (err != UCB_OK)
        return (ucb_unicode_result_t){.error = err, .error_pos = ctx.errpos};

    ucb_unicode_result_t ret = {.error = UCB_OK};
    ucb_buffer_fit(&ctx.cp);
    ucb_buffer_transfer(&ctx.cp, (void**)&ret.data, &ret.size, UCB_NULL);
    ucb_buffer_release(&ctx.cp);
    ret.size -= 1; // Do not count null-terminator
    return ret;
}
