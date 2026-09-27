/**
 * @file envmap_private.h
 *
 * This file is part of the UCB project
 * - SPDX-FileCopyrightText: © 2026 Åke Svedin <ake@svedin.org>
 * - SPDX-License-Identifier: MIT
 *
 * @brief Private environment map helpers
 */

#ifndef UCB_ENVMAP_PRIVATE_H
#define UCB_ENVMAP_PRIVATE_H

#include <ucb/envmap.h>
#include <ucb/error.h>

/**
 * @brief Build a NULL-terminated "NAME=VALUE" array from a map
 *
 * The map is opaque and has no public iteration API, so this helper lives next
 * to the implementation that knows the internal entry layout. Entries are
 * emitted in map order and an empty value still produces a "NAME=" entry, since
 * presence is expressed by membership.
 *
 * The returned array and every entry are allocated with @ref ucb_malloc and must
 * be released with @ref ucb_envmap_envp_free.
 *
 * @param map the environment map, must be non-NULL
 * @param perr optional location to store the error on failure
 * @return a NULL-terminated array, or UCB_NULL on failure
 */
char** ucb_envmap_to_envp(const ucb_envmap* map, ucb_error** perr);

/**
 * @brief Release an array returned by @ref ucb_envmap_to_envp
 * @param envp the array, may be UCB_NULL
 */
void ucb_envmap_envp_free(char** envp);

#endif // UCB_ENVMAP_PRIVATE_H
