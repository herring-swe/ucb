/**
 * @file unicode_enum.c
 * This file is part of the UCB project
 * - SPDX-FileCopyrightText: © 2026 Åke Svedin <ake@svedin.org>
 * - SPDX-License-Identifier: MIT
 *
 * @brief Unicode enum implementation
 */

#include "ucb/unicode_enum.h"

#include "ucb/cstring.h"

const char* ucb_uc_norm_form_to_str(ucb_norm_form form)
{
    switch (form)
    {
    case UCB_NORM_NFC:
        return "NFC";
    case UCB_NORM_NFD:
        return "NFD";
    case UCB_NORM_NFKC:
        return "NFKC";
    case UCB_NORM_NFKD:
        return "NFKD";
    case UCB_NORM_INVALID:
        break;
    default:
        break;
    }
    return "";
}

ucb_norm_form ucb_uc_norm_form_from_str(const char* str)
{
    if (ucb_cstr_icomp(str, "NFC") == 0)
        return UCB_NORM_NFC;
    else if (ucb_cstr_icomp(str, "NFD") == 0)
        return UCB_NORM_NFD;
    else if (ucb_cstr_icomp(str, "NFKC") == 0)
        return UCB_NORM_NFKC;
    else if (ucb_cstr_icomp(str, "NFKD") == 0)
        return UCB_NORM_NFKD;
    else
        return UCB_NORM_INVALID;
}
