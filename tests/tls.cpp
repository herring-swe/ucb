/**
 * @file tls.cpp
 *
 * This file is part of the UCB project
 * - SPDX-FileCopyrightText: © 2026 Åke Svedin <ake@svedin.org>
 * - SPDX-License-Identifier: MIT
 *
 * @brief TLS tests
 */

#include "ucb/tls.h"

#include "common.h"

#include "ucb/memory.h"
#include "ucb/threads.h"

#include <doctest.h>

#include <atomic>
#include <cstring>

static std::atomic<int> s_dtor_calls{0};

static void tls_free_dtor(void* value)
{
    s_dtor_calls.fetch_add(1);
    ucb_free(value);
}

struct TlsWorkerArg
{
    ucb_tls_key* key;
    int value;
};

static int tls_worker(void* arg)
{
    TlsWorkerArg* wa = static_cast<TlsWorkerArg*>(arg);
    REQUIRE(wa != nullptr);
    REQUIRE(ucb_tls_get(wa->key) == nullptr); // Isolated per thread

    int* val = static_cast<int*>(ucb_malloc(sizeof(int)));
    REQUIRE(val != nullptr);
    *val = wa->value;
    ucb_tls_set(wa->key, val);
    REQUIRE(ucb_tls_get(wa->key) == val);

    return 0;
}

TEST_CASE("tls key basics")
{
    ucb_tls_key* key = ucb_tls_key_new(tls_free_dtor);
    REQUIRE(key != nullptr);
    ucb_tls_key_free(key);
    ucb_tls_key_free(nullptr); // Must be a safe no-op
}

TEST_CASE("tls get/set")
{
    s_dtor_calls.store(0);
    ucb_tls_key* key = ucb_tls_key_new(tls_free_dtor);
    REQUIRE(key != nullptr);

    REQUIRE(ucb_tls_get(key) == nullptr);

    char* val = static_cast<char*>(ucb_malloc(16));
    REQUIRE(val != nullptr);
    strcpy(val, "tls-test");
    ucb_tls_set(key, val);
    REQUIRE(ucb_tls_get(key) == val);

    // Clearing must not run the destructor for the current thread
    ucb_tls_set(key, nullptr);
    REQUIRE(ucb_tls_get(key) == nullptr);
    ucb_free(val);

    ucb_tls_key_free(key);
    REQUIRE(s_dtor_calls.load() == 0);
}

TEST_CASE("tls destructor runs at thread exit")
{
    s_dtor_calls.store(0);
    ucb_tls_key* key = ucb_tls_key_new(tls_free_dtor);
    REQUIRE(key != nullptr);

    TlsWorkerArg arg = {key, 42};
    for (int i = 0; i < 3; i++)
    {
        ucb_thread* th = ucb_thread_new();
        REQUIRE(th != nullptr);

        ucb_task task = {0};
        task.func = tls_worker;
        task.arg = &arg;
        REQUIRE(ucb_thread_start(th, task) == true);
        ucb_thread_join(th);
        ucb_thread_free(th);

        // Destructor runs once per exiting thread, not per call
        REQUIRE(s_dtor_calls.load() == i + 1);
    }

    ucb_tls_key_free(key);
}

TEST_CASE_FIXTURE(TestFailureFixture, "tls error handling")
{
    CHECK_ABORTS(ucb_tls_set(nullptr, (void*)0x1));
    REQUIRE(num_aborts == 1);
    REQUIRE(num_error == 1);
    REQUIRE(errors[0].code == UCB_ERROR_INVALID_ARG);
}