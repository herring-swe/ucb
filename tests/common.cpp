/**
 * @file common.cpp
 *
 * This file is part of the UCB project
 * - SPDX-FileCopyrightText: © 2026 Åke Svedin <ake@svedin.org>
 * - SPDX-License-Identifier: MIT
 *
 * @brief comment test utilities implementation
 */

#include "common.h"

#include "ucb/errcodes.h"
#include "ucb/error.h"

#include <cstdlib>
#include <iostream>

#ifndef _WIN32
#include <signal.h>
#endif

static thread_local TestFailureFixture* s_fixture = nullptr;

#ifdef _WIN32
jmp_buf FailureSetJump;
#else
sigjmp_buf FailureSetJump;
#endif

/**
 * Catches errors without failing so we can validate invalid arguments etc
 */

TestFailureFixture::TestFailureFixture() : m_prev_func(ucb_error_set_func(error_collect))
{
    s_fixture  = this;
    num_error  = 0;
    num_aborts = 0;
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

void TestFailureFixture::abort_handler(int)
{
    // No I/O here — this runs in signal context (async-signal-unsafe)
    if (!s_fixture)
        _Exit(128 + SIGABRT);

    s_fixture->num_aborts++;
#ifdef _WIN32
    longjmp(FailureSetJump, 1);
#else
    siglongjmp(FailureSetJump, 1);
#endif
}
