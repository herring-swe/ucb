/**
 * @file env.h
 * This file is part of the UCB project
 * - SPDX-FileCopyrightText: © 2026 Åke Svedin <ake@svedin.org>
 * - SPDX-License-Identifier: MIT
 *
 * @brief Environment variables
 *
 * All strings are UTF-8 encoded.
 */

#ifndef UCB_ENV_H
#define UCB_ENV_H

#include <ucb/export.h>
#include <ucb/string.h>

#include <stdbool.h>

/**
 * @brief Check if environment variable exists
 * @param name Variable name
 * @return true if variable exist
 */
UCB_API bool ucb_env_has(const ucb_str* name);

/**
 * @brief Get environment variable value
 *
 * The returned string is owned by the caller and must be released with
 * @ref ucb_str_free. It is an independent copy, so it is safe to keep and to
 * call this function again while holding it.
 *
 * @param name Variable name
 * @return owned variable value, or UCB_NULL if not found or on allocation error
 */
UCB_API ucb_str* ucb_env_get(const ucb_str* name);

/**
 * @brief Set an environment variable value
 * @param name Variable name
 * @param value Value to set
 * @param overwrite If true, will overwrite any existing value, otherwise leave it unchanged.
 * @return true if the variable was set
 */
UCB_API bool ucb_env_set(const ucb_str* name, const ucb_str* value, bool overwrite);

/**
 * @brief Unset an environment variable
 * @param name Variable name
 * @return true if the variable was unset
 */
UCB_API bool ucb_env_unset(const ucb_str* name);

/**
 * @brief Append a string to environment variable
 *
 * If the variable does not exist, it will be created, without separator.
 * @param name Variable name
 * @param value String to append before existing value
 * @param sep Optional separator between values (NULL okay)
 * @return true if the variable was set
 */
UCB_API bool ucb_env_append(const ucb_str* name, const ucb_str* value, const ucb_str* sep);

/**
 * @brief Prepend a string to environment variable
 *
 * If the variable does not exist, it will be created, without separator.
 * @param name Variable name
 * @param value String to prepend before existing value
 * @param sep Optional separator between values (NULL okay)
 * @return true if the variable was set
 */
UCB_API bool ucb_env_prepend(const ucb_str* name, const ucb_str* value, const ucb_str* sep);

#endif // UCB_ENV_H
