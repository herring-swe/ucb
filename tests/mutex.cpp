/**
 * @file mutex.cpp
 *
 * This file is part of the UCB project
 * - SPDX-FileCopyrightText: © 2026 Åke Svedin <ake@svedin.org>
 * - SPDX-License-Identifier: MIT
 *
 * @brief mutex tests
 */

#include "ucb/mutex.h"

#include "common.h"
#include <condition_variable>

#include "ucb/memory.h"
#include "ucb/threads.h"

#include <doctest.h>

#include <mutex>
#include <thread>

// Handshake used to hold a ucb_mutex from another thread and release it on
// demand, without relying on timing.
struct MutexHandshake
{
    ucb_mutex* mutex;
    std::mutex* hm;
    std::condition_variable* cv;
    bool* ready;
    bool* go;
};

static int hold_mutex_worker(void* arg)
{
    MutexHandshake* hs = static_cast<MutexHandshake*>(arg);
    ucb_mutex_lock(hs->mutex);
    {
        std::lock_guard<std::mutex> lk(*hs->hm);
        *hs->ready = true;
    }
    hs->cv->notify_all();
    {
        std::unique_lock<std::mutex> lk(*hs->hm);
        hs->cv->wait(lk, [hs] { return *hs->go; });
    }
    ucb_mutex_unlock(hs->mutex);
    return 0;
}

TEST_CASE("mutex - basics")
{
    SUBCASE("heap standard")
    {
        ucb_mutex* mutex = ucb_mutex_new();
        REQUIRE(mutex != nullptr);
        REQUIRE_FALSE(ucb_mutex_is_recursive(mutex));
        REQUIRE(ucb_mutex_trylock(mutex));
        ucb_mutex_unlock(mutex);
        ucb_mutex_free(mutex);
    }

    SUBCASE("heap recursive")
    {
        ucb_mutex* mutex = ucb_mutex_new_recursive();
        REQUIRE(mutex != nullptr);
        REQUIRE(ucb_mutex_is_recursive(mutex));
        ucb_mutex_lock(mutex);
        ucb_mutex_lock(mutex);
        ucb_mutex_unlock(mutex);
        ucb_mutex_unlock(mutex);
        ucb_mutex_free(mutex);
    }

    SUBCASE("stack standard")
    {
        ucb_mutex mutex;
        REQUIRE(ucb_mutex_init(&mutex));
        REQUIRE_FALSE(ucb_mutex_is_recursive(&mutex));
        ucb_mutex_lock(&mutex);
        ucb_mutex_unlock(&mutex);
        ucb_mutex_release(&mutex);
    }

    SUBCASE("stack recursive trylock")
    {
        ucb_mutex mutex;
        REQUIRE(ucb_mutex_init_recursive(&mutex));
        REQUIRE(ucb_mutex_is_recursive(&mutex));
        REQUIRE(ucb_mutex_trylock(&mutex));
        REQUIRE(ucb_mutex_trylock(&mutex));
        ucb_mutex_unlock(&mutex);
        ucb_mutex_unlock(&mutex);
        ucb_mutex_release(&mutex);
    }

    SUBCASE("free null is safe")
    {
        ucb_mutex_free(nullptr);
    }
}

TEST_CASE("mutex - cross-thread trylock")
{
    ucb_mutex mutex;
    REQUIRE(ucb_mutex_init(&mutex));

    std::mutex hm;
    std::condition_variable cv;
    bool ready = false;
    bool go = false;
    MutexHandshake hs = {&mutex, &hm, &cv, &ready, &go};

    ucb_thread* th = ucb_thread_new();
    REQUIRE(th != nullptr);

    ucb_task task = ucb_task_make(hold_mutex_worker);
    task.arg = &hs;
    REQUIRE(ucb_thread_start(th, task));

    {
        std::unique_lock<std::mutex> lk(hm);
        cv.wait(lk, [&] { return ready; });
    }

    // Another thread holds the mutex, so trylock must fail
    REQUIRE_FALSE(ucb_mutex_trylock(&mutex));

    {
        std::lock_guard<std::mutex> lk(hm);
        go = true;
    }
    cv.notify_all();

    ucb_thread_join(th);
    ucb_thread_free(th);
    ucb_mutex_release(&mutex);
}

TEST_CASE_FIXTURE(TestFailureFixture, "mutex - misuse")
{
    SUBCASE("null init")
    {
        CHECK_ABORTS(ucb_mutex_init(nullptr));

        REQUIRE(num_aborts == 1);
        REQUIRE(num_error == 1);
        REQUIRE(errors[0].code == UCB_ERROR_INVALID_ARG);
    }

    SUBCASE("release locked")
    {
        ucb_mutex mutex;
        REQUIRE(ucb_mutex_init(&mutex));
        ucb_mutex_lock(&mutex);

        CHECK_ABORTS(ucb_mutex_release(&mutex));

        // The mutex is still usable after the failed release
        ucb_mutex_unlock(&mutex);
        ucb_mutex_release(&mutex);

        REQUIRE(num_aborts == 1);
        REQUIRE(num_error == 1);
        REQUIRE(errors[0].code == UCB_ERROR_MUTEX_LOCKED);
    }

    SUBCASE("standard self relock")
    {
        ucb_mutex mutex;
        REQUIRE(ucb_mutex_init(&mutex));
        ucb_mutex_lock(&mutex);

        CHECK_ABORTS(ucb_mutex_lock(&mutex));

        ucb_mutex_unlock(&mutex);
        ucb_mutex_release(&mutex);

        REQUIRE(num_aborts == 1);
        REQUIRE(num_error == 1);
        REQUIRE(errors[0].code == UCB_ERROR_MUTEX_LOCKED);
    }

    SUBCASE("unlock by non-owner")
    {
        ucb_mutex mutex;
        REQUIRE(ucb_mutex_init(&mutex));

        std::mutex hm;
        std::condition_variable cv;
        bool ready = false;
        bool go = false;
        MutexHandshake hs = {&mutex, &hm, &cv, &ready, &go};

        ucb_thread* th = ucb_thread_new();
        REQUIRE(th != nullptr);
        ucb_task task = ucb_task_make(hold_mutex_worker);
        task.arg = &hs;
        REQUIRE(ucb_thread_start(th, task));

        {
            std::unique_lock<std::mutex> lk(hm);
            cv.wait(lk, [&] { return ready; });
        }

        CHECK_ABORTS(ucb_mutex_unlock(&mutex));

        {
            std::lock_guard<std::mutex> lk(hm);
            go = true;
        }
        cv.notify_all();

        ucb_thread_join(th);
        ucb_thread_free(th);
        ucb_mutex_release(&mutex);

        REQUIRE(num_aborts == 1);
        REQUIRE(num_error == 1);
        REQUIRE(errors[0].code == UCB_ERROR_MUTEX_LOCKED);
    }

    SUBCASE("release while held by another thread")
    {
        ucb_mutex mutex;
        REQUIRE(ucb_mutex_init(&mutex));

        std::mutex hm;
        std::condition_variable cv;
        bool ready = false;
        bool go = false;
        MutexHandshake hs = {&mutex, &hm, &cv, &ready, &go};

        ucb_thread* th = ucb_thread_new();
        REQUIRE(th != nullptr);
        ucb_task task = ucb_task_make(hold_mutex_worker);
        task.arg = &hs;
        REQUIRE(ucb_thread_start(th, task));

        {
            std::unique_lock<std::mutex> lk(hm);
            cv.wait(lk, [&] { return ready; });
        }

        CHECK_ABORTS(ucb_mutex_release(&mutex));

        {
            std::lock_guard<std::mutex> lk(hm);
            go = true;
        }
        cv.notify_all();

        ucb_thread_join(th);
        ucb_thread_free(th);

        // The mutex is still usable after the failed release
        REQUIRE(ucb_mutex_trylock(&mutex));
        ucb_mutex_unlock(&mutex);
        ucb_mutex_release(&mutex);

        REQUIRE(num_aborts == 1);
        REQUIRE(num_error == 1);
        REQUIRE(errors[0].code == UCB_ERROR_MUTEX_LOCKED);
    }
}
