/**
 * @file string.h
 *
 * This file is part of the UCB project
 * - SPDX-FileCopyrightText: © 2026 Åke Svedin <ake@svedin.org>
 * - SPDX-License-Identifier: MIT
 *
 * @brief String type and related functions
 */

#ifndef UCB_STRING_H
#define UCB_STRING_H

#include "defines.h"
#include "diag.h"
#include "error.h"
#include "export.h"
#include "types.h"
#include "unicode_enum.h"

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

/**
 * @struct ucb_str
 * @brief Represents a UTF-8 string type.
 *
 * It keeps track of the string length and capacity and can be used to
 * manipulate the string. The underlying string itself does not need to
 * be null terminated.
 *
 * Strings are always expected to be valid UTF-8 encoded unless when
 * explicitly documented and it's up to the user to validate them before
 * passing them to ucb_str functions.
 *
 * ### Owned strings
 *
 * Owned (regular) strings are strings that owns it's data pointer. Denoted
 * by alloc > 0. The string data is freed when the ucb_str is
 * released or freed.
 *
 * ### Wrapped strings
 *
 * Wrapped strings are strings that are not owned by the ucb_str instance.
 * Denoted by alloc = 0. The string data is not freed when the ucb_str is
 * released or freed.
 *
 * The data pointer in a wrapped string will never be manipulated. Any
 * manipulations on the ucb_str will first make a copy of the data and
 * change ucb_str to an owned string.
 *
 * ### Length of string
 *
 * - UCB use "len", "length" or simetimes "size" to denote the number of bytes in
 * the string.
 * - UCB use "num_char" to represent the number of perceived characters.
 *
 * @warning When using strings with multiple null characters, the length of the
 * underlying string @ref ucb_str_cstr() will not match strlen. This string is
 * not suitable to pass directly to printf or other C functions that strictly
 * expects null-terminated strings. The length from @ref ucb_str_len() will
 * be the actual length of the string.
 *
 * Details:
 *
 * There are four ways to describe the string length:
 * 1. Number of bytes.
 * 2. Number of unicode codepoints.
 * 3. Number of perceived characters.
 * 4. Number of code units *(honorable mention)*.
 *
 * Remark: For UTF-8 encoded ASCII only strings, all of the above are equal. Given
 * that it is only null-terminated at the end.
 *
 * #### 1. Bytes:
 * This is the most natural choice to represent length or "len"
 *
 * For UCB, the term length or "len" is used  to represent the number of bytes in
 * the string. This would match C's strlen and probably the most expected behaviour.
 * This also matches for instance Rust, which also use UTF-8 encoded strings.
 *
 * #### 2. Unicode codepoints:
 * While Python and others may use length as unicode codepoints, this is rarely
 * of any use unless you explicitly deal with unicode operations. The number of
 * codepoints can be checked by passing the underlying cstr to @ref ucb_uc_num_cp(),
 * in @ref unicode.h.
 *
 * #### 3. Perceived characters:
 * This is the number of characters that is seen when the string is printed to the
 * console or other UTF-8 enabled outputs. Since in unicode, characters may be
 * represented in multiple codepoints, this is not the same as previous lengths.
 *
 * This is important to know in order to truncate or split strings properly. It is
 * also useful as the measurement to check a certain column width when printing to
 * screen. Used in graphics rendering and text editing.
 *
 * #### 4. Code units:
 * The smallest unit of encoding to represent a codepoint. For UTF-8, this is
 * identical to the number of bytes (1-4 bytes per codepoint). This is useful
 * for other encodings such as UTF-16 (each code unit is 2 bytes) or UTF-32
 * (each code unit is 4 bytes). So it's not a useful term here.
 *
 * ### Thread safety
 *
 * Methods are not thread safe. It's up to the user to ensure thread safety.
 *
 * Stack allocations
 * @code
 * ucb_str str = ucb_str_make();        // Wrapped string around "" string literal
 * ucb_str str2, str3;                  // Invalid state
 * ucb_str_init_c(&str2, "my string");  // Owned string with "my string" copied
 * ucb_str_init_wrap_c(&str3, "wrap");  // Wrapped string around "wrap" string literal
 * ucb_str_release(&str);               // Only zeroes the struct
 * ucb_str_release(&str2);              // Frees its data and zeroes the struct
 * ucb_str_release(&str3);              // Only zeroes the struct
 *
 * // Example of bad initialization
 * ucb_str_init_c(&str2, "my string");  // Okay since str2 was released.
 * ucb_str_init_c(&str, "my string");   // Memory leak. str data is not verified.
 * @endcode
 *
 * Stack copies
 * @code
 * ucb_str str = ucb_str_make();        // Initialize wrapped string around "" string literal
 * ucb_str str2 = {0};                  // Invalid until intialized
 * ucb_str_copy(&str2, &str);           // Explicit copy, always make str2 an owned string.
 * ucb_str_release(&str2);              // Free heap allocated "" and zeroes str2
 * ucb_str_release(&str);               // No heap deallocation needed, only zeroes the struct
 * @endcode
 *
 * Heap allocation
 * @code
 * ucb_str* str = ucb_str_new_empty();         // New wrapped string around "" string literal
 * ucb_str* str2 = ucb_str_new_c("my string"); // New owned string with "my string" copied
 * ucb_str* str3 = ucb_str_new_wrap_c("wrap")  // New wrapped string around "wrap" string literal
 * ucb_str_free(str);                          // Frees ucb_str but not its data
 * ucb_str_free(str2);                         // Frees ucb_str and its data
 * ucb_str_free(str3);                         // Frees ucb_str but not its data
 * @endcode
 *
 * Heap copies
 * @code
 * ucb_str* str = ucb_str_new_empty();    // New wrapped string around "" string literal();
 * ucb_str* str2 = ucb_str_clone(str);    // Deep-copy, returns new string that owns data
 * ucb_str* str3 = ucb_str_new_empty();   // New wrapped string around "" string literal
 * ucb_str_copy(str3, str);               // Explicit copy, releases str3 and turn into owned string
 * ucb_str_free(str);                     // Frees ucb_str but not its data
 * ucb_str_free(str2);                    // Frees ucb_str and its data
 * ucb_str_free(str3);                    // Frees ucb_str and its data
 * @endcode
 */
typedef struct ucb_str
{
    char* data;   ///< Pointer to string data
    size_t size;  ///< String bytes excluding null-terminator
    size_t alloc; ///< Data allocated bytes
} ucb_str;

/**
 * @name Construction
 * @{
 */

/**
 * @brief Helper function for stack initialization of an empty string
 * @return an empty string
 */
static inline ucb_str ucb_str_make()
{
    ucb_str str;
    str.data = "";
    str.size = 0;
    str.alloc = 0;
    return str;
}

/**
 * @brief Allocate and initialize a new owned string from a C string.
 *
 * If @p cstr is UCB_NULL, it is treated as the empty string and @p len will be
 * ignored.
 *
 * If @p len is zero, the string must be null-terminated and will be measured.
 * If @p len is non-zero, the string may contain multiple null characters.
 *
 * The returned pointer must always be freed with @ref ucb_str_free()
 *
 * @param cstr a C string or literal
 * @param len 0 or length of string excluding null-terminator
 * @return pointer to new ucb_str or UCB_NULL on error.
 */
UCB_API ucb_str* ucb_str_new(const char* cstr, size_t len);
static inline ucb_str* ucb_str_new_c(const char* cstr)
{
    return ucb_str_new(cstr, 0);
}

/**
 * @brief Allocate and initialize a wrapped string from a C string without copying data.
 *
 * Otherwise behaves same as same as @ref ucb_str_new()
 *
 * @note If the string is not null-terminated, it will become an owned string.
 * @param cstr a C string or literal
 * @param len length of string excluding null-terminator, or 0
 * @return pointer to new ucb_str or UCB_NULL on error.
 */
UCB_API ucb_str* ucb_str_new_wrap(const char* cstr, size_t len);
static inline ucb_str* ucb_str_new_wrap_c(const char* cstr)
{
    return ucb_str_new_wrap(cstr, 0);
}

/**
 * @brief Allocate and initialize an empty wrapped string around "" string literal.
 * @return pointer to new ucb_str or UCB_NULL on error.
 */
static inline ucb_str* ucb_str_new_empty(void)
{
    return ucb_str_new_wrap("", 0);
}

/**
 * @brief Allocate and initialize a string as a copy of another string.
 *
 * The returned string will always own its data.
 * @param src string to clone
 * @return pointer to new ucb_str or UCB_NULL on error.
 */
UCB_API ucb_str* ucb_str_clone(const ucb_str* src);

/**
 * @brief Initialize an owned string from a C string.
 *
 * If @p cstr is UCB_NULL, it is treated as the empty string and @p len will be
 * ignored.
 *
 * If @p len is zero, the string must be null-terminated and will be measured.
 * If @p len is non-zero, the string may contain multiple null characters.
 *
 * str must always be released with @ref ucb_str_release().
 *
 * @warning It doesn't perform any verifications on @p str data, which may lead to leaks
 * if the string is already initialized.
 * @param str string to initialize
 * @param cstr a C string or literal or UCB_NULL
 * @param len 0 or length of string excluding null-terminator
 * @return true on success, false on allocation error
 */
UCB_API bool ucb_str_init(ucb_str* str, const char* cstr, size_t len);
static inline void ucb_str_init_c(ucb_str* str, const char* cstr)
{
    ucb_str_init(str, cstr, 0);
}

/**
 * @brief Initialize a wrapped string around a C string.
 *
 * Otherwise behaves same as same as @ref ucb_str_init()
 *
 * @param str string to initialize
 * @param cstr a C string or literal or UCB_NULL
 * @param len 0 or length of string excluding null-terminator
 */
UCB_API void ucb_str_init_wrap(ucb_str* str, const char* cstr, size_t len);
static inline void ucb_str_init_wrap_c(ucb_str* str, const char* cstr)
{
    ucb_str_init_wrap(str, cstr, 0);
}

/**
 * @brief Initialize an empty wrapped string around "" string literal.
 */
static inline void ucb_str_init_empty(ucb_str* str)
{
    ucb_str_init_wrap(str, "", 0);
}

/** @} */

/**
 * @name Destruction
 * @{
 */

/**
 * @brief Release a string
 *
 * The string must have been initialized with @ref ucb_str_init()
 * @param str string to release
 */
UCB_API void ucb_str_release(ucb_str* str);

/**
 * @brief Free a string
 *
 * The string must have been allocated with @ref ucb_str_new()
 * @param str string to free
 */
UCB_API void ucb_str_free(ucb_str* str);

/** @} */

/**
 * @name Assignment and data update
 *
 * Applies on initialized strings.
 * Includes copy, but not clone.
 * @{
 */

/**
 * @brief Copy a string
 *
 * @p dst will release any current data and initialize itself to a copy the string from @p src.
 * @p dst will always be an owned string.
 * @param dst destination string
 * @param src source string
 * @return true on success, false on allocation error
 */
UCB_API bool ucb_str_copy(ucb_str* dst, const ucb_str* src);

/**
 * @brief Assign a C string.
 *
 * Behaves as if calling @ref ucb_str_release() followed by @ref ucb_str_init().
 */
UCB_API bool ucb_str_assign(ucb_str* str, const char* cstr, size_t len);
static inline bool ucb_str_assign_c(ucb_str* str, const char* cstr)
{
    return ucb_str_assign(str, cstr, 0);
}

/**
 * @brief Creates own copy of the underlying string, null-terminated.
 *
 * If @p str is an owned string, this call is a no-op
 * If @p str is a wrapped string, a new allocation will be made,
 * and the string will be owned and null-terminated.
 *
 * @param str string to detach
 * @return true if the string was modified
 * @return false if the string is already owned or on out of memory
 */
UCB_API bool ucb_str_detach(ucb_str* str);

/**
 * @brief Shrink data allocation to fit the string.
 *
 * If the current capacity is larger than the string length, the allocation will
 * keep shrink to length + 1 and set the null-terminator.
 *
 * This is a no-op for wrapped strings or if the allocation is already fitted.
 * @param str string to shrink
 * @return true if the string was modified
 */
UCB_API bool ucb_str_fit(ucb_str* str);

/**
 * @brief Ensure at least @p size bytes of free space in string.
 *
 * The current size is always the string length, regardless of null-terminator.
 * It will add an extra byte when reallocating to ensure null-termination.
 *
 * Wrapped strings will be detached and the current string copied over.
 *
 * This is a no-op if the string is already large enough.
 * @param str string to update
 * @param size bytes of free space to ensure.
 * @return true if the string has enough space.
 * @return false if out of memory
 */
UCB_API bool ucb_str_reserve(ucb_str* str, size_t size);

/**
 * @brief Wrap a C string pointer.
 *
 * Behaves as if calling @ref ucb_str_release() followed by @ref ucb_str_init_wrap().
 */
UCB_API void ucb_str_wrap(ucb_str* str, const char* cstr, size_t len);
static inline void ucb_str_wrap_c(ucb_str* str, const char* cstr)
{
    ucb_str_wrap(str, cstr, 0);
}

/**
 * @brief Adopt memory as a string.
 *
 * Release current data and take ownership of @p data, which will be
 * freed when @ref ucb_str_free() or @ref ucb_str_release() is called.
 *
 * The @p data must be allocated with ucb functions.
 * The caller must not manipulate @p data after this call.
 *
 * If @p len is zero, the string must be null-terminated and will be measured.
 * If @p len is non-zero, the string may contain multiple null characters.
 *
 * If @p alloc is zero, the allocation size is assumed to be @p len bytes.
 *
 * @todo Add pointer verification in <ucb/memdbg.h> for debug builds
 *
 * @param str string to update
 * @param data string data to adopt
 * @param len 0 or length of string excluding null-terminator
 * @param alloc 0 or size of allocation
 */
UCB_API void ucb_str_adopt(ucb_str* str, char* data, size_t len, size_t alloc);

/**
 * @brief Adopt a null-terminated C string
 *
 * For when the memory allocated for the C string fits the string + null-terminator.
 *
 * @see ucb_str_adopt()
 * @param str string to update
 * @param cstr a null-terminated C string
 */
UCB_API void ucb_str_adopt_c(ucb_str* str, char* cstr);

/**
 * @brief Abandon managed data and return the underlying string.
 *
 * The string data is no longer managed by @p str and must be managed by the caller.
 *
 * @p str will be zeroed (same as calling @ref ucb_str_release()). If @p str is
 * heap allocated it must be followed by @ref ucb_str_free(). It can be Reinitialized
 * with any @ref ucb_str_init function.
 *
 * @note This call will abandon the string, owned or not. Take a note of the return value.
 *
 * @param str the string
 * @param data output pointer to string data
 * @param len optional output pointer to C string length
 * @param alloc optional output pointer to C string allocated size
 * @return true if the string was owned (caller must free)
 * @return false if the string was wrapped
 */
UCB_API bool ucb_str_abandon(ucb_str* str, char** data, size_t* len, size_t* alloc);

/**
 * @brief Abandon managed data and return the underlying string.
 *
 * Convenience function for @ref ucb_str_abandon() when only the string is needed.
 *
 * @param str the string
 * @return char* the underlying string
 */
static inline char* ucb_str_abandon_c(ucb_str* str)
{
    char* cstr;
    ucb_str_abandon(str, &cstr, UCB_NULL, UCB_NULL);
    return cstr;
}

/** @} */

/**
 * @name Conversion
 * @{
 */

#ifdef _WIN32
/**
 * Creates a UTF-8 string from a Windows wide string
 * @param wstr UTF-16 string
 * @param wlen length of the UTF-16 string, can be 0 if the string is null-terminated and length
 *             will be determined.
 * @return ucb_str* UTF-8 string or UCB_NULL on error
 */
UCB_API ucb_str* ucb_str_from_wchar(const wchar_t* wstr, size_t wlen, const ucb_error** perr);

/**
 * Creates a Windows wide string from a UTF-8 string
 * @param str UTF-8 string
 * @param wlen_out optional pointer, set to length of the returned string
 * @return wchar_t* UTF-16 string or UCB_NULL on error
 */
UCB_API wchar_t* ucb_str_to_wchar(const ucb_str* str, size_t* wlen_out, const ucb_error** perr);
#endif

/** @} */

/**
 * @name Querying
 * @{
 */

/**
 * @brief Check if the underlying data is owned by @p str.
 * @param str string to query
 * @return true if str is an owned string
 * @return false if str is a wrapped string
 */
UCB_API bool ucb_str_is_owned(const ucb_str* str);

/**
 * @brief Check if the string is empty
 * @param str string to query
 * @return true if the string is empty
 */
UCB_API bool ucb_str_is_empty(const ucb_str* str);

/**
 * @brief Get the allocated size of the underlying data
 *
 * This returns 0 for wrapped strings.
 * @param str string to query
 * @return size in bytes
 */
UCB_API size_t ucb_str_capacity(const ucb_str* str);

/**
 * @brief Get the used capacity of the underlying data
 *
 * This return 0 for wrapped strings.
 * This includes any eventual null-terminator.
 * @param str string to query
 * @return size in bytes
 */
UCB_API size_t ucb_str_used(const ucb_str* str);

/**
 * @brief Get the free capacity of the underlying data
 *
 * This excludes any eventual null-terminator
 * @param str string to query
 * @return size in bytes
 */
UCB_API size_t ucb_str_avail(const ucb_str* str);

/**
 * @brief Get a pointer to the underlying C string
 *
 * @note Do not manipulate this string.
 * @param str string to query
 * @return const pointer to the underlying C string
 */
UCB_API const char* ucb_str_cstr(const ucb_str* str);

/**
 * @brief Get the length of the underlying string
 *
 * This is the number of bytes in the UTF-8 encoded string.
 * If properly null-terminated strings with no embedded null characters,
 * this is same as strlen.
 *
 * @param str string to query
 * @return length in bytes
 */
UCB_API size_t ucb_str_len(const ucb_str* str);

/**
 * @brief Get the number of perceived characters in the string.
 *
 * This is the number of characters that would be printed to the screen.
 * For instance it could count a starter + multiple combining characters
 * into a single character.
 *
 * @note This is not the same as the number of unicode codepoints
 * @param str
 * @return length in printable characters
 */
UCB_API size_t ucb_str_num_char(const ucb_str* str);

/** @} */

/**
 * @name Comparison and lookup
 *
 * Strings should be in same normalization form before comparison.
 *
 * Casefolding will be used for case-insensitive comparison. For best-performance,
 * when comparing strings more than once, casefold them first and compare with
 * case sensitive comparison.
 *
 * @see ucb_str_normalize() and ucb_str_casefold()
 * @{
 */

UCB_API bool ucb_str_equal(const ucb_str* str1, const ucb_str* str2);

UCB_API int ucb_str_comp(const ucb_str* str1, const ucb_str* str2);
UCB_API int ucb_str_icomp(const ucb_str* str1, const ucb_str* str2);

UCB_API bool ucb_str_startswith(const ucb_str* str, const ucb_str* prefix);
UCB_API bool ucb_str_endswith(const ucb_str* str, const ucb_str* suffix);
UCB_API size_t ucb_str_find(const ucb_str* str, const ucb_str* substr, size_t pos);

/**
 * @brief Get the next character position
 *
 * @see ucb_uc_next_char()
 * @param str string to search
 * @param from_byte byte offset to start from
 * @return byte offset to next character or UCB_NPOS if at end of string
 */
UCB_API size_t ucb_str_next_char(const ucb_str* str, size_t from_byte);

/** @} */

/**
 * @name Modification
 *
 * All of the functions in this group will detach the string if it's
 * a wrapped string.
 * @{
 */

/**
 * @brief Clear the string
 *
 * Set the string to empty without modifying the allocation.
 * @param str string to update
 */
UCB_API void ucb_str_clear(ucb_str* str);

/**
 * @brief Append another string to the end of this string
 * @param str string to append to
 * @param append string to append
 */
UCB_API void ucb_str_append(ucb_str* str, const ucb_str* append);
UCB_API void ucb_str_append_cp(ucb_str* str,
                               const ucb_cp* cp,
                               size_t num_cp,
                               const ucb_error** perr);
UCB_API void ucb_str_append_utf8(ucb_str* str, const char* cstr, size_t len);

/**
 * @brief Insert a string into another string at a specific character index
 *
 * @note Insert at character index, not byte index
 * @param str string to insert into
 * @param index character index to insert at
 * @param insert string to insert
 */
UCB_API void ucb_str_insert(ucb_str* str, size_t index, const ucb_str* insert);
UCB_API void ucb_str_insert_cp(ucb_str* str,
                               size_t index,
                               const ucb_cp* cp,
                               size_t num_cp,
                               const ucb_error** perr);
UCB_API void ucb_str_insert_utf8(ucb_str* str, size_t index, const char* cstr, size_t len);

UCB_API ucb_str* ucb_str_concat(const ucb_str* str1, const ucb_str* str2);

/**
 * @brief Allocate and initialize a substring from a string
 *
 * The substring will be a wrapped string of the original @p str underlying
 * C string.
 * @param str string to copy from
 * @param index start index of character byte
 * @param count number of characters or UCB_NPOS until end of string
 * @return ucb_str*
 */
UCB_API ucb_str* ucb_str_substr(const ucb_str* str, size_t index, size_t count);
UCB_API ucb_str* ucb_str_substr_wrapped(const ucb_str* str, size_t index, size_t count);

UCB_API bool ucb_str_to_lower(ucb_str* str);
UCB_API bool ucb_str_to_upper(ucb_str* str);
UCB_API bool ucb_str_to_title(ucb_str* str);
UCB_API bool ucb_str_casefold(ucb_str* str);
UCB_API bool ucb_str_normalize(ucb_str* str, ucb_norm_form form);

/** @} */

#endif // UCB_STRING_H
