/**
 * @file ucb.h
 *
 * This file is part of the UCB project
 * - SPDX-FileCopyrightText: © 2026 Åke Svedin <ake@svedin.org>
 * - SPDX-License-Identifier: MIT
 *
 * @brief UCB Core API
 */

#ifndef UCB_UCB_H
#define UCB_UCB_H

#include <ucb/export.h>
#include <ucb/version.h>

UCB_API const char* ucb_get_version(void);

/**
 * @brief Initiate console to use UTF-8
 */
UCB_API void ucb_init_console(void);

/**
 * @struct ucb_config
 *
 * @brief Configuration for the library.
 *
 * This is a thread-local configuration and can be set per-thread. This is used for settings that
 * are expected to be global for a thread, such as the Unicode policy.
 *
 * New ucb_threads will inherit the configuration. Other thread implementations must manually
 * copy the configuration if needed. This can be done with @ref ucb_conf_get() and @ref
 * ucb_conf_set().
 */
typedef struct ucb_config
{
    int _placeholder; // Add actual configuration when needed
} ucb_config;

/**
 * @brief Get the thread-local configuration for the library.
 * @return the thread-local configuration
 */
UCB_API ucb_config ucb_conf_get(void);

/**
 * @brief Set the thread-local configuration for the library.
 * @param config a config or UCB_NULL to reset to default configuration
 */
UCB_API void ucb_conf_set(const ucb_config* config);

#endif // UCB_UCB_H
