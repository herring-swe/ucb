/**
 * This file is part of the UCB project
 * SPDX-FileCopyrightText: © 2025 Åke Svedin <ake@svedin.org>
 * SPDX-License-Identifier: MIT
 */

// #define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include <ucb/diag.h>

#define DOCTEST_CONFIG_IMPLEMENT
UCB_DIAG_PUSH
UCB_DIAG_IGN_NRVO
#include "doctest.h"
UCB_DIAG_POP

#include <ucb/diag.h>
#include <ucb/error.h>
#include <ucb/memdbg.h>
#include <ucb/ucb.h>

static void testing_errfunc(ucb_errlvl lvl, const ucb_error* e)
{
    ucb_error_print(lvl, e);
    if (lvl != UCB_ERRLVL_WARNING)
        abort();
}

static ucb_mem_report_func s_default_mem_report;

UCB_DIAG_PUSH
UCB_DIAG_IGN_UNUSED_FUNCTION
static void testing_memreport(const ucb_mem_report* report)
{
    if (s_default_mem_report)
        s_default_mem_report(report);

    if (report)
    {
        CHECK(report->allocs == UCB_NULL);
    }
}

static void testing_memreport_final(const ucb_mem_report* report)
{
    if (report && report->allocs != NULL)
    {
        if (s_default_mem_report)
            s_default_mem_report(report);

        exit(1);
    }
}
UCB_DIAG_POP

// struct VerboseReporter : public doctest::ConsoleReporter
// {
//     VerboseReporter(const doctest::ContextOptions& opt): ConsoleReporter(opt) {}

//     void test_case_start(const doctest::TestCaseData& in) override
//     {
//         std::cout << "[RUN] " << in.m_name << std::endl;
//         ConsoleReporter::test_case_start(in);
//     }
//     void test_case_reenter(const doctest::TestCaseData& in) override
//     {
//         std::cout << "[REENTER] " << in.m_name << std::endl;
//         ConsoleReporter::test_case_reenter(in);
//     }
//     // See IReporter abstract base class for more options
// };

// REGISTER_LISTENER("verbose", 1, VerboseReporter);

DOCTEST_MSVC_SUPPRESS_WARNING_WITH_PUSH(4007) // 'function' : must be 'attribute' - see issue #182
int main(int argc, char** argv)
{
    ucb_init_console();
    ucb_error_set_func(testing_errfunc);
    UCB_MEMTRACK_ENABLE();
    s_default_mem_report = UCB_MEMTRACK_SET_FUNC(testing_memreport);
    int ret              = doctest::Context(argc, argv).run();
    UCB_MEMTRACK_SET_FUNC(testing_memreport_final);
    UCB_MEMTRACK_FINAL();
    return ret;
}
DOCTEST_MSVC_SUPPRESS_WARNING_POP
