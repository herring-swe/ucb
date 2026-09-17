/**
 * @file tls.h
 * This file is part of the UCB project
 * - SPDX-FileCopyrightText: © 2026 Åke Svedin <ake@svedin.org>
 * - SPDX-License-Identifier: MIT
 *
 * @brief Thread-local storage with automatic cleanup at thread exit
 *
 * A TLS key associates a value with the calling thread. Each thread has its
 * own value for the key. Values are automatically freed at thread exit
 * through a user-provided destructor, which is invoked in the context of the
 * exiting thread, even for threads that were not created by UCB.
 *
 * The C standard mechanisms (_Thread_local / C23 thread_local) have no
 * destructor hooks. Use this module instead of thread-local static variables
 * whenever the thread-local state is heap allocated or otherwise needs
 * cleanup.
 *
 * Lifetime:
 * - Keys are created with @ref ucb_tls_key_new and are intended to be
 *   process-lifetime objects, typically created once at program start.
 * - The value stored for a thread is valid until a new value is set with
 *   @ref ucb_tls_set, the key is cleared with @ref ucb_tls_set(..., UCB_NULL),
 *   or the thread exits.
 * - The destructor is run exactly once per thread that had a value set,
 *   when that thread exits. The destructor is responsible for freeing the
 *   value.
 * - Overwriting a value with @ref ucb_tls_set does not free the previous
 *   value. The caller is responsible for the previous value.
 *
 * Usage example:
 * @code{.c}
 * static void buffer_dtor(void* value)
 * {
 *     ucb_free(value);
 * }
 *
 * static ucb_tls_key* s_buf_key; // Created once at program start
 *
 * char* get_scratch(size_t size)
 * {
 *     char* buf = (char*)ucb_tls_get(s_buf_key);
 *     if (!buf)
 *     {
 *         buf = ucb_malloc(size);
 *         ucb_tls_set(s_buf_key, buf); // Freed automatically at thread exit
 *     }
 *     return buf;
 * }
 * @endcode
 *
 * Limitations:
 * - Values are per-thread and only accessible from the thread that set them.
 * - Destructors run at thread exit. They do not run on abrupt process exit
 *   (exit(), abort() or TerminateThread), so any values may then be reported
 *   as leaks by memdbg.
 * - On POSIX, a destructor that sets a new non-NULL value for its own key may
 *   be called again, up to PTHREAD_DESTRUCTOR_ITERATIONS times. A destructive
 *   destructor must therefore never re-set its own key.
 * - @ref ucb_tls_key_free does not run pending destructors for values still
 *   stored in other threads, and must only be called when no other thread can
 *   use the key, normally during program shutdown.
 * - Destructors run in the exiting thread. Avoid blocking or acquiring locks
 *   that may be held by other threads at that point.
 */

#ifndef UCB_TLS_H
#define UCB_TLS_H

#include <ucb/export.h>

/**
 * @brief Destructor for a thread-local value.
 *
 * Called when the thread that set a value for the key exits, or when the
 * thread clears it, depending on the function. Must not call @ref ucb_tls_set
 * with the same key.
 *
 * @param value The value stored for the exiting thread, or UCB_NULL.
 */
typedef void (*ucb_tls_dtor)(void* value);

/**
 * @struct ucb_tls_key
 * @brief Thread-local storage key
 */
typedef struct ucb_tls_key ucb_tls_key;

/**
 * @brief Create a new TLS key
 *
 * The key is intended to be process-lifetime. The destructor is optional and
 * may be UCB_NULL, in which case values are never automatically freed.
 *
 * On failure a system error is reported and UCB_NULL is returned.
 *
 * @param dtor Destructor called at thread exit with the stored value, or
 *             UCB_NULL for no destructor.
 * @return the new key, or UCB_NULL on failure
 */
UCB_API ucb_tls_key* ucb_tls_key_new(ucb_tls_dtor dtor);

/**
 * @brief Delete a TLS key
 *
 * The key and its internal resources are freed. Pending destructors for
 * values still stored in other threads are ignored. Only call this when no
 * thread can still use the key, normally during program shutdown.
 *
 * @param key key to delete, may be UCB_NULL
 */
UCB_API void ucb_tls_key_free(ucb_tls_key* key);

/**
 * @brief Set the value for the calling thread
 *
 * The previous value, if any, is overwritten without being freed, unless it
 * was not set. The caller is responsible for the previous value.
 *
 * Passing UCB_NULL clears the value for the calling thread. The destructor is
 * not called.
 *
 * @param key the key
 * @param value value to store, or UCB_NULL to clear
 */
UCB_API void ucb_tls_set(ucb_tls_key* key, void* value);

/**
 * @brief Get the value for the calling thread
 * @param key the key
 * @return the stored value, or UCB_NULL if not set
 */
UCB_API void* ucb_tls_get(const ucb_tls_key* key);

#endif // UCB_TLS_H