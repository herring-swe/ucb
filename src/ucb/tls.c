/**
 * @file tls.c
 * This file is part of the UCB project
 * - SPDX-FileCopyrightText: © 2026 Åke Svedin <ake@svedin.org>
 * - SPDX-License-Identifier: MIT
 *
 * @brief Thread-local storage implementation
 *
 * POSIX: One pthread_key per ucb_tls_key. The value is stored as-is and the
 * destructor is registered with pthread_key_create, so the OS runs it in the
 * exiting thread's context.
 *
 * Windows: One FLS slot per ucb_tls_key. FlsCallbacks only receive the stored
 * value, not the key, so a small wrapper holding the key is stored in the
 * slot instead. The callback resolves the key from the wrapper and runs the
 * user destructor.
 */

#include "ucb/tls.h"

#include "ucb/error.h"
#include "ucb/memory.h"

#ifdef _WIN32
#include <Windows.h>
#else
#include <pthread.h>
#endif

struct ucb_tls_key
{
#ifdef _WIN32
    DWORD fls_index;
#else
    pthread_key_t key;
#endif
    ucb_tls_dtor dtor;
};

#ifdef _WIN32

typedef struct ucb_tls_wrapper
{
    ucb_tls_key* key;
    void* value;
} ucb_tls_wrapper;

static void WINAPI ucb_tls_fls_callback(void* data)
{
    ucb_tls_wrapper* wrapper = (ucb_tls_wrapper*)data;
    if (!wrapper)
        return;

    if (wrapper->key->dtor)
        wrapper->key->dtor(wrapper->value);
    ucb_free(wrapper);
}

ucb_tls_key* ucb_tls_key_new(ucb_tls_dtor dtor)
{
    ucb_tls_key* key = ucb_malloc_type(1, ucb_tls_key);
    if (!key)
        return UCB_NULL;

    key->dtor = dtor;
    key->fls_index = FlsAlloc(ucb_tls_fls_callback);
    if (key->fls_index == FLS_OUT_OF_INDEXES)
    {
        UCB_REPORT_WIN32(GetLastError(), "FlsAlloc failed");
        ucb_free(key);
        return UCB_NULL;
    }
    return key;
}

void ucb_tls_key_free(ucb_tls_key* key)
{
    if (!key)
        return;
    FlsFree(key->fls_index);
    ucb_free(key);
}

void ucb_tls_set(ucb_tls_key* key, void* value)
{
    UCB_VERIFY_ARGS(key);

    ucb_tls_wrapper* wrapper = (ucb_tls_wrapper*)FlsGetValue(key->fls_index);
    if (value)
    {
        if (!wrapper)
        {
            wrapper = ucb_calloc_type(1, ucb_tls_wrapper);
            if (!wrapper)
                return;
            wrapper->key = key;
            if (!FlsSetValue(key->fls_index, wrapper))
            {
                UCB_REPORT_WIN32(GetLastError(), "FlsSetValue failed");
                ucb_free(wrapper);
            }
        }
        wrapper->value = value;
    }
    else if (wrapper)
    {
        if (!FlsSetValue(key->fls_index, UCB_NULL))
            UCB_REPORT_WIN32(GetLastError(), "FlsSetValue failed");
        ucb_free(wrapper);
    }
}

void* ucb_tls_get(const ucb_tls_key* key)
{
    if (!key)
        return UCB_NULL;
    ucb_tls_wrapper* wrapper = (ucb_tls_wrapper*)FlsGetValue(key->fls_index);
    return wrapper ? wrapper->value : UCB_NULL;
}

#else // _WIN32

ucb_tls_key* ucb_tls_key_new(ucb_tls_dtor dtor)
{
    ucb_tls_key* key = ucb_malloc_type(1, ucb_tls_key);
    if (!key)
        return UCB_NULL;

    key->dtor = dtor;
    int rc = pthread_key_create(&key->key, dtor);
    if (rc != 0)
    {
        UCB_REPORT_ERRNO(rc, "Failed to create TLS key");
        ucb_free(key);
        return UCB_NULL;
    }
    return key;
}

void ucb_tls_key_free(ucb_tls_key* key)
{
    if (!key)
        return;
    pthread_key_delete(key->key);
    ucb_free(key);
}

void ucb_tls_set(ucb_tls_key* key, void* value)
{
    UCB_VERIFY_ARGS(key);

    int rc = pthread_setspecific(key->key, value);
    if (rc != 0)
        UCB_REPORT_ERRNO(rc, "Failed to set TLS value");
}

void* ucb_tls_get(const ucb_tls_key* key)
{
    if (!key)
        return UCB_NULL;
    return pthread_getspecific(key->key);
}

#endif // _WIN32