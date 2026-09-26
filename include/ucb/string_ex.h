/**
 * @file string_ex.h
 *
 * This file is part of the UCB project
 * - SPDX-FileCopyrightText: © 2026 Åke Svedin <ake@svedin.org>
 * - SPDX-License-Identifier: MIT
 *
 * @brief Extended, opt-in string helpers
 *
 * This header follows the `_ex.h` convention: an optional companion to a main
 * header that holds additions which do not fit cleanly into a section of that
 * header, or extra functions that most users do not need. It is never included
 * by the main header; include it explicitly when needed.
 *
 * Include this header instead of @ref string.h when the extended API is wanted.
 */

#ifndef UCB_STRING_EX_H
#define UCB_STRING_EX_H

#include <ucb/string.h>

/**
 * @name C string conversion
 * @{
 */

/**
 * @def UCB_CSTR
 * @brief Coerce a C string, @ref ucb_str or (C++ only) a type exposing
 * <tt>c_str()</tt> to a <tt>const char*</tt>.
 *
 * In C this is implemented with @c _Generic and accepts <tt>char*</tt>,
 * <tt>const char*</tt>, <tt>ucb_str*</tt> and <tt>const ucb_str*</tt>. String
 * literals are accepted since they decay to <tt>char*</tt>.
 *
 * In C++ additional inline overloads plus a SFINAE template support any type
 * with a <tt>c_str()</tt> method, such as <tt>std::string</tt>. This header
 * does not include any C++ standard library header.
 *
 * The accessors below are named @c ucb_cstr_of in C++ to provide the overload
 * set used by the macro; use @ref UCB_CSTR rather than calling them directly.
 *
 * @code
 * ucb_str* s = ucb_str_new_c("hello");
 * const char* p = UCB_CSTR(s);
 * @endcode
 */
#ifdef __cplusplus
inline const char* ucb_cstr_of(const char* s)
{
    return s;
}
inline const char* ucb_cstr_of(char* s)
{
    return s;
}
inline const char* ucb_cstr_of(const ucb_str* s)
{
    return ucb_str_cstr(s);
}
inline const char* ucb_cstr_of(ucb_str* s)
{
    return ucb_str_cstr(s);
}
/* Opt-in support for types exposing c_str() (e.g. std::string). SFINAE keeps
   arrays and char* out, so the overloads above win for those. */
template <class T>
inline auto ucb_cstr_of(const T& s) -> decltype(s.c_str())
{
    return s.c_str();
}
#define UCB_CSTR(x) ucb_cstr_of(x)
#else
#define UCB_CSTR(x)                                  \
    _Generic((x),                                    \
        char*: (const char*)(x),                     \
        const char*: (const char*)(x),               \
        ucb_str*: ucb_str_cstr((const ucb_str*)(x)), \
        const ucb_str*: ucb_str_cstr((const ucb_str*)(x)))
#endif

/** @} */

#endif // UCB_STRING_EX_H
