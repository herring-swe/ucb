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

#ifndef _WIN32
#include <signal.h>
#endif

struct error_report
{
    ucb_errlvl lvl;
    ucb_ecode code;
};

#ifdef _WIN32
extern jmp_buf FailureSetJump;
#else
extern sigjmp_buf FailureSetJump;
#endif

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
    static void abort_handler(int);

private:
    static void error_collect(ucb_errlvl lvl, const ucb_error* e);
    ucb_error_func m_prev_func;
};

#ifdef _WIN32
#define CHECK_ABORTS(expr)                                  \
    do                                                      \
    {                                                       \
        UCB_DIAG_PUSH()                                     \
        UCB_DIAG_MSVC_IGN(4611)                             \
        auto ucb_prev = std::signal(SIGABRT, abort_handler);\
        int prev_aborts = num_aborts;                       \
        if (setjmp(FailureSetJump) == 0)                    \
        {                                                   \
            expr;                                           \
        }                                                   \
        std::signal(SIGABRT, ucb_prev);                     \
        if (prev_aborts + 1 != num_aborts)                  \
        {                                                   \
            FAIL("Expected abort but none occurred");       \
        }                                                   \
        UCB_DIAG_POP()                                      \
    } while (0)
#else
#define CHECK_ABORTS(expr)                                  \
    do                                                      \
    {                                                       \
        UCB_DIAG_PUSH()                                     \
        struct sigaction ucb_sa      = {};                  \
        struct sigaction ucb_prev_sa = {};                  \
        ucb_sa.sa_handler            = abort_handler;       \
        sigemptyset(&ucb_sa.sa_mask);                       \
        ucb_sa.sa_flags              = 0;                   \
        sigaction(SIGABRT, &ucb_sa, &ucb_prev_sa);          \
        int prev_aborts = num_aborts;                       \
        if (sigsetjmp(FailureSetJump, 1) == 0)              \
        {                                                   \
            expr;                                           \
        }                                                   \
        sigaction(SIGABRT, &ucb_prev_sa, nullptr);          \
        if (prev_aborts + 1 != num_aborts)                  \
        {                                                   \
            FAIL("Expected abort but none occurred");       \
        }                                                   \
        UCB_DIAG_POP()                                      \
    } while (0)
#endif

#endif // TESTS_COMMON_H
