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

#include <ucb/defines.h>
#include <ucb/diag.h>
#include <ucb/error.h>
#include <ucb/export.h>
#include <ucb/types.h>
#include <ucb/unicode_enum.h>

#include <stdarg.h>
#include <stdbool.h>
#include <stdint.h>
#include <string.h>

/**
 * @struct ucb_str
 * @brief Represents a UTF-8 string type.
 *
 * It keeps track of the string length and capacity and can be used to
 * manipulate the string.
 *
 * Strings are always expected to be valid UTF-8 encoded unless when
 * explicitly documented and it's up to the user to validate them before
 * passing them to ucb_str functions.
 *
 * ### Ownership
 *
 * A @ref ucb_str is either an owned string or the interned empty string:
 *
 * - Owned strings (alloc > 0) own their data pointer. The data is freed when
 *   the ucb_str is released or freed.
 * - The interned empty string (alloc == 0, data == "" and size == 0) is a
 *   shared, non-owned literal used by @ref ucb_str_make(), @ref ucb_str_new_empty()
 *   and @ref ucb_str_init_empty(). It requires no deallocation.
 *
 * Apart from the interned empty string, every ucb_str owns its data.
 *
 * ### Invariants
 *
 * For every valid ucb_str:
 * - @c data is never UCB_NULL.
 * - The data is null-terminated: data[size] == '\0'.
 * - There is no embedded null character: memchr(data, '\0', size) == NULL.
 *
 * This makes @ref ucb_str_cstr() directly usable with C functions that expect
 * null-terminated strings.
 *
 * Passing an explicit length that contains an embedded null character, or
 * appending or inserting one, is a user error and always aborts.
 *
 * ### Length of string
 *
 * - UCB use "len", "length" or sometimes "size" to denote the number of bytes in
 * the string.
 * - UCB use "num_char" to represent the number of perceived characters.
 *
 * Details:
 *
 * There are four ways to describe the string length:
 * 1. Number of bytes.
 * 2. Number of unicode codepoints.
 * 3. Number of perceived characters.
 * 4. Number of code units *(honorable mention)*.
 *
 * Remark: For UTF-8 encoded ASCII only strings, all of the above are equal.
 *
 * #### 1. Bytes:
 * This is the most natural choice to represent length or "len"
 *
 * For UCB, the term length or "len" is used to represent the number of bytes in
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
 * UCB measures these as extended grapheme clusters (UAX #29).
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
 * ucb_str str = ucb_str_make();        // Interned empty string ("" literal)
 * ucb_str str2;                        // Invalid state
 * ucb_str_init_c(&str2, "my string");  // Owned string with "my string" copied
 * ucb_str_release(&str);               // Only zeroes the struct (interned empty)
 * ucb_str_release(&str2);              // Frees its data and zeroes the struct
 *
 * // Reinitialization is only safe after a release
 * ucb_str_init_c(&str2, "again");      // Okay, str2 was released above
 * ucb_str_init_c(&str2, "again");      // Memory leak. str2 data is not verified.
 * @endcode
 *
 * Stack copies
 * @code
 * ucb_str str = ucb_str_make();        // Interned empty string
 * ucb_str str2 = {0};                  // Invalid until intialized
 * ucb_str_copy(&str2, &str);           // Explicit copy, always makes str2 an owned string.
 * ucb_str_release(&str2);              // Frees its data and zeroes str2
 * ucb_str_release(&str);               // No heap deallocation needed, only zeroes the struct
 * @endcode
 *
 * Heap allocation
 * @code
 * ucb_str* str = ucb_str_new_empty();         // Interned empty string
 * ucb_str* str2 = ucb_str_new_c("my string"); // New owned string with "my string" copied
 * ucb_str_free(str);                          // Frees ucb_str but not its data (interned empty)
 * ucb_str_free(str2);                         // Frees ucb_str and its data
 * @endcode
 *
 * Heap copies
 * @code
 * ucb_str* str = ucb_str_new_empty();    // Interned empty string
 * ucb_str* str2 = ucb_str_clone(str);    // Deep-copy, returns new owned string
 * ucb_str* str3 = ucb_str_new_empty();   // Interned empty string
 * ucb_str_copy(str3, str);               // Explicit copy, releases str3 and makes it owned
 * ucb_str_free(str);                     // Frees ucb_str but not its data (interned empty)
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
    str.data = (char*)"";
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
 * If @p len is non-zero, the string must not contain an embedded null
 * character; passing one is a user error and always aborts.
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
 * @brief Allocate and initialize an empty string (interned "" literal).
 * @return pointer to new ucb_str or UCB_NULL on error.
 */
static inline ucb_str* ucb_str_new_empty(void)
{
    return ucb_str_new(UCB_NULL, 0);
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
 * If @p len is non-zero, the string must not contain an embedded null
 * character; passing one is a user error and always aborts.
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
 * @brief Initialize an empty string (interned "" literal).
 */
static inline void ucb_str_init_empty(ucb_str* str)
{
    ucb_str_init(str, UCB_NULL, 0);
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
 *
 * If @p len is zero, the string must be null-terminated and will be measured.
 * If @p len is non-zero, the string must not contain an embedded null
 * character; passing one is a user error and always aborts.
 */
UCB_API bool ucb_str_assign(ucb_str* str, const char* cstr, size_t len);
static inline bool ucb_str_assign_c(ucb_str* str, const char* cstr)
{
    return ucb_str_assign(str, cstr, 0);
}

/**
 * @brief Shrink data allocation to fit the string.
 *
 * If the current capacity is larger than the string length, the allocation will
 * keep shrink to length + 1 and set the null-terminator.
 *
 * This is a no-op if the allocation is already fitted.
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
 * This is a no-op if the string is already large enough.
 * @param str string to update
 * @param size bytes of free space to ensure.
 * @return true if the string has enough space.
 * @return false if out of memory
 */
UCB_API bool ucb_str_reserve(ucb_str* str, size_t size);

/**
 * @brief Adopt memory as a string.
 *
 * Release current data and take ownership of @p data, which will be
 * freed when @ref ucb_str_free() or @ref ucb_str_release() is called.
 *
 * The @p data must be allocated with ucb functions.
 * The caller must not manipulate @p data after this call.
 *
 * If @p len is zero, the string must be null-terminated and will be measured
 * with strlen.
 *
 * @p data must be null-terminated at @c data[len] and must be large enough to
 * hold it: @p alloc must be at least <tt>len + 1</tt>. In addition, the string
 * up to @p len must not contain an embedded null character. Violating any of
 * these is a user error and always aborts.
 *
 * @todo Add pointer verification in <ucb/memdbg.h> for debug builds
 *
 * @param str string to update
 * @param data string data to adopt
 * @param len 0 or length of string excluding null-terminator
 * @param alloc size of allocation, must be at least <tt>len + 1</tt>
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
 * @note This call will abandon the string. Take a note of the return value:
 * only owned data may be freed by the caller.
 *
 * @param str the string
 * @param data output pointer to string data
 * @param len optional output pointer to C string length
 * @param alloc optional output pointer to C string allocated size
 * @return true if the string was owned (caller must free)
 * @return false if the string was the interned empty (caller must not free)
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
UCB_API ucb_str* ucb_str_from_wchar(const wchar_t* wstr, size_t wlen, ucb_error** perr);

/**
 * Creates a Windows wide string from a UTF-8 string
 * @param str UTF-8 string
 * @param wlen_out optional pointer, set to length of the returned string
 * @return wchar_t* UTF-16 string or UCB_NULL on error
 */
UCB_API wchar_t* ucb_str_to_wchar(const ucb_str* str, size_t* wlen_out, ucb_error** perr);
#endif

/** @} */

/**
 * @name Querying
 * @{
 */

/**
 * @brief Check if the string is empty
 * @param str string to query
 * @return true if the string is empty
 */
UCB_API bool ucb_str_is_empty(const ucb_str* str);

/**
 * @brief Get the allocated size of the underlying data
 *
 * This returns 0 for the interned empty string.
 * @param str string to query
 * @return size in bytes
 */
UCB_API size_t ucb_str_capacity(const ucb_str* str);

/**
 * @brief Get the used capacity of the underlying data
 *
 * This includes the null-terminator for owned strings and is 0 for the
 * interned empty string.
 * @param str string to query
 * @return size in bytes
 */
UCB_API size_t ucb_str_used(const ucb_str* str);

/**
 * @brief Get the free capacity of the underlying data
 *
 * This excludes the null-terminator.
 * @param str string to query
 * @return size in bytes
 */
UCB_API size_t ucb_str_avail(const ucb_str* str);

/**
 * @brief Get a pointer to the underlying C string
 *
 * The returned pointer is guaranteed to be null-terminated with no embedded
 * null characters, so it can be passed directly to C functions. It is borrowed
 * and remains valid until the string is modified or released.
 *
 * @note Do not manipulate this string.
 * @param str string to query
 * @return const pointer to the underlying C string
 */
UCB_API const char* ucb_str_cstr(const ucb_str* str);

/**
 * @brief Get the length of the underlying string
 *
 * This is the number of bytes in the UTF-8 encoded string, and is always equal
 * to <tt>strlen(ucb_str_cstr(str))</tt>.
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
 * @param str string to query
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

/**
 * @brief Check if two strings are equal.
 *
 * Comparison is a byte-wise (and therefore codepoint-wise) comparison. The
 * strings should be in the same normalization form. This is a case sensitive
 * comparison; use @ref ucb_str_icomp() for case-insensitive comparison.
 *
 * @param str1 first string
 * @param str2 second string
 * @return true if the strings have identical length and contents
 */
UCB_API bool ucb_str_equal(const ucb_str* str1, const ucb_str* str2);

/**
 * @brief Compare two strings byte-wise.
 *
 * @param str1 first string
 * @param str2 second string
 * @return 0 if equal, a negative value if @p str1 sorts before @p str2 and a
 * positive value otherwise
 */
UCB_API int ucb_str_comp(const ucb_str* str1, const ucb_str* str2);

/**
 * @brief Compare two strings case-insensitively.
 *
 * Uses full case folding. For repeated comparisons, case fold the strings once
 * with @ref ucb_str_casefold() and use @ref ucb_str_comp() instead.
 *
 * @param str1 first string
 * @param str2 second string
 * @return 0 if equal, a negative value if @p str1 sorts before @p str2 and a
 * positive value otherwise
 */
UCB_API int ucb_str_icomp(const ucb_str* str1, const ucb_str* str2);

/**
 * @brief Comparison adapter for @c qsort and similar functions.
 *
 * @param a pointer to a @ref ucb_str
 * @param b pointer to a @ref ucb_str
 * @return as @ref ucb_str_comp()
 */
static inline int ucb_str_cmp_func(const void* a, const void* b)
{
    return ucb_str_comp((const ucb_str*)a, (const ucb_str*)b);
}

/**
 * @brief Check if a string starts with a prefix.
 * @param str string to test
 * @param prefix prefix to look for
 * @return true if @p str starts with @p prefix
 */
UCB_API bool ucb_str_startswith(const ucb_str* str, const ucb_str* prefix);

/**
 * @brief Check if a string ends with a suffix.
 * @param str string to test
 * @param suffix suffix to look for
 * @return true if @p str ends with @p suffix
 */
UCB_API bool ucb_str_endswith(const ucb_str* str, const ucb_str* suffix);

/**
 * @brief Find the first occurrence of a substring.
 *
 * @param str string to search
 * @param substr substring to find
 * @param pos byte offset to start searching from
 * @return byte offset of the first occurrence at or after @p pos, or
 * UCB_NPOS if not found
 */
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
 * @{
 */

/**
 * @brief Clear the string
 *
 * Frees the current allocation and resets the string to the interned empty
 * string (alloc == 0, data == "", size == 0).
 * @param str string to update
 */
UCB_API void ucb_str_clear(ucb_str* str);

/**
 * @brief Append another string to the end of this string
 * @param str string to append to
 * @param append string to append
 */
UCB_API void ucb_str_append(ucb_str* str, const ucb_str* append);

/**
 * @brief Append an array of codepoints, encoded as UTF-8.
 *
 * On an invalid codepoint, @p perr is set and nothing is appended.
 *
 * A codepoint of zero (U+0000) is rejected as a user error and always aborts,
 * since ucb_str never contains embedded null characters.
 *
 * @param str string to append to
 * @param cp array of codepoints
 * @param num_cp number of codepoints
 * @param perr optional pointer that may be set on error
 */
UCB_API void ucb_str_append_cp(ucb_str* str, const ucb_cp* cp, size_t num_cp, ucb_error** perr);

/**
 * @brief Append a C string of a given byte length.
 *
 * If @p len is 0, @p cstr must be null-terminated and its length is measured.
 * If @p len is non-zero, @p cstr must not contain an embedded null character;
 * passing one is a user error and always aborts.
 *
 * This is safe when @p cstr points into this string (for example when appending
 * a slice of the string to itself).
 *
 * @param str string to append to
 * @param cstr C string to append
 * @param len length in bytes, or 0 to measure @p cstr
 */
UCB_API void ucb_str_append_cstr(ucb_str* str, const char* cstr, size_t len);
static inline void ucb_str_append_c(ucb_str* str, const char* cstr)
{
    ucb_str_append_cstr(str, cstr, strlen(cstr));
}

/**
 * @brief Insert a string into another string at a specific character index
 *
 * @note Insert at character index, not byte index
 * @param str string to insert into
 * @param index character index to insert at
 * @param insert string to insert
 */
UCB_API void ucb_str_insert(ucb_str* str, size_t index, const ucb_str* insert);

/**
 * @brief Insert an array of codepoints, encoded as UTF-8, at a character index.
 *
 * On an invalid codepoint, @p perr is set and nothing is inserted.
 *
 * A codepoint of zero (U+0000) is rejected as a user error and always aborts,
 * since ucb_str never contains embedded null characters.
 *
 * @note Insert at character index, not byte index
 * @param str string to insert into
 * @param index character index to insert at
 * @param cp array of codepoints
 * @param num_cp number of codepoints
 * @param perr optional pointer that may be set on error
 */
UCB_API void ucb_str_insert_cp(ucb_str* str,
                               size_t index,
                               const ucb_cp* cp,
                               size_t num_cp,
                               ucb_error** perr);

/**
 * @brief Insert a C string at a character index.
 *
 * If @p len is 0, @p cstr must be null-terminated and its length is measured.
 * If @p len is non-zero, @p cstr must not contain an embedded null character;
 * passing one is a user error and always aborts.
 *
 * This is safe when @p cstr points into this string (for example when inserting
 * a slice of the string into itself).
 *
 * @note Insert at character index, not byte index
 * @param str string to insert into
 * @param index character index to insert at
 * @param cstr C string to insert
 * @param len length in bytes, or 0 to measure @p cstr
 */
UCB_API void ucb_str_insert_cstr(ucb_str* str, size_t index, const char* cstr, size_t len);
static inline void ucb_str_insert_c(ucb_str* str, size_t index, const char* cstr)
{
    ucb_str_insert_cstr(str, index, cstr, strlen(cstr));
}

/**
 * @brief Allocate and concatenate a list of strings.
 *
 * The argument list must be terminated with UCB_NULL.
 *
 * @param str first string (may not be UCB_NULL)
 * @param args null-terminated va_list of @ref ucb_str pointers to append
 * @return pointer to the new owned string or UCB_NULL on error
 */
UCB_API ucb_str* ucb_str_concatv(const ucb_str* str, va_list args);

/**
 * @brief Allocate and concatenate a list of strings.
 *
 * The variadic arguments must be @ref ucb_str pointers terminated with
 * UCB_NULL.
 *
 * @code
 * ucb_str* result = ucb_str_concat(a, b, c, UCB_NULL);
 * @endcode
 *
 * @param str first string (may not be UCB_NULL)
 * @param ... null-terminated list of @ref ucb_str pointers to append
 * @return pointer to the new owned string or UCB_NULL on error
 */
UCB_API ucb_str* ucb_str_concat(const ucb_str* str, ...);

/**
 * @brief Allocate and initialize a substring from a string.
 *
 * The range is given as byte offsets: the result contains the bytes in
 * <tt>[start, end)</tt>. If @p end is UCB_NPOS, the end of the string is used.
 *
 * The returned string always owns its data.
 *
 * @param str string to copy from
 * @param start start byte offset
 * @param end end byte offset (exclusive), or UCB_NPOS for the end of @p str
 * @return pointer to the new owned string or UCB_NULL on error
 */
UCB_API ucb_str* ucb_str_substr(const ucb_str* str, size_t start, size_t end);

/**
 * @brief Convert the string to lower case in place.
 *
 * The case mapping may change the length of the string.
 *
 * @param str string to convert
 * @return true on success
 * @return false on error or out of memory
 */
UCB_API bool ucb_str_to_lower(ucb_str* str);

/**
 * @brief Convert the string to upper case in place.
 * @see ucb_str_to_lower()
 * @param str string to convert
 * @return true on success
 * @return false on error or out of memory
 */
UCB_API bool ucb_str_to_upper(ucb_str* str);

/**
 * @brief Convert the string to title case in place.
 * @see ucb_str_to_lower()
 * @param str string to convert
 * @return true on success
 * @return false on error or out of memory
 */
UCB_API bool ucb_str_to_title(ucb_str* str);

/**
 * @brief Case fold the string in place.
 *
 * Intended for case-insensitive comparisons, not for display.
 *
 * @see ucb_str_to_lower()
 * @param str string to fold
 * @return true on success
 * @return false on error or out of memory
 */
UCB_API bool ucb_str_casefold(ucb_str* str);

/**
 * @brief Normalize the string in place.
 *
 * @param str string to normalize; must be valid UTF-8
 * @param form the normalization form to apply
 * @return true on success
 * @return false on error or out of memory
 */
UCB_API bool ucb_str_normalize(ucb_str* str, ucb_norm_form form);

/** @} */

#endif // UCB_STRING_H
