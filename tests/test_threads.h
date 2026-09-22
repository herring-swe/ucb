/**
 * @file test_threads.h
 *
 * This file is part of the UCB project
 * - SPDX-FileCopyrightText: © 2026 Åke Svedin <ake@svedin.org>
 * - SPDX-License-Identifier: MIT
 *
 * @brief C threads API testing
 */

#ifndef TESTS_TEST_THREADS_H
#define TESTS_TEST_THREADS_H

#ifdef __cplusplus
extern "C" {
#endif

/**
 * Exercise the public threads API from C, including stack initialization.
 * @return 1 on success, 0 on failure
 */
int test_c_threads(void);

#ifdef __cplusplus
}
#endif

#endif // TESTS_TEST_THREADS_H
