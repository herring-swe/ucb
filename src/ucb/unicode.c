/**
 * @file unicode.c
 *
 * This file is part of the UCB project
 * - SPDX-FileCopyrightText: © 2026 Åke Svedin <ake@svedin.org>
 * - SPDX-License-Identifier: MIT
 *
 * @brief Unicode support implementation
 */

#include "ucb/unicode.h"

#include "unicode_combine.h"
#include "unicode_decomp.h"
#include "unicode_defines.h"
#include "unicode_mapping.h"
#include "unicode_private.h"
#include "unicode_props.h"

#include "ucb/bufutil.h"
#include "ucb/cstring.h"
#include "ucb/debug.h"
#include "ucb/defines.h"
#include "ucb/error.h"
#include "ucb/memory.h"

#include <assert.h>
#include <inttypes.h>
#include <math.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

/* -------------------------------------------------------------------------- */
/*                                   Macros                                   */
/* -------------------------------------------------------------------------- */

#define FOR_EACH_CODEPOINT(cp, str, len)                              \
    const unsigned char* _iter = (const unsigned char*)(str);         \
    const unsigned char* _last = (const unsigned char*)_iter + (len); \
    for (; ucb_uc_iter_next(&_iter, _last, &(cp));)

#define FOR_EACH_CODEPOINT_CHECK_RET() \
    UCB_VERIFY(_iter == _last, UCB_ERROR_INVALID_UTF8, "Invalid UTF-8")

#define CODEPOINT_BYTE_POS(str) (size_t)(_iter - (const unsigned char*)(str))

#define UCB_COMP(a, b) (((a) > (b)) ? 1 : ((a) < (b)) ? -1 : 0)

/* -------------------------------------------------------------------------- */
/*                               Property lookup                              */
/* -------------------------------------------------------------------------- */

static const ucb_uc_prop* ucb_uc_get_prop(ucb_cp cp)
{
    assert(cp <= 0x110000);
    unsigned int offset = s_ucb_uc_stage1_table[cp / UCB_UC_BLOCK_SIZE] * UCB_UC_BLOCK_SIZE;
    unsigned int index = s_ucb_uc_stage2_table[offset + cp % UCB_UC_BLOCK_SIZE];
    assert(index < UCB_UC_NUM_PROPERTIES);
    return s_ucb_uc_prop_table + index;
}

static const ucb_uc_mapping* ucb_uc_get_mapping(const ucb_uc_prop* prop)
{
    if (prop && prop->mapping_idx)
    {
        assert(prop->mapping_idx > 0 && prop->mapping_idx <= UCB_UC_NUM_MAPPING);
        return s_ucb_uc_mapping_table + prop->mapping_idx - 1;
    }
    return UCB_NULL;
}

static const ucb_uc_decomp* ucb_uc_get_decomp(const ucb_uc_prop* prop)
{
    if (prop && prop->decomp_idx)
    {
        assert(prop->decomp_idx > 0 && prop->decomp_idx <= UCB_UC_NUM_DECOMP);
        return s_ucb_uc_decomp_table + prop->decomp_idx - 1;
    }
    return UCB_NULL;
}

static const ucb_uc_combiners* ucb_uc_get_combiners(const ucb_uc_prop* prop)
{
    if (prop && prop->combiner_idx)
    {
        assert(prop->combiner_idx > 0 && prop->combiner_idx <= UCB_UC_NUM_COMBINER);
        return s_ucb_uc_combiner_table + prop->combiner_idx - 1;
    }
    return UCB_NULL;
}

static inline ucb_uc_gcb ucb_uc_prop_gcb(const ucb_uc_prop* prop)
{
    return prop ? (ucb_uc_gcb)prop->gcb : UCB_UC_GCB_OTHER;
}

static inline ucb_uc_incb ucb_uc_prop_incb(const ucb_uc_prop* prop)
{
    return prop ? (ucb_uc_incb)prop->incb : UCB_UC_INCB_NONE;
}

static inline bool ucb_uc_prop_extpict(const ucb_uc_prop* prop)
{
    return prop && prop->extpict;
}

// static bool is_combining_mark(ucb_uc_prop* prop)
// {
//     return prop && prop->ccc > 0;
// }

/* -------------------------------------------------------------------------- */
/*                                 Validation                                 */
/* -------------------------------------------------------------------------- */

static inline ucb_cp ucb_uc_fetch2(const unsigned char* bytes)
{
    return ((bytes[0] & 0x1Fu) << 6) //
           | (bytes[1] & 0x3Fu);
}

static inline ucb_cp ucb_uc_fetch3(const unsigned char* bytes)
{
    return ((bytes[0] & 0x0Fu) << 12)  //
           | ((bytes[1] & 0x3Fu) << 6) //
           | (bytes[2] & 0x3Fu);       //
}

static inline ucb_cp ucb_uc_fetch4(const unsigned char* bytes)
{
    return ((bytes[0] & 0x07u) << 18)   //
           | ((bytes[1] & 0x3Fu) << 12) //
           | ((bytes[2] & 0x3Fu) << 6)  //
           | (bytes[3] & 0x3Fu);        //
}

/**
 * @brief UTF-8 string iterator, only works on validated string
 * @param iter pointer to current position, will increment to next valid position
 * @return ucb_cp, might be NULL
 */
static ucb_cp ucb_uc_next_valid(const unsigned char** iter)
{
    const unsigned char* bytes = *iter;
    ucb_cp cp;
    if (bytes[0] < 0x80u)
    {
        cp = bytes[0];
        *iter = bytes + 1;
    }
    else if ((bytes[0] & 0xE0) == 0xC0u)
    {
        cp = ucb_uc_fetch2(bytes);
        *iter = bytes + 2;
    }
    else if ((bytes[0] & 0xF0) == 0xE0u)
    {
        cp = ucb_uc_fetch3(bytes);
        *iter = bytes + 3;
    }
    else // 4-byte sequence
    {
        cp = ucb_uc_fetch4(bytes);
        *iter = bytes + 4;
    }
    return cp;
}

ucb_cp ucb_uc_iter_utf8(const unsigned char** iter)
{
    return ucb_uc_next_valid(iter);
}

/**
 * @brief Decode the next codepoint if it starts before @p last.
 *
 * Unlike @ref ucb_uc_next_valid(), this never reads at or past @p last, so it
 * is safe for buffers that are not null-terminated.
 *
 * @param iter in/out pointer to the current position
 * @param last pointer one past the end of the buffer
 * @param out receives the decoded codepoint
 * @return true if a codepoint was decoded, false if @p *iter reached @p last
 */
static bool ucb_uc_iter_next(const unsigned char** iter, const unsigned char* last, ucb_cp* out)
{
    if (*iter >= last)
        return false;
    *out = ucb_uc_next_valid(iter);
    return true;
}

// static inline size_t ucb_uc_iter_pos(const unsigned char* iter, const char* src)
// {
//     return (size_t)(iter - (const unsigned char*)src);
// }

// NOTE:
// See implementation notes here:
// https://unicode.org/mail-arch/unicode-ml/y2003-m02/att-0467/01-The_Algorithm_to_Valide_an_UTF-8_String
bool ucb_uc_validate(const char* str, size_t len, ucb_error** perr)
{
    UCB_VERIFY_ARGS(str);

    if (len == UCB_NPOS)
        len = strlen(str);

    const unsigned char* bytes = (const unsigned char*)str;

    bool success = false;
    size_t errpos = SIZE_MAX;
    size_t expected = 0;

    ucb_cp cp;
    size_t i = 0;

    while (i < len)
    {
        unsigned char c = bytes[i];
        if (c < 0x80) // < 0xxx xxxx
        {
            i++;
            continue;
        }
        else if ((c & 0xE0u) == 0xC0u) // 110x xxxx, 2 bytes
        {
            if (i + 1 >= len) // Truncated
            {
                errpos = i;
                expected = 2;
                break;
            }
            if ((bytes[i + 1] & 0xC0u) != 0x80u)
            {
                errpos = i;
                break;
            }
            cp = ucb_uc_fetch2(bytes + i);
            if (cp < 0x80u) // Overlong
            {
                errpos = i;
                break;
            }
            i += 2;
        }
        else if ((c & 0xF0u) == 0xE0u) // 1110 xxxx, 3 bytes
        {
            if (i + 2 >= len) // Truncated
            {
                errpos = i;
                expected = 3;
                break;
            }
            if ((bytes[i + 1] & 0xC0u) != 0x80u || (bytes[i + 2] & 0xC0u) != 0x80u)
            {
                errpos = i;
                break;
            }
            cp = ucb_uc_fetch3(bytes + i);
            if (cp < 0x800u) // Overlong
            {
                errpos = i;
                break;
            }
            if (cp >= 0xD800u && cp <= 0xDFFFu) // Surrogate
            {
                errpos = i;
                break;
            }
            i += 3;
        }
        else if ((c & 0xF8u) == 0xF0u) // 1111 xxxx, 4 bytes
        {
            if (i + 3 >= len) // Truncated
            {
                errpos = i;
                expected = 4;
                break;
            }
            if ((bytes[i + 1] & 0xC0u) != 0x80u || (bytes[i + 2] & 0xC0u) != 0x80u ||
                (bytes[i + 3] & 0xC0u) != 0x80u)
            {
                errpos = i;
                break;
            }
            cp = ucb_uc_fetch4(bytes + i);
            if (cp < 0x10000u || cp > 0x10FFFFu) // Out of range
            {
                errpos = i;
                break;
            }
            i += 4;
        }
        else
        {
            errpos = i;
            break;
        }
    }
    if (i == len)
    {
        success = true;
    }
    else if (perr)
    {
        if (expected)
        {
            ucb_throw_format(
                perr,
                UCB_ERROR_INVALID_UTF8,
                "Truncated UTF-8 sequence at position %zu: expected %zu bytes, got %zu",
                errpos,
                expected,
                len - errpos);
        }
        else
        {
            ucb_throw_format(perr,
                             UCB_ERROR_INVALID_UTF8,
                             "Invalid UTF-8 sequence at position %zu",
                             errpos);
        }
    }
    return success;
}

/* -------------------------------------------------------------------------- */
/*                             Buffer and encoder                             */
/* -------------------------------------------------------------------------- */

int ucb_uc_encode_codepoint(uint8_t* dst, const ucb_cp cp)
{
    int bytes_len = 0;
    if (cp <= 0x7F)
    {
        // 1-byte sequence (0xxxxxxx)
        if (dst)
        {
            dst[bytes_len++] = (uint8_t)cp;
        }
        else
        {
            bytes_len++;
        }
    }
    else if (cp <= 0x7FF)
    {
        // 2-byte sequence (110xxxxx 10xxxxxx)
        if (dst)
        {
            dst[bytes_len++] = (uint8_t)(0xC0 | (cp >> 6));
            dst[bytes_len++] = (uint8_t)(0x80 | (cp & 0x3F));
        }
        else
        {
            bytes_len += 2;
        }
    }
    else if (cp <= 0xFFFF)
    {
        // Surrogates are not valid Unicode scalar values
        if (cp >= 0xD800 && cp <= 0xDFFF)
            return -1;
        // 3-byte sequence (1110xxxx 10xxxxxx 10xxxxxx)
        if (dst)
        {
            dst[bytes_len++] = (uint8_t)(0xE0 | (cp >> 12));
            dst[bytes_len++] = (uint8_t)(0x80 | ((cp >> 6) & 0x3F));
            dst[bytes_len++] = (uint8_t)(0x80 | (cp & 0x3F));
        }
        else
        {
            bytes_len += 3;
        }
    }
    else if (cp <= 0x10FFFF)
    {
        // 4-byte sequence (11110xxx 10xxxxxx 10xxxxxx 10xxxxxx)
        if (dst)
        {
            dst[bytes_len++] = (uint8_t)(0xF0 | (cp >> 18));
            dst[bytes_len++] = (uint8_t)(0x80 | ((cp >> 12) & 0x3F));
            dst[bytes_len++] = (uint8_t)(0x80 | ((cp >> 6) & 0x3F));
            dst[bytes_len++] = (uint8_t)(0x80 | (cp & 0x3F));
        }
        else
        {
            bytes_len += 4;
        }
    }
    else
    {
        // Invalid code point (outside Unicode range)
        return -1;
    }
    return bytes_len;
}

/**
 * Encodes an array of unicode code points into UTF-8 and appends them
 * to the buffer.
 * Assumes that the buffer codepoints and len are valid.
 */
bool ucb_uc_encode_codepoints(ucb_buffer* buf,
                              const ucb_cp* codepoints,
                              size_t len,
                              ucb_error** perr)
{
    // Minimum chunks to work with
    uint8_t bytes[5 * UCB_UC_MAX_MULTI_LEN];
    size_t bytes_len = 0;
    for (size_t i = 0; i < len; i++)
    {
        int res = ucb_uc_encode_codepoint(bytes + bytes_len, codepoints[i]);
        if (res < 0)
        {
            if (perr)
                ucb_throw_format(perr,
                                 UCB_ERROR_INVALID_CODEPOINT,
                                 "Invalid codepoint: " PRIu32,
                                 codepoints[i]);
            return false;
        }
        else
        {
            bytes_len += (size_t)res;
        }

        // Push to buffer if next pass might overflow
        if (bytes_len >= 4 * UCB_UC_MAX_MULTI_LEN)
        {
            if (!ucb_buffer_push(buf, bytes, bytes_len))
                return false;
            bytes_len = 0;
        }
    }
    // Push remaining bytes
    return bytes_len == 0 || ucb_buffer_push(buf, bytes, bytes_len);
}

/* -------------------------------------------------------------------------- */
/*                                 Characters                                 */
/* -------------------------------------------------------------------------- */

size_t ucb_uc_num_cp(const char* str, size_t len)
{
    size_t num = 0;
    if (str)
    {
        if (len == UCB_NPOS)
            len = strlen(str);

        ucb_cp cp;
        FOR_EACH_CODEPOINT(cp, str, len)
        {
            UCB_UNUSED(cp);
            num++;
        }
        FOR_EACH_CODEPOINT_CHECK_RET();
    }
    return num;
}

/* -------------------------------------------------------------------------- */
/*                          Extended grapheme clusters                        */
/* -------------------------------------------------------------------------- */

// State carried while scanning for grapheme cluster boundaries (UAX #29).
typedef struct ucb_uc_grapheme_state
{
    bool ri_odd;      // Odd number of consecutive Regional_Indicator before current
    uint8_t gb11;     // 0 none, 1 ExtPict, 2 ExtPict Extend*, 3 ExtPict Extend* ZWJ
    bool incb_seq;    // Inside Consonant [Extend Linker]*
    bool incb_linker; // A Linker has been seen since the consonant
} ucb_uc_grapheme_state;

static void ucb_uc_grapheme_update(ucb_uc_grapheme_state* st,
                                   ucb_uc_gcb gcb,
                                   bool extpict,
                                   ucb_uc_incb incb)
{
    // GB11: track Extended_Pictographic Extend* ZWJ
    if (extpict)
        st->gb11 = 1;
    else if (gcb == UCB_UC_GCB_EXTEND && (st->gb11 == 1 || st->gb11 == 2))
        st->gb11 = 2;
    else if (gcb == UCB_UC_GCB_ZWJ && (st->gb11 == 1 || st->gb11 == 2))
        st->gb11 = 3;
    else
        st->gb11 = 0;

    // GB12/GB13: parity of consecutive regional indicators
    if (gcb == UCB_UC_GCB_REGIONAL_INDICATOR)
        st->ri_odd = !st->ri_odd;
    else
        st->ri_odd = false;

    // GB9c: track Consonant [Extend Linker]* with at least one Linker
    if (incb == UCB_UC_INCB_CONSONANT)
    {
        st->incb_seq = true;
        st->incb_linker = false;
    }
    else if (st->incb_seq && (incb == UCB_UC_INCB_EXTEND || incb == UCB_UC_INCB_LINKER))
    {
        if (incb == UCB_UC_INCB_LINKER)
            st->incb_linker = true;
    }
    else
    {
        st->incb_seq = false;
        st->incb_linker = false;
    }
}

// Evaluate the UAX #29 boundary rules in order. Returns true when a break is
// required between the previous codepoint (described by @p st) and the current.
static bool ucb_uc_grapheme_should_break(const ucb_uc_grapheme_state* st,
                                         ucb_uc_gcb a,
                                         ucb_uc_gcb b,
                                         bool cur_extpict,
                                         ucb_uc_incb cur_incb)
{
    // GB3: CR x LF
    if (a == UCB_UC_GCB_CR && b == UCB_UC_GCB_LF)
        return false;
    // GB4: (Control | CR | LF) ÷
    if (a == UCB_UC_GCB_CONTROL || a == UCB_UC_GCB_CR || a == UCB_UC_GCB_LF)
        return true;
    // GB5: ÷ (Control | CR | LF)
    if (b == UCB_UC_GCB_CONTROL || b == UCB_UC_GCB_CR || b == UCB_UC_GCB_LF)
        return true;
    // GB6: L x (L | V | LV | LVT)
    if (a == UCB_UC_GCB_L &&
        (b == UCB_UC_GCB_L || b == UCB_UC_GCB_V || b == UCB_UC_GCB_LV || b == UCB_UC_GCB_LVT))
        return false;
    // GB7: (LV | V) x (V | T)
    if ((a == UCB_UC_GCB_LV || a == UCB_UC_GCB_V) && (b == UCB_UC_GCB_V || b == UCB_UC_GCB_T))
        return false;
    // GB8: (LVT | T) x T
    if ((a == UCB_UC_GCB_LVT || a == UCB_UC_GCB_T) && b == UCB_UC_GCB_T)
        return false;
    // GB9: x (Extend | ZWJ)
    if (b == UCB_UC_GCB_EXTEND || b == UCB_UC_GCB_ZWJ)
        return false;
    // GB9a: x SpacingMark
    if (b == UCB_UC_GCB_SPACINGMARK)
        return false;
    // GB9b: Prepend x
    if (a == UCB_UC_GCB_PREPEND)
        return false;
    // GB9c: Consonant [Extend Linker]* Linker [Extend Linker]* x Consonant
    if (st->incb_seq && st->incb_linker && cur_incb == UCB_UC_INCB_CONSONANT)
        return false;
    // GB11: ExtPict Extend* ZWJ x ExtPict
    if (st->gb11 == 3 && cur_extpict)
        return false;
    // GB12/GB13: RI x RI when preceded by an odd number of RI
    if (a == UCB_UC_GCB_REGIONAL_INDICATOR && b == UCB_UC_GCB_REGIONAL_INDICATOR && st->ri_odd)
        return false;
    // GB999: Any ÷ Any
    return true;
}

// Step back to the start of the codepoint ending just before @p pos.
static const unsigned char* ucb_uc_prev_start(const unsigned char* base, const unsigned char* pos)
{
    pos--; // Last byte of the previous codepoint
    while (pos > base && (*pos & 0xC0u) == 0x80u)
        pos--;
    return pos;
}

// Decode the codepoint at @p start and return its property entry. If @p end is
// given, it receives the position after the codepoint.
static const ucb_uc_prop* ucb_uc_prop_at(const unsigned char* start, const unsigned char** end)
{
    const unsigned char* iter = start;
    ucb_cp cp = ucb_uc_next_valid(&iter);
    if (end)
        *end = iter;
    return ucb_uc_get_prop(cp);
}

// Reconstruct the scanner state for a valid cluster boundary at @p from.
static void ucb_uc_grapheme_state_at(const char* str, size_t from, ucb_uc_grapheme_state* st)
{
    memset(st, 0, sizeof(*st));
    if (from == 0)
        return;

    const unsigned char* base = (const unsigned char*)str;
    const unsigned char* pos = base + from;

    // GB12/GB13: parity of the trailing run of Regional_Indicator.
    {
        bool odd = false;
        const unsigned char* p = pos;
        while (p > base)
        {
            const unsigned char* q = ucb_uc_prev_start(base, p);
            const ucb_uc_prop* prop = ucb_uc_prop_at(q, UCB_NULL);
            if (!prop || ucb_uc_prop_gcb(prop) != UCB_UC_GCB_REGIONAL_INDICATOR)
                break;
            odd = !odd;
            p = q;
        }
        st->ri_odd = odd;
    }

    // GB11: does the sequence end with Extended_Pictographic Extend* ZWJ?
    {
        const unsigned char* p = pos;
        if (p > base)
        {
            const unsigned char* q = ucb_uc_prev_start(base, p);
            const ucb_uc_prop* prop = ucb_uc_prop_at(q, UCB_NULL);
            if (prop && ucb_uc_prop_gcb(prop) == UCB_UC_GCB_ZWJ)
            {
                p = q;
                while (p > base)
                {
                    q = ucb_uc_prev_start(base, p);
                    prop = ucb_uc_prop_at(q, UCB_NULL);
                    if (prop && ucb_uc_prop_extpict(prop))
                    {
                        st->gb11 = 3;
                        break;
                    }
                    if (!prop || ucb_uc_prop_gcb(prop) != UCB_UC_GCB_EXTEND)
                        break;
                    p = q;
                }
            }
        }
    }

    // GB9c: does the sequence end with Consonant [Extend Linker]* including a Linker?
    {
        bool linker = false;
        const unsigned char* p = pos;
        while (p > base)
        {
            const unsigned char* q = ucb_uc_prev_start(base, p);
            const ucb_uc_prop* prop = ucb_uc_prop_at(q, UCB_NULL);
            ucb_uc_incb incb = ucb_uc_prop_incb(prop);
            if (incb == UCB_UC_INCB_LINKER)
            {
                linker = true;
                p = q;
            }
            else if (incb == UCB_UC_INCB_EXTEND)
            {
                p = q;
            }
            else if (incb == UCB_UC_INCB_CONSONANT)
            {
                st->incb_seq = true;
                st->incb_linker = linker;
                break;
            }
            else
            {
                break;
            }
        }
    }
}

// Find the next cluster boundary at or after @p start. @p st must describe the
// position @p start, which itself must be a boundary.
static size_t ucb_uc_grapheme_next_from(const char* str,
                                        const unsigned char* start,
                                        const unsigned char* last,
                                        ucb_uc_grapheme_state* st)
{
    const unsigned char* iter = start;
    const ucb_uc_prop* prop = ucb_uc_prop_at(iter, &iter);
    UCB_VERIFY(prop, UCB_ERROR_INVALID_UTF8, "Invalid UTF-8");
    ucb_uc_grapheme_update(st,
                           ucb_uc_prop_gcb(prop),
                           ucb_uc_prop_extpict(prop),
                           ucb_uc_prop_incb(prop));

    while (iter < last)
    {
        const unsigned char* cp_start = iter;
        const ucb_uc_prop* next_prop = ucb_uc_prop_at(iter, &iter);
        UCB_VERIFY(next_prop, UCB_ERROR_INVALID_UTF8, "Invalid UTF-8");

        if (ucb_uc_grapheme_should_break(st,
                                         ucb_uc_prop_gcb(prop),
                                         ucb_uc_prop_gcb(next_prop),
                                         ucb_uc_prop_extpict(next_prop),
                                         ucb_uc_prop_incb(next_prop)))
            return (size_t)(cp_start - (const unsigned char*)str);

        ucb_uc_grapheme_update(st,
                               ucb_uc_prop_gcb(next_prop),
                               ucb_uc_prop_extpict(next_prop),
                               ucb_uc_prop_incb(next_prop));
        prop = next_prop;
    }
    return UCB_NPOS;
}

size_t ucb_uc_num_char(const char* str, size_t len)
{
    size_t num = 0;

    if (str)
    {
        if (len == UCB_NPOS)
            len = strlen(str);

        const unsigned char* iter = (const unsigned char*)str;
        const unsigned char* last = iter + len;
        if (iter < last)
        {
            ucb_uc_grapheme_state st = {0};
            const ucb_uc_prop* prop = ucb_uc_prop_at(iter, &iter);
            UCB_VERIFY(prop, UCB_ERROR_INVALID_UTF8, "Invalid UTF-8");
            ucb_uc_grapheme_update(&st,
                                   ucb_uc_prop_gcb(prop),
                                   ucb_uc_prop_extpict(prop),
                                   ucb_uc_prop_incb(prop));
            num = 1;

            while (iter < last)
            {
                const ucb_uc_prop* next_prop = ucb_uc_prop_at(iter, &iter);
                UCB_VERIFY(next_prop, UCB_ERROR_INVALID_UTF8, "Invalid UTF-8");
                if (ucb_uc_grapheme_should_break(&st,
                                                 ucb_uc_prop_gcb(prop),
                                                 ucb_uc_prop_gcb(next_prop),
                                                 ucb_uc_prop_extpict(next_prop),
                                                 ucb_uc_prop_incb(next_prop)))
                    num++;
                ucb_uc_grapheme_update(&st,
                                       ucb_uc_prop_gcb(next_prop),
                                       ucb_uc_prop_extpict(next_prop),
                                       ucb_uc_prop_incb(next_prop));
                prop = next_prop;
            }
        }
    }
    return num;
}

size_t ucb_uc_next_char(const char* str, size_t len, size_t from_byte)
{
    UCB_VERIFY_ARGS(str);

    if (from_byte >= len)
        return UCB_NPOS;

    ucb_uc_grapheme_state st;
    ucb_uc_grapheme_state_at(str, from_byte, &st);

    const unsigned char* start = (const unsigned char*)str + from_byte;
    const unsigned char* last = (const unsigned char*)str + len;
    return ucb_uc_grapheme_next_from(str, start, last, &st);
}

size_t ucb_uc_char_index(const char* str, size_t len, size_t index)
{
    if (!str || index == 0)
        return len;

    const unsigned char* base = (const unsigned char*)str;
    const unsigned char* iter = base;
    const unsigned char* last = base + len;

    if (iter >= last)
        return len;

    ucb_uc_grapheme_state st = {0};
    const ucb_uc_prop* prop = ucb_uc_prop_at(iter, &iter);
    if (!prop)
        return len;
    ucb_uc_grapheme_update(&st,
                           ucb_uc_prop_gcb(prop),
                           ucb_uc_prop_extpict(prop),
                           ucb_uc_prop_incb(prop));

    // `num` is the cluster currently being scanned (1-based). A break at the
    // start of the next cluster completes `num` and yields its end boundary.
    size_t num = 1;

    while (iter < last)
    {
        const unsigned char* cp_start = iter;
        const ucb_uc_prop* next_prop = ucb_uc_prop_at(iter, &iter);
        if (!next_prop)
            return len;

        if (ucb_uc_grapheme_should_break(&st,
                                         ucb_uc_prop_gcb(prop),
                                         ucb_uc_prop_gcb(next_prop),
                                         ucb_uc_prop_extpict(next_prop),
                                         ucb_uc_prop_incb(next_prop)))
        {
            if (num == index)
                return (size_t)(cp_start - base);
            num++;
        }

        ucb_uc_grapheme_update(&st,
                               ucb_uc_prop_gcb(next_prop),
                               ucb_uc_prop_extpict(next_prop),
                               ucb_uc_prop_incb(next_prop));
        prop = next_prop;
    }

    // The final cluster ends at the end of the string.
    return len;
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
static bool ucb_uc_is_word_separator(ucb_cp cp, const ucb_uc_prop* prop)
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
    ucb_cp buf[UCB_UC_MAX_MULTI_LEN];
    ucb_uc_case_op_t op;
    const ucb_uc_prop* last_prop;
    ucb_cp last_cp;
} casemap_ctx_t;

static bool ucb_uc_case_map_cp(casemap_ctx_t* ctx, ucb_error** perr)
{
    ctx->out_len = 1; // By default, keep value sent in
    if (!ctx->buf[0]) // Null terminator
        return true;

    const ucb_uc_prop* prop = ucb_uc_get_prop(ctx->buf[0]);

    if (!prop)
    {
        ucb_throw_format(perr,
                         UCB_ERROR_INVALID_CODEPOINT,
                         "Invalid codepoint: " PRIu32,
                         ctx->buf[0]);
    }

    const ucb_uc_mapping* mapping = ucb_uc_get_mapping(prop);
    if (!mapping)
    {
        ctx->last_cp = ctx->buf[0];
        ctx->last_prop = prop;
        return true;
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
    case UCB_UC_CASE_TITLE:
    {
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
        UCB_ASSERT_INTERNAL(false, "Invalid operation");
        return false;
    }

    // No more use for last prop
    ctx->last_prop = prop;
    ctx->last_cp = ctx->buf[0];

    if (!ref)
        return true;

    if (ref <= UCB_UC_NUM_SIMPLE_MAP)
    {
        int idx = ref - 1;
        ctx->buf[0] = s_ucb_uc_smap_table[idx].value;
    }
    else
    {
        int idx = ref - UCB_UC_NUM_SIMPLE_MAP - 1;
        UCB_ASSERT_INTERNAL(idx <= UCB_UC_NUM_MULTI_MAP, "Invalid mapping index");
        const ucb_uc_mmap* entry = &s_ucb_uc_mmap_table[idx];

        size_t len = 0;
        for (; len < UCB_UC_MAX_MULTI_LEN && entry->value[len] != UCB_UC_NO_VALUE; len++)
        {
            ctx->buf[len] = entry->value[len];
        }
        ctx->out_len = len;
    }
    return true;
}

/**
 * Instead of working on a string, work on a buffer object
 * Setting codepoint of different sizes + UTF-8 encoding may result in different string
 * lengths.
 */
static ucb_uc_result ucb_uc_case_map(const char* str,
                                     size_t size,
                                     ucb_uc_case_op_t op,
                                     ucb_buffer* dstbuf,
                                     ucb_error** perr)
{
    ucb_uc_result ret = {0};
    UCB_VERIFY_ARGS(str);

    ucb_cp cp;
    bool own_buffer = false;
    casemap_ctx_t ctx;
    ctx.last_cp = UCB_UC_NO_VALUE;
    ctx.last_prop = UCB_NULL;
    ctx.op = op;

    if (!dstbuf)
    {
        dstbuf = ucb_buffer_new_heap(size + 1);
        if (!dstbuf)
            return ret;
        dstbuf->grow_func = ucb_buffer_grow_double;
        own_buffer = true;
    }

    if (!ucb_buffer_ensure(dstbuf, size + 1))
    {
        ucb_throw(perr, UCB_ERROR_OUT_OF_MEMORY, "Failed to allocate buffer");
        return ret;
    }

    FOR_EACH_CODEPOINT(cp, str, size)
    {
        ctx.buf[0] = cp;

        if (!ucb_uc_case_map_cp(&ctx, perr))
            return ret;
        assert(ctx.out_len > 0 && ctx.out_len <= UCB_UC_MAX_MULTI_LEN);

        if (!ucb_uc_encode_codepoints(dstbuf, ctx.buf, ctx.out_len, perr))
            return ret;
    }

    if (!ucb_buffer_push(dstbuf, "\0", 1))
    {
        ucb_throw(perr, UCB_ERROR_OUT_OF_MEMORY, "Failed push null terminator");
        return ret;
    }

    if (own_buffer)
    {
        ucb_buffer_fit(dstbuf);
        ucb_buffer_transfer(dstbuf, (void**)&ret.data, &ret.size, UCB_NULL, UCB_NULL);
        ucb_buffer_free(dstbuf);
        ret.size -= 1; // Do not count null-terminator
    }
    else
    {
        ret.data = ucb_malloc_type(dstbuf->size, char);
        ret.size = dstbuf->size - 1;
        memcpy(ret.data, dstbuf->data, dstbuf->size);
    }
    return ret;
}

ucb_uc_result ucb_uc_to_upper(const char* str, size_t size, ucb_error** perr)
{
    return ucb_uc_case_map(str, size, UCB_UC_CASE_UPPER, UCB_NULL, perr);
}

ucb_uc_result ucb_uc_to_lower(const char* str, size_t size, ucb_error** perr)
{
    return ucb_uc_case_map(str, size, UCB_UC_CASE_LOWER, UCB_NULL, perr);
}

ucb_uc_result ucb_uc_to_title(const char* str, size_t size, ucb_error** perr)
{
    return ucb_uc_case_map(str, size, UCB_UC_CASE_TITLE, UCB_NULL, perr);
}

ucb_uc_result ucb_uc_casefold(const char* str, size_t size, ucb_error** perr)
{
    return ucb_uc_case_map(str, size, UCB_UC_CASE_FOLD, UCB_NULL, perr);
}

/* -------------------------------------------------------------------------- */
/*                                Normalization                               */
/* -------------------------------------------------------------------------- */

// Size of the on-stack canonical reordering window. Segments longer than this
// grow to the heap; a canonical segment (starter + non-starters) has no upper
// bound, so this must never be treated as a hard limit.
#define NORM_CTX_BUFSIZE 18
#define UC_FLAGS_COMPOSE 0x01
#define UC_FLAGS_COMPAT 0x02

typedef struct
{
    ucb_buffer cp; // Codepoint buffer
    ucb_cp cp_inline[NORM_CTX_BUFSIZE];
    uint8_t ccc_inline[NORM_CTX_BUFSIZE];
    ucb_cp* cp_buf;   // Reorder window, either cp_inline or a heap block
    uint8_t* ccc_buf; // Reorder window, either ccc_inline or a heap block
    size_t len;
    size_t cap;
    char* errpos;
    uint8_t flags;
} norm_ctx_t;

static void norm_ctx_init(norm_ctx_t* ctx)
{
    ctx->cp_buf = ctx->cp_inline;
    ctx->ccc_buf = ctx->ccc_inline;
    ctx->cap = NORM_CTX_BUFSIZE;
}

static void norm_ctx_destroy(norm_ctx_t* ctx)
{
    if (ctx->cp_buf != ctx->cp_inline)
        ucb_free(ctx->cp_buf);
    norm_ctx_init(ctx);
    ctx->len = 0;
}

// Slow path: grow the reorder window. Both arrays live in one allocation.
static bool norm_ctx_grow_slow(norm_ctx_t* ctx, size_t needed, ucb_error** perr)
{
    const size_t unit = sizeof(ucb_cp) + sizeof(uint8_t);
    size_t newcap = ctx->cap;
    while (newcap < needed && newcap <= SIZE_MAX / 2)
        newcap *= 2;
    if (newcap < needed)
        newcap = needed;
    if (newcap > SIZE_MAX / unit)
    {
        ucb_throw(perr, UCB_ERROR_OUT_OF_MEMORY, "Normalization segment too long");
        return false;
    }

    ucb_cp* mem = (ucb_cp*)ucb_malloc(newcap * unit);
    if (!mem)
    {
        ucb_throw(perr, UCB_ERROR_OUT_OF_MEMORY, "Could not grow normalization buffer");
        return false;
    }

    ucb_cp* new_cp = mem;
    uint8_t* new_ccc = (uint8_t*)(mem + newcap);
    if (ctx->len > 0)
    {
        memcpy(new_cp, ctx->cp_buf, ctx->len * sizeof(ucb_cp));
        memcpy(new_ccc, ctx->ccc_buf, ctx->len * sizeof(uint8_t));
    }
    if (ctx->cp_buf != ctx->cp_inline)
        ucb_free(ctx->cp_buf);

    ctx->cp_buf = new_cp;
    ctx->ccc_buf = new_ccc;
    ctx->cap = newcap;
    return true;
}

static inline bool norm_ctx_add(norm_ctx_t* ctx, ucb_cp cp, uint8_t ccc, ucb_error** perr)
{
    if (ctx->len >= ctx->cap && !norm_ctx_grow_slow(ctx, ctx->len + 1, perr))
        return false;

    ctx->cp_buf[ctx->len] = cp;
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
            ctx->cp_buf[i] = ctx->cp_buf[i - 1];
            ctx->ccc_buf[i] = ctx->ccc_buf[i - 1];
            ctx->cp_buf[i - 1] = cp;
            ctx->ccc_buf[i - 1] = ccc;
            i--;
        }
    }
    return true;
}

static bool norm_ctx_flush(norm_ctx_t* ctx, ucb_error** perr)
{
    bool ret = true;
    if (ctx->len > 0)
    {
        if (ctx->flags & UC_FLAGS_COMPOSE)
        {
            // Flush as codepoints
            ret = ucb_buffer_push(&ctx->cp, ctx->cp_buf, ctx->len * sizeof(ucb_cp));
            if (!ret)
            {
                ucb_throw(perr, UCB_ERROR_OUT_OF_MEMORY, "Could not push to internal buffer");
            }
        }
        else
        {
            // Flush as UTF-8
            ret = ucb_uc_encode_codepoints(&ctx->cp, ctx->cp_buf, ctx->len, perr);
        }
    }
    ctx->len = 0;
    return ret;
}

static inline bool is_hangul_syllable(ucb_cp c)
{
    return (c >= 0xAC00 && c <= 0xD7A3); // was 0xD7AF
}
static inline bool is_leading_jamo(ucb_cp c)
{
    return (c >= 0x1100 && c <= 0x1112);
}
static inline bool is_vowel_jamo(ucb_cp c)
{
    return (c >= 0x1161 && c <= 0x1175);
}
static inline bool is_trailing_jamo(ucb_cp c)
{
    return (c >= 0x11A8 && c <= 0x11C2);
}

static bool norm_decompose_hangul(norm_ctx_t* ctx, ucb_cp syllable, ucb_error** perr)
{
    assert(is_hangul_syllable(syllable));

    const ucb_cp S = syllable - 0xAC00;
    const ucb_cp T_idx1 = S % 28;
    const ucb_cp V_idx0 = ((S - T_idx1) % 588) / 28; // was = (S / 28) % 21;
    const ucb_cp L_idx0 = S / 588;                   // was = S / (28 * 21);

    if (!norm_ctx_add(ctx, 0x1100 + L_idx0, 0, perr) ||
        !norm_ctx_add(ctx, 0x1161 + V_idx0, 0, perr))
        return false;
    if (T_idx1 != 0 && !norm_ctx_add(ctx, 0x11A7 + T_idx1, 0, perr))
        return false;
    return true;
}

static inline ucb_cp compose_hangul(ucb_cp L, ucb_cp V, ucb_cp T)
{
    const ucb_cp L_idx0 = L - 0x1100;
    const ucb_cp V_idx0 = V - 0x1161;
    const ucb_cp T_idx1 = (T != 0) ? T - 0x11A7 : 0; // Note: SUBTRACT 0x11A8 - 1
    return 0xAC00 + (L_idx0 * 21 + V_idx0) * 28 + T_idx1;
}

static bool norm_decompose_cp(norm_ctx_t* ctx, ucb_cp cp, bool must_decomp, ucb_error** perr)
{
    uint8_t ccc = 0;
    const ucb_uc_prop* prop = ucb_uc_get_prop(cp);
    const ucb_uc_decomp* decomp = UCB_NULL;
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
        if (ccc == 0 && !norm_ctx_flush(ctx, perr))
            return false;
        // No decomposition or wrong type: append as-is
        return norm_ctx_add(ctx, cp, ccc, perr);
    }

    // Recursively decompose each codepoint
    bool ret = true;
    for (size_t i = 0; i < decomp->len && ret; i++)
    {
        ucb_cp next_cp = (decomp->len <= 2) ? decomp->vals[i] : decomp->ptr[i];
        ret = norm_decompose_cp(ctx, next_cp, must_decomp, perr);
    }
    return ret;
}

static ucb_cp norm_compose_cp(ucb_cp starter, ucb_cp combiner, const ucb_uc_combiners* combiners)
{
    if (!combiners)
        return 0;
#ifdef UCB_UC_WITH_VERIF
    UCB_UNUSED(starter);
    assert(starter == combiners->starter);
#endif
    const ucb_uc_combiner_entry* entries = combiners->entries;

    // Binary search for the combiner
    size_t left = 0;
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

static size_t norm_compose(ucb_cp* cps, size_t count)
{
    if (count < 2)
        return count; // Nothing to compose

    const ucb_uc_prop* prop;
    const ucb_uc_prop* prev_prop;
    const ucb_uc_combiners* combiners = UCB_NULL;
    // size_t last_starter_idx             = 0; // Only valid if combiners != NULL
    for (size_t i = 1; i < count; i++)
    {
        if (is_leading_jamo(cps[i - 1]) && is_vowel_jamo(cps[i]))
        {
            ucb_cp T = (i + 1 < count) ? cps[i + 1] : 0;
            if (!is_trailing_jamo(T))
                T = 0;

            cps[i - 1] = compose_hangul(cps[i - 1], cps[i], T);
            // Remove 1 or 2 codepoints and shift the rest
            size_t shift = (T != 0) ? 2 : 1;
            memmove(&cps[i], &cps[i + shift], (count - i - shift) * sizeof(ucb_cp));
            count -= shift;
            i--; // recheck same index
            continue;
        }

        // Seek back left to find last starting character
        // int due to possible overflow
        prop = ucb_uc_get_prop(cps[i]);
        for (int j = (int)i - 1; j >= 0; j--)
        {
            prev_prop = ucb_uc_get_prop(cps[j]);
            combiners = prev_prop ? ucb_uc_get_combiners(prev_prop) : UCB_NULL;
            ucb_cp composed = 0;
            if (combiners)
                composed = norm_compose_cp(cps[j], cps[i], combiners);
            if (composed != 0)
            {
                cps[j] = composed;
                // Remove current combiner and shift the rest
                memmove(&cps[i], &cps[i + 1], (count - i - 1) * sizeof(ucb_cp));
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

static bool normalize(norm_ctx_t* ctx,
                      const char* str,
                      size_t size,
                      bool must_decomp,
                      ucb_error** perr)
{
    ucb_cp cp;

    FOR_EACH_CODEPOINT(cp, str, size)
    {
        if (is_hangul_syllable(cp))
        {
            if (!norm_decompose_hangul(ctx, cp, perr) || !norm_ctx_flush(ctx, perr))
                return false;
        }
        else
        {
            if (!norm_decompose_cp(ctx, cp, must_decomp, perr))
                return false;

            // Only flush if last is a starter
            if (ctx->len > 0 && ctx->ccc_buf[ctx->len - 1] == 0)
            {
                if (!norm_ctx_flush(ctx, perr))
                    return false;
            }
        }
    }
    if (!norm_ctx_flush(ctx, perr))
        return false;

    if (ctx->flags & UC_FLAGS_COMPOSE)
    {
        ucb_cp* cps = (ucb_cp*)ctx->cp.data;
        size_t count = ctx->cp.size / sizeof(ucb_cp);

        count = norm_compose(cps, count);

        ucb_buffer_clear(&ctx->cp);
        if (!ucb_uc_encode_codepoints(&ctx->cp, cps, count, perr))
            return false;
    }

    return ucb_buffer_push(&ctx->cp, "\0", 1);
}

/**
 * @brief Quick check whether a string is already in the requested normalization form.
 *
 * The result is conservative: it only returns true when the string is guaranteed to be
 * in the requested form. A false result means full normalization is required.
 *
 * This mirrors the Unicode quick check
 * (https://unicode.org/reports/tr15/#Detecting_Normalization_Forms) using data we already carry in
 * the property table:
 *   - NFD:  no canonical decomposition and no Hangul syllable.
 *   - NFKD: no decomposition at all and no Hangul syllable.
 *   - NFC:  no composition exclusion (NFC_QC=No) and no code point that can compose
 *           with a preceding starter (NFC_QC=Maybe).
 *   - NFKC: no code point that is not in NFKC form (NFKC_QC=No) and no code point that
 *           can compose with a preceding starter (NFKC_QC=Maybe).
 * Canonical ordering is verified for all forms.
 */
static bool ucb_uc_is_normalized(const char* str, size_t size, ucb_norm_form form)
{
    const bool decomp = (form == UCB_NORM_NFD || form == UCB_NORM_NFKD);
    const bool compat = (form == UCB_NORM_NFKD || form == UCB_NORM_NFKC);

    uint8_t last_ccc = 0;
    ucb_cp cp;

    FOR_EACH_CODEPOINT(cp, str, size)
    {
        if (is_hangul_syllable(cp))
        {
            // Hangul syllables are already composed (NFC/NFKC) but always decompose.
            if (decomp)
                return false;
            last_ccc = 0; // Starter
            continue;
        }

        const ucb_uc_prop* prop = ucb_uc_get_prop(cp);
        if (!prop)
            return false;

        if (decomp)
        {
            if (prop->decomp_idx)
            {
                if (compat)
                    return false;
                const ucb_uc_decomp* d = ucb_uc_get_decomp(prop);
                if (d && d->type == UCB_UC_DC_CANON)
                    return false;
            }
        }
        else
        {
            // NFC/NFKC quick check: reject NFC_QC/NFKC_QC = No and Maybe.
            uint8_t reject = compat ? UCB_UC_PROP_NFKCNO : UCB_UC_PROP_COMPEXCL;
            if (prop->flags & (reject | UCB_UC_PROP_COMPMAYBE))
                return false;
            // Hangul vowel/trailing jamo compose algorithmically
            if (is_vowel_jamo(cp) || is_trailing_jamo(cp))
                return false;
        }

        // Canonical ordering: combining classes must be non-decreasing.
        uint8_t ccc = prop->ccc;
        if (ccc != 0 && ccc < last_ccc)
            return false;
        last_ccc = ccc;
    }
    FOR_EACH_CODEPOINT_CHECK_RET();

    return true;
}

static ucb_uc_result normalize_common(const char* str,
                                      size_t size,
                                      ucb_norm_form form,
                                      bool quick,
                                      ucb_error** perr)
{
    norm_ctx_t ctx = {0};
    ucb_uc_result ret = {0};

    switch (form)
    {
    case UCB_NORM_NFD:
        break;
    case UCB_NORM_NFKD:
        ctx.flags |= UC_FLAGS_COMPAT;
        break;
    case UCB_NORM_NFC:
        ctx.flags |= UC_FLAGS_COMPOSE;
        break;
    case UCB_NORM_NFKC:
        ctx.flags |= UC_FLAGS_COMPAT | UC_FLAGS_COMPOSE;
        break;
    case UCB_NORM_INVALID:
    default:
        UCB_VERIFY_ARGS(false);
    }

    UCB_VERIFY_ARGS(str);

    if (size == UCB_NPOS)
        size = strlen(str);

    // Quick path: the string is already in the requested form, just copy it.
    if (quick && ucb_uc_is_normalized(str, size, form))
    {
        ret.data = ucb_malloc_type(size + 1, char);
        if (!ret.data)
        {
            ucb_throw(perr, UCB_ERROR_OUT_OF_MEMORY, "Failed to allocate buffer");
            return ret;
        }
        memcpy(ret.data, str, size);
        ret.data[size] = '\0';
        ret.size = size;
        return ret;
    }

    // Find out how many codepoints we deal with for the worst case allocation
    size_t numcp = ucb_uc_num_cp(str, size);

    // Allocate for the worst case decomposition of the input
    if (!ucb_buffer_init_heap(&ctx.cp, 4 * numcp * sizeof(ucb_cp) + 1))
    {
        ucb_throw(perr, UCB_ERROR_OUT_OF_MEMORY, "Failed to allocate buffer");
        return ret;
    }
    ctx.cp.grow_func = ucb_buffer_grow_double;
    norm_ctx_init(&ctx);

    if (!normalize(&ctx, str, size, true, perr))
    {
        norm_ctx_destroy(&ctx);
        ucb_buffer_release(&ctx.cp);
        return ret;
    }

    norm_ctx_destroy(&ctx);
    ucb_buffer_fit(&ctx.cp);
    ucb_buffer_transfer(&ctx.cp, (void**)&ret.data, &ret.size, UCB_NULL, UCB_NULL);
    ucb_buffer_release(&ctx.cp);
    ret.size -= 1; // Do not count null-terminator
    return ret;
}

ucb_uc_result ucb_uc_normalize(const char* str, size_t size, ucb_norm_form form, ucb_error** perr)
{
    return normalize_common(str, size, form, true, perr);
}

ucb_uc_result ucb_uc_normalize_full(const char* str,
                                    size_t size,
                                    ucb_norm_form form,
                                    ucb_error** perr)
{
    return normalize_common(str, size, form, false, perr);
}

// Streaming case-folding iterator used for case-insensitive comparison. A
// single input codepoint can fold into several codepoints, so the folded
// overflow is buffered until it has been consumed.
typedef struct ucb_uc_fold_iter
{
    const unsigned char* pos;
    const unsigned char* end;
    casemap_ctx_t ctx;
    ucb_cp pending[UCB_UC_MAX_MULTI_LEN];
    size_t pending_pos;
    size_t pending_len;
} ucb_uc_fold_iter;

static void ucb_uc_fold_iter_init(ucb_uc_fold_iter* iter, const char* str, size_t len)
{
    iter->pos = (const unsigned char*)str;
    iter->end = iter->pos + len;
    iter->ctx.last_cp = UCB_UC_NO_VALUE;
    iter->ctx.last_prop = UCB_NULL;
    iter->ctx.op = UCB_UC_CASE_FOLD;
    iter->pending_pos = 0;
    iter->pending_len = 0;
}

// Get the next folded codepoint. Returns false when the input is exhausted, or
// on error (which is reported).
static bool ucb_uc_fold_iter_next(ucb_uc_fold_iter* iter, ucb_cp* out)
{
    if (iter->pending_pos < iter->pending_len)
    {
        *out = iter->pending[iter->pending_pos++];
        return true;
    }
    if (iter->pos >= iter->end)
        return false;

    iter->ctx.buf[0] = ucb_uc_next_valid(&iter->pos);

    ucb_error* err = UCB_NULL;
    if (!ucb_uc_case_map_cp(&iter->ctx, &err))
    {
        UCB_REPORT_ERROR(err); // Aborts
    }

    assert(iter->ctx.out_len > 0 && iter->ctx.out_len <= UCB_UC_MAX_MULTI_LEN);
    iter->pending_len = iter->ctx.out_len;
    for (size_t i = 0; i < iter->pending_len; i++)
        iter->pending[i] = iter->ctx.buf[i];
    iter->pending_pos = 0;

    *out = iter->pending[iter->pending_pos++];
    return true;
}

int ucb_uc_icomp(const char* str1, size_t len1, const char* str2, size_t len2)
{
    UCB_VERIFY_ARGS(str1 && str2);

    // Early out if the strings are identical
    if (str1 == str2)
    {
        UCB_VERIFY_ARGS(len1 == len2);
        return 0;
    }

    ucb_uc_fold_iter iter1;
    ucb_uc_fold_iter iter2;
    ucb_uc_fold_iter_init(&iter1, str1, len1);
    ucb_uc_fold_iter_init(&iter2, str2, len2);

    for (;;)
    {
        ucb_cp c1 = 0;
        ucb_cp c2 = 0;
        bool has1 = ucb_uc_fold_iter_next(&iter1, &c1);
        bool has2 = ucb_uc_fold_iter_next(&iter2, &c2);

        if (!has1 && !has2)
            return 0;
        if (!has1)
            return -1;
        if (!has2)
            return 1;

        if (c1 != c2)
            return UCB_COMP(c1, c2);
    }
}
