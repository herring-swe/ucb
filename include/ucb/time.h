/**
 * @file time.h
 * 
 * This file is part of the UCB project
 * - SPDX-FileCopyrightText: © 2026 Åke Svedin <ake@svedin.org>
 * - SPDX-License-Identifier: MIT
 * 
 * @brief Time handling utilities
 */

#ifndef UCB_TIME_H
#define UCB_TIME_H

#include "export.h"

#include <stdint.h>

/**
 * @brief Time in seconds
 */
typedef int64_t ucb_time;
typedef int64_t ucb_time_ms;

/**
 * @brief Short time
 * Used when underlying functions only support 32-bit values
 */
typedef int32_t ucb_stime;
typedef int32_t ucb_stime_ms;

/**
 * Sleep for a given duration in seconds
 * Ignores any interruptions.
 * Duration must be less than INT32_MAX / 1000 seconds (~24 days)
 * @param dur time in seconds
 */
UCB_API void ucb_sleep(ucb_stime dur);

/**
 * Sleep for a given duration in milliseconds
 * Ignores any interruptions.
 * Duration must be less than INT32_MAX milliseconds (~24 days)
 * @param dur time in milliseconds
 */
UCB_API void ucb_sleep_ms(ucb_stime_ms dur);


#endif // UCB_TIME_H
