#pragma once

/**
 * @file envmap.h
 * This file is part of the UCB project
 * - SPDX-FileCopyrightText: © 2026 Åke Svedin <ake@svedin.org>
 * - SPDX-License-Identifier: MIT
 *
 * @brief Environment map
 *
 * All strings are UTF-8 encoded.
 */

#include <ucb/export.h>

#include <stdbool.h>
#include <stddef.h>

/**
 * @brief Environment map structure
 *
 * Holds name-value pairs representing environment variables.
 */
struct ucb_envmap;
typedef struct ucb_envmap ucb_envmap;

/**
 * @brief Create a new environment map
 * @return Pointer to a new empty environment map, or UCB_NULL on failure
 */
UCB_API ucb_envmap* ucb_envmap_new(void);

/**
 * @brief Free an environment map
 * @param map The environment map to free. May be UCB_NULL.
 */
UCB_API void ucb_envmap_free(ucb_envmap* map);

/**
 * @brief Initialize an environment map with current environment variables
 * @param map The environment map to initialize
 */
UCB_API void ucb_envmap_init_from_current(ucb_envmap* map);

/**
 * @brief Reserve space for a number of entries in the environment map
 * @param map The environment map
 * @param count Number of entries to reserve space for
 */
UCB_API void ucb_envmap_reserve(ucb_envmap* map, size_t count);

/**
 * @brief Clone an environment map
 * @param src The source environment map to clone
 * @return A new copy of the environment map, or UCB_NULL on failure
 */
UCB_API ucb_envmap* ucb_envmap_clone(const ucb_envmap* src);

/**
 * @brief Copy an environment map to another
 * @param dst The destination environment map
 * @param src The source environment map to copy from
 */
UCB_API void ucb_envmap_copy(ucb_envmap* dst, const ucb_envmap* src);

/**
 * @brief Check if an environment variable exists in the map
 * @param map The environment map
 * @param name Variable name
 * @return true if the variable exists in the map
 */
UCB_API bool ucb_envmap_has(const ucb_envmap* map, const char* name);

/**
 * @brief Get an environment variable value from the map
 *
 * The returned string refers to internal memory and should not be freed.
 * It is only valid as long as the map is not modified.
 *
 * @param map The environment map
 * @param name Variable name
 * @return Variable value, or UCB_NULL if not found
 */
UCB_API const char* ucb_envmap_get(const ucb_envmap* map, const char* name);

/**
 * @brief Set an environment variable value in the map
 * @param map The environment map
 * @param name Variable name
 * @param value Value to set
 * @return true if the variable was set
 */
UCB_API bool ucb_envmap_set(ucb_envmap* map, const char* name, const char* value);

/**
 * @brief Unset an environment variable from the map
 * @param map The environment map
 * @param name Variable name
 * @return true if the variable was unset
 */
UCB_API bool ucb_envmap_unset(ucb_envmap* map, const char* name);

/**
 * @brief Append a string to an environment variable in the map
 *
 * If the variable does not exist, it will be created without a separator.
 * @param map The environment map
 * @param name Variable name
 * @param value String to append after existing value
 * @param sep Optional separator between values (NULL okay)
 * @return true if the variable was set
 */
UCB_API bool ucb_envmap_append(ucb_envmap* map,
                               const char* name,
                               const char* value,
                               const char* sep);

/**
 * @brief Prepend a string to an environment variable in the map
 *
 * If the variable does not exist, it will be created without a separator.
 * @param map The environment map
 * @param name Variable name
 * @param value String to prepend before existing value
 * @param sep Optional separator between values (NULL okay)
 * @return true if the variable was set
 */
UCB_API bool ucb_envmap_prepend(ucb_envmap* map,
                                const char* name,
                                const char* value,
                                const char* sep);

/**
 * @brief Apply all environment variables from the map to the current process environment
 * @param map The environment map to apply
 */
UCB_API void ucb_envmap_apply(const ucb_envmap* map);
