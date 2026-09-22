/**
 * @file unicode_private.h
 *
 * This file is part of the UCB project
 * - SPDX-FileCopyrightText: © 2026 Åke Svedin <ake@svedin.org>
 * - SPDX-License-Identifier: MIT
 *
 * @brief Unicode internal helpers
 */

#ifndef UCB_UNICODE_PRIVATE_H
#define UCB_UNICODE_PRIVATE_H

#include <ucb/export.h>
#include <ucb/unicode.h>

/**
 * @brief Normalize an UTF-8 string, skipping the quick check fast path.
 *
 * Internal/benchmark-only variant of ucb_uc_normalize() that always runs the
 * full decomposition/composition algorithm. Used to measure the quick path.
 * Not part of the public, installed API.
 */
UCB_API ucb_uc_result ucb_uc_normalize_full(const char* str,
                                            size_t len,
                                            ucb_norm_form form,
                                            ucb_error** perr);

#endif // UCB_UNICODE_PRIVATE_H