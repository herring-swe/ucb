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

#include "ucb/diag.h"
#include "ucb/errcodes.h"
#include "ucb/error.h"

#include <csetjmp>
#include <csignal>
#include <vector>

struct error_report
{
    ucb_errlvl lvl;
    ucb_ecode code;
};

extern jmp_buf FailureSetJump;

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
    int num_aborts;

protected:
    static void TestFailureFixture::abort_handler(int);

private:
    static void error_collect(ucb_errlvl lvl, const ucb_error* e);
    ucb_error_func m_prev_func;
};

#define CHECK_ABORTS(expr)                                \
    do                                                    \
    {                                                     \
        UCB_DIAG_PUSH()                                   \
        UCB_DIAG_MSVC_IGN(4611)                           \
        auto prev = std::signal(SIGABRT, abort_handler);  \
        if (setjmp(FailureSetJump) == 0)                  \
        {                                                 \
            int prev_aborts = num_aborts;                 \
            expr;                                         \
            if (prev_aborts + 1 != num_aborts)            \
            {                                             \
                FAIL("Expected abort but none occurred"); \
            }                                             \
        }                                                 \
        std::signal(SIGABRT, prev);                       \
        UCB_DIAG_POP()                                    \
    } while (0)

#endif // TESTS_COMMON_H
