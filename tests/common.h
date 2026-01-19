/**
 * @file common.h
 *
 * This file is part of the UCB project
 * - SPDX-FileCopyrightText: © 2026 Åke Svedin <ake@svedin.org>
 * - SPDX-License-Identifier: MIT
 *
 * @brief comment test utilities
 */

#ifndef TESTS_COMMON_H
#define TESTS_COMMON_H

#include "ucb/errcodes.h"
#include "ucb/error.h"

#include <vector>

struct error_report
{
    ucb_errlvl lvl;
    ucb_ecode code;
};

/**
 * Catches errors without failing so we can validate invalid arguments etc
 */
class TestFailureFixture
{
public:
    TestFailureFixture();
    ~TestFailureFixture();

    std::vector<error_report> errors;
    int num_error;

private:
    static void error_collect(ucb_errlvl lvl, const ucb_error* e);
    ucb_error_func m_prev_func;
};

#endif // TESTS_COMMON_H
