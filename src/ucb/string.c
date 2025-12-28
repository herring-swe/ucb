/**
 * This file is part of the UCB project
 * SPDX-FileCopyrightText: 2025 Åke Svedin <ake@svedin.org>
 * SPDX-License-Identifier: MIT
 *
 * @brief
 */

#include "ucb/string.h"

#include "ucb/cstring.h"
#include "ucb/defines.h"
#include "ucb/errcodes.h"
#include "ucb/error.h"
#include "ucb/memory.h"
#include "ucb/string_private.h"

#include <limits.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

ucb_str_t* ucb_str_new(const char* cstr, size_t len, uint32_t flags)
{
    if (!cstr)
    {
        return NULL;
    }

    ucb_str_t* str = ucb_calloc_type(1, ucb_str_t);
    if (flags & UCB_STR_SHALLOW)
    {
        str->data = (char*)cstr; // NOLINT (const cast)
    }
    else
    {
        // TODO: Verify len and utf-8
        str->data = ucb_malloc_type(len + 1, char);
        memcpy(str->data, cstr, len);
        str->data[len] = '\0';
    }
    str->size  = len;
    str->flags = flags;
    return str;
}

void ucb_str_free(ucb_str_t* str)
{
    if (str->flags & UCB_STR_SHALLOW && str->data)
        ucb_free(str->data);
    ucb_free(str);
}

bool ucb_str_is_shallow(const ucb_str_t* str)
{
    return (str->flags & UCB_STR_SHALLOW) == UCB_STR_SHALLOW;
}

const char* ucb_str_cstr(const ucb_str_t* str)
{
    if (!str)
        return UCB_NULL;
    return str->data;
}

size_t ucb_str_size(const ucb_str_t* str)
{
    if (!str)
    {
        return 0;
    }
    return 0;
}

// size_t ucb_str_len(const ucb_str_t* str)
// {

// }

// size_t ucb_str_num_chars(const ucb_str_t* str)
// {

// }

// ucb_str_t* ucb_str_copy(const ucb_str_t* str)
// {

// }

// ucb_str_t* ucb_str_concat(const ucb_str_t* str1, const ucb_str_t* str2)
// {

// }

// ucb_str_t* ucb_str_substr(const ucb_str_t* str, size_t index, size_t count, int flags)
// {

// }
