/**
 * @file test_vector.h
 * This file is part of the UCB project
 * - SPDX-FileCopyrightText: © 2026 Åke Svedin <ake@svedin.org>
 * - SPDX-License-Identifier: MIT
 *
 * @brief C test functions for vector
 */

#pragma once

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

int64_t test_carr_direct(int iterations, int n_elem);
int64_t test_carr_iter(int iterations, int n_elem);

int64_t test_vector1_direct(int iterations, int n_elem);
int64_t test_vector1_foreach(int iterations, int n_elem);

int64_t test_vector_ptr_direct(int iterations, int n_elem);
int64_t test_vector_ptr_foreach(int iterations, int n_elem);

int64_t test_vector_int_direct(int iterations, int n_elem);
int64_t test_vector_int_foreach(int iterations, int n_elem);

#ifdef __cplusplus
}
#endif
