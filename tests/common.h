/**
 * This file is part of the UCB project
 * SPDX-FileCopyrightText: © 2026 Åke Svedin <ake@svedin.org>
 * SPDX-License-Identifier: MIT
 *
 * @brief Testing common utility
 */

#ifndef TESTS_COMMON_H
#define TESTS_COMMON_H

#include "ucb/errcodes.h"
#include "ucb/error.h"

#include <iostream>
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

static thread_local TestFailureFixture* s_fixture = nullptr;

/**
 * Catches errors without failing so we can validate invalid arguments etc
 */

TestFailureFixture::TestFailureFixture() : m_prev_func(ucb_error_set_func(error_collect))
{
    s_fixture = this;
    num_error = 0;
}

TestFailureFixture::~TestFailureFixture()
{
    ucb_error_set_func(m_prev_func);
    s_fixture = nullptr;
}

void TestFailureFixture::error_collect(ucb_errlvl lvl, const ucb_error* e)
{
    std::cerr << "Caught expected error: ";
    ucb_error_print(lvl, e);
    s_fixture->errors.push_back({lvl, e->code});
    s_fixture->num_error++;
}

#endif // TESTS_COMMON_H
