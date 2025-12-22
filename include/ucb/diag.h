/**
 * This file is part of the UCB project
 * SPDX-FileCopyrightText: © 2025 Åke Svedin <ake@svedin.org>
 * SPDX-License-Identifier: MIT
 *
 * @brief
 */

#ifndef UCB_DIAG_H
#define UCB_DIAG_H

#if defined(__clang__)
#define UCB_DIAG_PUSH                    __pragma(clang diagnostic push)
#define UCB_DIAG_POP                     __pragma(clang diagnostic pop)
#define UCB_DIAG_IGN(w)                  __pragma(clang diagnostic ignored w)
#define UCB_DIAG_CLANG_IGN(w)            __pragma(clang diagnostic ignored w)
#define UCB_DIAG_IGN_ALIGN_CAST          __pragma(clang diagnostic ignored "-Wcast-align")
#define UCB_DIAG_IGN_FORMAT_NONLITERAL   __pragma(clang diagnostic ignored "-Wformat-nonliteral")
#define UCB_DIAG_IGN_NRVO                __pragma(clang diagnostic ignored "-Wnrvo")
#define UCB_DIAG_IGN_PADDED              __pragma(clang diagnostic ignored "-Wpadded")
#define UCB_DIAG_IGN_UNSAFE_BUFFER_USAGE __pragma(clang diagnostic ignored "-Wunsafe-buffer-usage")
#elif defined(__GNUC__)
#define UCB_DIAG_PUSH                    __pragma(GCC diagnostic push)
#define UCB_DIAG_POP                     __pragma(GCC diagnostic pop)
#define UCB_DIAG_NO(w)                   __pragma(GCC diagnostic ignored w)
#define UCB_DIAG_GCC_NO(w)               __pragma(GCC diagnostic ignored w)
#define UCB_DIAG_IGN_ALIGN_CAST          __pragma(GCC diagnostic ignored "-Wcast-align")
#define UCB_DIAG_IGN_FORMAT_NONLITERAL   __pragma(GCC diagnostic ignored "-Wformat-nonliteral")
#define UCB_DIAG_IGN_NRVO                __pragma(GCC diagnostic ignored "-Wnrvo")
#define UCB_DIAG_IGN_PADDED              __pragma(GCC diagnostic ignored "-Wpadded")
#define UCB_DIAG_IGN_UNSAFE_BUFFER_USAGE __pragma(GCC diagnostic ignored "-Wunsafe-buffer-usage")
#elif defined(_MSC_VER)
#define UCB_DIAG_PUSH        __pragma(warning(push))
#define UCB_DIAG_POP         __pragma(warning(pop))
#define UCB_DIAG_IGN(w)      __pragma(warning(disable : w))
#define UCB_DIAG_MSVC_IGN(w) __pragma(warning(disable : w))
#define UCB_DIAG_IGN_ALIGN_CAST
#define UCB_DIAG_IGN_FORMAT_NONLITERAL
#define UCB_DIAG_IGN_NRVO
#define UCB_DIAG_IGN_PADDED __pragma(warning(disable : 4820))
#define UCB_DIAG_IGN_UNSAFE_BUFFER_USAGE
#else
// Unsupported compiler
#define UCB_DIAG_PUSH
#define UCB_DIAG_POP
#define UCB_DIAG_IGN(w)
#define UCB_DIAG_IGN_PADDED
#endif

#ifndef UCB_DIAG_CLANG_IGN
#define UCB_DIAG_CLANG_IGN(w)
#endif
#ifndef UCB_DIAG_GCC_IGN
#define UCB_DIAG_GCC_IGN(w)
#endif
#ifndef UCB_DIAG_MSVC_IGN
#define UCB_DIAG_MSVC_IGN(w)
#endif

#endif // UCB_DIAG_H
