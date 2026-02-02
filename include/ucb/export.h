/**
 * @file export.h
 * This file is part of the UCB project
 * - SPDX-FileCopyrightText: © 2026 Åke Svedin <ake@svedin.org>
 * - SPDX-License-Identifier: MIT
 *
 * @brief API definitions for UCB library
 */

#ifndef UCB_EXPORT_H
#define UCB_EXPORT_H

// clang-format off
#ifdef __cplusplus
    #ifdef _MSC_VER
        #define UCB_RESTRICT __restrict
    #else
        #define UCB_RESTRICT __restrict__
    #endif
#else
    #define UCB_RESTRICT restrict
#endif

// clang-format off
#ifdef _MSC_VER
    #define UCB_DEPRECATED __declspec(deprecated)
#elif defined(__GNUC__) || defined(__clang__)
    #define UCB_DEPRECATED __attribute__((deprecated))
#else
    #define UCB_DEPRECATED
#endif

#ifdef __cplusplus
    #define UCB_EXTERN_C extern "C"
    #define UCB_NORETURN [[noreturn]]
#else
    #define UCB_EXTERN_C
    #if defined(__STDC_VERSION__) && __STDC_VERSION__ >= 202311L
        #define UCB_NORETURN [[noreturn]]
    #elif defined(_MSC_VER)
        #define UCB_NORETURN _Noreturn
    #elif defined(__GNUC__) || defined(__clang__)
        #define UCB_NORETURN __attribute__((noreturn))
    #else
        #error "Unsupported compiler"
    #endif
#endif

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

#define UCB_API          UCB_EXTERN_C UCB_EXPORT
#define UCB_API_NORETURN UCB_EXTERN_C UCB_NORETURN UCB_EXPORT

#endif // UCB_EXPORT_H
