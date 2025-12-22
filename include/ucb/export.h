/**
 * This file is part of the UCB project
 * SPDX-FileCopyrightText: 2025 Åke Svedin <ake@svedin.org>
 * SPDX-License-Identifier: MIT
 *
 * @brief
 */

#ifndef UCB_EXPORT_H
#define UCB_EXPORT_H

#ifdef _WIN32
#define UCB_DEPRECATED __declspec(deprecated)
#elif defined(__GNUC__) || defined(__clang__)
#define UCB_DEPRECATED __attribute__((deprecated))
#else
#define UCB_DEPRECATED
#endif

#ifdef __cplusplus
#define UCB_EXTERN_C extern "C"
#else
#define UCB_EXTERN_C
#endif

// clang-format off
#ifdef UCB_STATIC_LIB
    #define UCB_EXPORT
#else
    #ifdef _WIN32
        #ifdef UCB_BUILD_LIB
            #define UCB_EXPORT __declspec(dllexport)
        #else
            #define UCB_EXPORT __declspec(dllimport)
        #endif
    #else
        #define UCB_EXPORT __attribute__((visibility("default")))
    #endif
#endif
// clang-format on

#define UCB_API UCB_EXTERN_C UCB_EXPORT

#endif // UCB_EXPORT_H
