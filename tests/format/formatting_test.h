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

namespace test {
namespace format {

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

void TestFailureFixture::error_collect(ucb_errlvl lvl, const ucb_error* e)
{
    switch (lvl)
    {
    case UCB_ERRLVL_ERROR:
        num_error++;
        break;
    case UCB_ERRLVL_ABORT:
        num_aborts++;
        break;
    default:
        break;
    }

    if (false)
        (void)e;
}

extern "C" {
void TestFailureFixture::abort_handler(int)
{
    longjmp(FailureSetJump, 1);
}
}

template <typename Func>
void long_signature(TestFailureFixture& fixture,
                    Func func,
                    const bool args1 = false,
                    const bool args2 = false,
                    const bool args3 = false,
                    const bool args4 = false,
                    const bool args5 = false)
{
    if (args1)
        func(1);
    if (args2)
        func(2);
    if (args3)
        func(3);
}

void empty_func(int)
{
}

} // namespace format
} // namespace test

#endif // TESTS_COMMON_H
