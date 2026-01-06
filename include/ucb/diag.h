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
#define UCB_DO_PRAGMA(x)                 _Pragma(#x)
#define UCB_PRAGMA_CLANG_DIAG(x)         UCB_DO_PRAGMA(clang diagnostic x)
#define UCB_DIAG_PUSH                    UCB_PRAGMA_CLANG_DIAG(push)
#define UCB_DIAG_POP                     UCB_PRAGMA_CLANG_DIAG(pop)
#define UCB_DIAG_IGN(w)                  UCB_PRAGMA_CLANG_DIAG(ignored w)
#define UCB_DIAG_CLANG_IGN(w)            UCB_DIAG_IGN(w)
#define UCB_DIAG_IGN_ALIGN_CAST          UCB_DIAG_IGN("-Wcast-align")
#define UCB_DIAG_IGN_DBL_PROM            UCB_DIAG_IGN("-Wdouble-promotion")
#define UCB_DIAG_IGN_FORMAT_NONLITERAL   UCB_DIAG_IGN("-Wformat-nonliteral")
#define UCB_DIAG_IGN_IMPL_INT_FLOAT      UCB_DIAG_IGN("-Wimplicit-int-float-conversion")
#define UCB_DIAG_IGN_PADDED              UCB_DIAG_IGN("-Wpadded")
#define UCB_DIAG_IGN_UNREACHABLE_CODE    UCB_DIAG_IGN("-Wunreachable-code")
#define UCB_DIAG_IGN_UNSAFE_BUFFER_USAGE UCB_DIAG_IGN("-Wunsafe-buffer-usage")
#define UCB_DIAG_IGN_UNUSED_FUNCTION     UCB_DIAG_IGN("-Wunused-function")
#define UCB_DIAG_IGN_UNUSED_VALUE        UCB_DIAG_IGN("-Wunused-value")
#if defined(__clang_major__) && __clang_major__ >= 21
#define UCB_DIAG_IGN_NRVO UCB_DIAG_IGN("-Wnrvo")
#endif
#elif defined(__GNUC__)
#define UCB_DO_PRAGMA(x)               _Pragma(#x)
#define UCB_PRAGMA_GCC_DIAG(x)         UCB_DO_PRAGMA(GCC diagnostic x)
#define UCB_DIAG_PUSH                  UCB_PRAGMA_GCC_DIAG(push)
#define UCB_DIAG_POP                   UCB_PRAGMA_GCC_DIAG(pop)
#define UCB_DIAG_IGN(w)                UCB_PRAGMA_GCC_DIAG(ignored w)
#define UCB_DIAG_GCC_IGN(w)            UCB_DIAG_IGN(w)
#define UCB_DIAG_IGN_ALIGN_CAST        UCB_DIAG_IGN("-Wcast-align")
#define UCB_DIAG_IGN_FORMAT_NONLITERAL UCB_DIAG_IGN("-Wformat-nonliteral")
#define UCB_DIAG_IGN_PADDED            UCB_DIAG_IGN("-Wpadded")
#define UCB_DIAG_IGN_UNUSED_FUNCTION   UCB_DIAG_IGN("-Wunused-function")
#define UCB_DIAG_IGN_UNUSED_VALUE      UCB_DIAG_IGN("-Wunused-value")
#if defined(__GNUC__) && __GNUC__ >= 14
#define UCB_DIAG_IGN_NRVO UCB_DIAG_IGN("-Wnrvo")
#endif
#elif defined(_MSC_VER)
#define UCB_DIAG_PUSH                __pragma(warning(push))
#define UCB_DIAG_POP                 __pragma(warning(pop))
#define UCB_DIAG_IGN(w)              __pragma(warning(disable : w))
#define UCB_DIAG_MSVC_IGN(w)         __pragma(warning(disable : w))
#define UCB_DIAG_IGN_PADDED          __pragma(warning(disable : 4820))
#define UCB_DIAG_IGN_UNUSED_FUNCTION __pragma(warning(disable : 4505))
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

#ifndef UCB_DIAG_IGN_ALIGN_CAST
#define UCB_DIAG_IGN_ALIGN_CAST
#endif

#ifndef UCB_DIAG_IGN_DBL_PROM
#define UCB_DIAG_IGN_DBL_PROM
#endif

#ifndef UCB_DIAG_IGN_FORMAT_NONLITERAL
#define UCB_DIAG_IGN_FORMAT_NONLITERAL
#endif

#ifndef UCB_DIAG_IGN_IMPL_INT_FLOAT
#define UCB_DIAG_IGN_IMPL_INT_FLOAT
#endif

#ifndef UCB_DIAG_IGN_NRVO
#define UCB_DIAG_IGN_NRVO
#endif

#ifndef UCB_DIAG_IGN_UNREACHABLE_CODE
#define UCB_DIAG_IGN_UNREACHABLE_CODE
#endif

#ifndef UCB_DIAG_IGN_UNSAFE_BUFFER_USAGE
#define UCB_DIAG_IGN_UNSAFE_BUFFER_USAGE
#endif

#ifndef UCB_DIAG_IGN_UNUSED_VALUE
#define UCB_DIAG_IGN_UNUSED_VALUE
#endif

#endif // UCB_DIAG_H
