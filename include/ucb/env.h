#pragma once

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

#include <ucb/export.h>

#include <stdbool.h>

/**
 * @brief Check if environment variable exists
 * @param name Variable name
 * @return true if variable exist
 */
UCB_API bool ucb_env_has(const char* name);

/**
 * @brief Get environment variable value
 *
 * The returned string is referring to internal memory and should not be freed.
 * It is only valid until the next call to this function.
 *
 * Use ucb_env_getdup, to get a copy of the value.
 *
 * @param name Variable name
 * @return Variable value, or UCB_NULL if not found
 */
UCB_API const char* ucb_env_get(const char* name);

/**
 * @brief Set an environment variable value
 * @param name Variable name
 * @param value Value to set
 * @param overwrite If true, will overwrite any existing value, otherwise leave it unchanged.
 * @return true if the variable was set
 */
UCB_API bool ucb_env_set(const char* name, const char* value, bool overwrite);

/**
 * @brief Unset an environment variable
 * @param name Variable name
 * @return true if the variable was unset
 */
UCB_API bool ucb_env_unset(const char* name);

/**
 * @brief Append a string to environment variable
 *
 * If the variable does not exist, it will be created, without separator.
 * @param name Variable name
 * @param value String to append before existing value
 * @param sep Optional separator between values (NULL okay)
 * @return true if the variable was set
 */
UCB_API bool ucb_env_append(const char* name, const char* value, const char* sep);

/**
 * @brief Prepend a string to environment variable
 *
 * If the variable does not exist, it will be created, without separator.
 * @param name Variable name
 * @param value String to prepend before existing value
 * @param sep Optional separator between values (NULL okay)
 * @return true if the variable was set
 */
UCB_API bool ucb_env_prepend(const char* name, const char* value, const char* sep);
