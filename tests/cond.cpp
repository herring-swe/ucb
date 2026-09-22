/**
 * @file cond.cpp
 *
 * This file is part of the UCB project
 * - SPDX-FileCopyrightText: © 2026 Åke Svedin <ake@svedin.org>
 * - SPDX-License-Identifier: MIT
 *
 * @brief condition variable tests
 */

#include "ucb/cond.h"

#include "common.h"

#include "ucb/memory.h"
#include "ucb/mutex.h"
#include "ucb/threads.h"

#include <doctest.h>

#include <atomic>
#include <vector>

// Deterministic handshake: waiters signal readiness on ready_cond, then wait
// on wait_cond until go is set. All state is protected by the shared mutex.
struct CondHandshake
{
    ucb_mutex* mutex;
    ucb_cond* wait_cond;
    ucb_cond* ready_cond;
    int* ready_count;
    bool* go;
    std::atomic<int>* woken;
};

static int cond_waiter(void* arg)
{
    CondHandshake* hs = static_cast<CondHandshake*>(arg);
    ucb_mutex_lock(hs->mutex);
    (*hs->ready_count)++;
    ucb_cond_signal(hs->ready_cond);
    while (!*hs->go)
    {
        if (!ucb_cond_wait(hs->wait_cond, hs->mutex))
            break;
    }
    hs->woken->fetch_add(1);
    ucb_mutex_unlock(hs->mutex);
    return 0;
}

static int cond_timed_waiter(void* arg)
{
    CondHandshake* hs = static_cast<CondHandshake*>(arg);
    ucb_mutex_lock(hs->mutex);
    (*hs->ready_count)++;
    ucb_cond_signal(hs->ready_cond);
    while (!*hs->go)
    {
        if (ucb_cond_timedwait(hs->wait_cond, hs->mutex, 1000) != UCB_COND_SIGNALLED)
            break;
    }
    hs->woken->fetch_add(1);
    ucb_mutex_unlock(hs->mutex);
    return 0;
}

TEST_CASE("cond basics")
{
    SUBCASE("heap")
    {
        ucb_cond* cond = ucb_cond_new();
        REQUIRE(cond != nullptr);
        ucb_cond_free(cond);
    }

    SUBCASE("stack")
    {
        ucb_cond cond;
        REQUIRE(ucb_cond_init(&cond));
        REQUIRE(ucb_cond_release(&cond));
    }

    SUBCASE("timedwait timeout")
    {
        ucb_mutex mutex;
        ucb_cond cond;
        REQUIRE(ucb_mutex_init(&mutex));
        REQUIRE(ucb_cond_init(&cond));

        ucb_mutex_lock(&mutex);
        CHECK(ucb_cond_timedwait(&cond, &mutex, 10) == UCB_COND_TIMEDOUT);
        ucb_mutex_unlock(&mutex);

        ucb_cond_release(&cond);
        ucb_mutex_release(&mutex);
    }

    SUBCASE("free null is safe")
    {
        ucb_cond_free(nullptr);
    }
}

TEST_CASE("cond wait and signal")
{
    ucb_mutex mutex;
    ucb_cond wait_cond;
    ucb_cond ready_cond;
    REQUIRE(ucb_mutex_init(&mutex));
    REQUIRE(ucb_cond_init(&wait_cond));
    REQUIRE(ucb_cond_init(&ready_cond));

    int ready_count = 0;
    bool go = false;
    std::atomic<int> woken{0};
    CondHandshake hs = {&mutex, &wait_cond, &ready_cond, &ready_count, &go, &woken};

    ucb_thread* th = ucb_thread_new();
    REQUIRE(th != nullptr);
    ucb_task task = ucb_task_make(cond_waiter);
    task.arg = &hs;
    REQUIRE(ucb_thread_start(th, task));

    ucb_mutex_lock(&mutex);
    while (ready_count < 1)
        ucb_cond_wait(&ready_cond, &mutex);
    go = true;
    ucb_cond_signal(&wait_cond);
    ucb_mutex_unlock(&mutex);

    ucb_thread_join(th);
    ucb_thread_free(th);

    REQUIRE(woken.load() == 1);

    ucb_cond_release(&ready_cond);
    ucb_cond_release(&wait_cond);
    ucb_mutex_release(&mutex);
}

TEST_CASE("cond timedwait signalled")
{
    ucb_mutex mutex;
    ucb_cond wait_cond;
    ucb_cond ready_cond;
    REQUIRE(ucb_mutex_init(&mutex));
    REQUIRE(ucb_cond_init(&wait_cond));
    REQUIRE(ucb_cond_init(&ready_cond));

    int ready_count = 0;
    bool go = false;
    std::atomic<int> woken{0};
    CondHandshake hs = {&mutex, &wait_cond, &ready_cond, &ready_count, &go, &woken};

    ucb_thread* th = ucb_thread_new();
    REQUIRE(th != nullptr);
    ucb_task task = ucb_task_make(cond_timed_waiter);
    task.arg = &hs;
    REQUIRE(ucb_thread_start(th, task));

    ucb_mutex_lock(&mutex);
    while (ready_count < 1)
        ucb_cond_wait(&ready_cond, &mutex);
    go = true;
    ucb_cond_signal(&wait_cond);
    ucb_mutex_unlock(&mutex);

    ucb_thread_join(th);
    ucb_thread_free(th);

    REQUIRE(woken.load() == 1);

    ucb_cond_release(&ready_cond);
    ucb_cond_release(&wait_cond);
    ucb_mutex_release(&mutex);
}

TEST_CASE("cond broadcast")
{
    constexpr int N = 4;

    ucb_mutex mutex;
    ucb_cond wait_cond;
    ucb_cond ready_cond;
    REQUIRE(ucb_mutex_init(&mutex));
    REQUIRE(ucb_cond_init(&wait_cond));
    REQUIRE(ucb_cond_init(&ready_cond));

    int ready_count = 0;
    bool go = false;
    std::atomic<int> woken{0};
    CondHandshake hs = {&mutex, &wait_cond, &ready_cond, &ready_count, &go, &woken};

    std::vector<ucb_thread*> threads;
    for (int i = 0; i < N; i++)
    {
        ucb_thread* th = ucb_thread_new();
        REQUIRE(th != nullptr);
        ucb_task task = ucb_task_make(cond_waiter);
        task.arg = &hs;
        REQUIRE(ucb_thread_start(th, task));
        threads.push_back(th);
    }

    ucb_mutex_lock(&mutex);
    while (ready_count < N)
        ucb_cond_wait(&ready_cond, &mutex);
    go = true;
    ucb_cond_broadcast(&wait_cond);
    ucb_mutex_unlock(&mutex);

    for (ucb_thread* th : threads)
    {
        ucb_thread_join(th);
        ucb_thread_free(th);
    }

    REQUIRE(woken.load() == N);

    ucb_cond_release(&ready_cond);
    ucb_cond_release(&wait_cond);
    ucb_mutex_release(&mutex);
}

TEST_CASE_FIXTURE(TestFailureFixture, "cond error handling")
{
    SUBCASE("null init")
    {
        CHECK_ABORTS(ucb_cond_init(nullptr));

        REQUIRE(num_aborts == 1);
        REQUIRE(num_error == 1);
        REQUIRE(errors[0].code == UCB_ERROR_INVALID_ARG);
    }

    SUBCASE("negative timeout")
    {
        ucb_mutex mutex;
        ucb_cond cond;
        REQUIRE(ucb_mutex_init(&mutex));
        REQUIRE(ucb_cond_init(&cond));

        ucb_mutex_lock(&mutex);
        CHECK_ABORTS(ucb_cond_timedwait(&cond, &mutex, -1));
        ucb_mutex_unlock(&mutex);

        ucb_cond_release(&cond);
        ucb_mutex_release(&mutex);

        REQUIRE(num_aborts == 1);
        REQUIRE(num_error == 1);
        REQUIRE(errors[0].code == UCB_ERROR_INVALID_ARG);
    }
}
