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

#include <ucb/path.h>
#include <ucb/string.h>

#include <string.h>

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

/**
 * @def UCB_STR
 * @brief Helper macro to wrap a string literal, a
 * <tt>const char*</tt> as a borrowed @ref ucb_str valid for the enclosing
 * full expression.
 *
 * The C++ version can also wrap a @c std::string or other type exposing
 * <tt>c_str()</tt> and <tt>size()</tt>.
 *
 * A UCB_NULL C string wraps as the interned empty string. In C the length of a
 * runtime string is measured once with @c strlen (there is no portable way to
 * distinguish a literal from a pointer after array-to-pointer conversion).
 *
 * The returned value is a non-owning view; it must only be used for the
 * duration of the expression it appears in.
 *
 * @code
 * ucb_path* p = ucb_path_new(UCB_STR("config"));
 * @endcode
 */

#ifdef __cplusplus
/* C++ version of UCB_CSTR */
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
inline const char* ucb_cstr_of(ucb_path* s)
{
    return ucb_path_cstr(s);
}
inline const char* ucb_cstr_of(const ucb_path* s)
{
    return ucb_path_cstr((ucb_path*)s);
}
/* Opt-in support for types exposing c_str() (e.g. std::string). SFINAE keeps
   arrays and char* out, so the overloads above win for those. */
template <class T>
inline auto ucb_cstr_of(const T& s) -> decltype(s.c_str())
{
    return s.c_str();
}
#define UCB_CSTR(x) ucb_cstr_of(x)

/* C++ version of UCB_STR */
namespace ucb_str_detail {
struct literal
{
    ucb_str s;

    literal(const char* c, size_t n)
    {
        ucb_str_verify_cstr(c, n, "literal");
        s.data = c ? const_cast<char*>(c) : const_cast<char*>("");
        s.size = n;
        s.alloc = 0;
    }

    operator const ucb_str*() const
    {
        return &s;
    }
};

template <size_t N>
inline literal of(const char (&c)[N])
{
    return literal(c, N - 1);
}

inline literal of(const char* c)
{
    return literal(c, c ? strlen(c) : 0);
}

inline literal of(char* c)
{
    return literal(c, c ? strlen(c) : 0);
}

template <class T>
inline auto of(const T& s) -> decltype(s.c_str(), s.size(), literal(s.c_str(), s.size()))
{
    return literal(s.c_str(), s.size());
}
} // namespace ucb_str_detail

#define UCB_STR(x) ::ucb_str_detail::of(x)
#else
/* C version of UCB_CSTR */
#define UCB_CSTR(x)                                        \
    _Generic((x),                                          \
        char*: (const char*)(x),                           \
        const char*: (const char*)(x),                     \
        ucb_str*: ucb_str_cstr((const ucb_str*)(x)),       \
        const ucb_str*: ucb_str_cstr((const ucb_str*)(x)), \
        ucb_path*: ucb_path_cstr((ucb_path*)(x)),          \
        const ucb_path*: ucb_path_cstr((ucb_path*)(x)))

/* C version of UCB_STR.

   Wraps @p cstr as a borrowed view in caller-provided storage and returns it.
   The length is measured once with strlen (there is no portable way to tell a
   string literal from a runtime pointer after array-to-pointer conversion), and
   the value is validated once. A UCB_NULL @p cstr becomes the interned empty
   string. The storage is a block-scope compound literal, so the returned view
   stays valid until the end of the enclosing block. */
static inline const ucb_str* ucb_str_wrap_view_(ucb_str* storage, const char* cstr)
{
    size_t len = 0;
    if (cstr && *cstr)
    {
        len = strlen(cstr);
        ucb_str_verify_cstr(cstr, len, "literal");
    }
    else
    {
        cstr = "";
    }

    storage->data = (char*)cstr;
    storage->size = len;
    storage->alloc = 0;
    return storage;
}

#define UCB_STR(x) ucb_str_wrap_view_(&(ucb_str){0}, (x))
#endif

/** @} */

#endif // UCB_STRING_EX_H
