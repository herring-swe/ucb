/**
 * @file unicode.h
 *
 * This file is part of the UCB project
 * - SPDX-FileCopyrightText: © 2026 Åke Svedin <ake@svedin.org>
 * - SPDX-License-Identifier: MIT
 *
 * @brief Unicode support
 *
 * Operates on UTF-8 strings, unless specifically documented.
 */

#ifndef UCB_UNICODE_H
#define UCB_UNICODE_H

#include <ucb/buffer.h>
#include <ucb/diag.h>
#include <ucb/export.h>
#include <ucb/types.h>
#include <ucb/unicode_enum.h>

#include <stddef.h>

#define UCB_NOCP 0xFFFF

UCB_DIAG_PUSH()
UCB_DIAG_IGN_PADDED()
/**
 * @struct ucb_uc_result
 * @brief Result of a unicode operation
 *
 * Holding a string and it's size
 */
typedef struct ucb_uc_result
{
    /**
     * @brief The resulting NULL-terminated string or UCB_NULL on error.
     * The string must be free'd with ucb_free by the caller.
     */
    char* data;
    /**
     * @brief The size of the resulting string, excluding the NULL-terminator.
     */
    size_t size;
} ucb_uc_result;
UCB_DIAG_POP()

/**
 * @brief Decode the next codepoint and advance a UTF-8 iterator.
 *
 * Reads the codepoint starting at @p *iter, advances @p *iter past it and
 * returns the codepoint.
 *
 * @warning No validation is performed. The memory must contain a complete,
 * valid UTF-8 codepoint. A null byte is decoded as U+0000, which is
 * indistinguishable from the end of a null-terminated string; use an explicit
 * length when iterating strings that may contain embedded null characters.
 *
 * @param iter in/out pointer to the current position
 * @return the decoded codepoint
 */
UCB_API ucb_cp ucb_uc_iter_utf8(const unsigned char** iter);

/**
 * @brief Encode a single codepoint as UTF-8
 *
 * The codepoint must be a valid Unicode code point (0 to 0x10FFFF, excluding surrogate pairs).
 *
 * @param dst destination buffer, must have space for at least 4 bytes. If UCB_NULL, the function
 * will return the number of bytes needed to encode the codepoint.
 * @param cp codepoint to encode
 * @return number of bytes written or needed
 * @return -1 if codepoint is invalid (outside the Unicode range or a surrogate)
 */
UCB_API int ucb_uc_encode_codepoint(uint8_t* dst, const ucb_cp cp);

/**
 * @brief Encode an array of codepoints as UTF-8 and append them to a buffer.
 *
 * The codepoints are appended to any existing content of @p buf. The buffer is
 * grown as needed. Encoding is all-or-nothing: if any codepoint is invalid the
 * function returns false and the buffer content is left unspecified.
 *
 * @param buf destination buffer
 * @param codepoints array of codepoints to encode
 * @param len number of codepoints
 * @param perr optional pointer that may be set on error
 * @return true on success
 * @return false on invalid codepoint or allocation failure
 */
UCB_API bool ucb_uc_encode_codepoints(ucb_buffer* buf,
                                      const ucb_cp* codepoints,
                                      size_t len,
                                      ucb_error** perr);

/**
 * @brief Validate a string as UTF-8
 *
 * If @p len is specified, the string may contain multiple null characters.
 * If @p len is UCB_NPOS, the string must only be null-terminated.
 * @param str null-terminated string to validate
 * @param len length of string or UCB_NPOS
 * @param perr optional pointer that may be set on error
 * @return true if the string is valid UTF-8
 * @return false otherwise, with UCB_ERROR_INVALID_UTF8 set through @p perr
 */
UCB_API bool ucb_uc_validate(const char* str, size_t len, ucb_error** perr);

/**
 * @brief Count the number of codepoints in a UTF-8 string.
 *
 * The string must already have been validated as UTF-8.
 * If @p len is specified, the string may contain multiple null characters.
 * If @p len is UCB_NPOS, the string must only be null-terminated.
 * @param str the string
 * @param len length of string or UCB_NPOS
 * @return number of codepoints in string
 */
UCB_API size_t ucb_uc_num_cp(const char* str, size_t len);

/**
 * @brief Count the number of printable characters in a UTF-8 string.
 *
 * Counts extended grapheme clusters as defined by UAX #29. This is the number
 * of user-perceived characters, and is not the same as the number of
 * codepoints (see @ref ucb_uc_num_cp).
 *
 * The string must already have been validated as UTF-8.
 *
 * If @p len is specified, the string may contain multiple null characters.
 * If @p len is UCB_NPOS, the string must only be null-terminated.
 *
 * @param str the string
 * @param len length of string or UCB_NPOS
 * @return number of printable characters in string
 */
UCB_API size_t ucb_uc_num_char(const char* str, size_t len);

/**
 * @brief Get the byte offset just past the @p index-th printable character.
 *
 * Characters are extended grapheme clusters (UAX #29). The returned offset is a
 * valid boundary that can be passed to @ref ucb_uc_next_char().
 *
 * @param str valid UTF-8 string
 * @param len length of string
 * @param index 1-based character index
 * @return byte offset after the @p index-th character, or @p len if there is no
 * such character (including @p index equal to 0, which is not a valid
 * 1-based index)
 */
UCB_API size_t ucb_uc_char_index(const char* str, size_t len, size_t index);

/**
 * @brief Get the next character position
 *
 * Characters are extended grapheme clusters (UAX #29).
 *
 * Calls with @p from_byte >= string length (including UCB_NPOS) will return UCB_NPOS.
 *
 * This function is only valid if called with
 * @p from_byte being one of:
 *   - 0 (start of string)
 *   - a value returned from previous call
 *   - value from @ref ucb_uc_char_index()
 *
 * The string must not be modified between calls.
 * @param str valid UTF-8 string
 * @param len length of string (must be specified)
 * @param from_byte byte offset to start from
 * @return byte offset to next character or UCB_NPOS if at end of string
 */
UCB_API size_t ucb_uc_next_char(const char* str, size_t len, size_t from_byte);

/**
 * @brief Map a UTF-8 string to upper case.
 *
 * The string must already have been validated as UTF-8. The mapping uses the
 * Unicode full case mapping tables and may change the number of codepoints
 * (for example "ß" maps to "SS").
 *
 * @param str valid UTF-8 string
 * @param size length of string in bytes; the string may contain embedded nulls
 * @param perr optional pointer that may be set on error
 * @return result; on success @ref ucb_uc_result::data is allocated with
 * @ref ucb_malloc and must be freed by the caller
 */
UCB_API ucb_uc_result ucb_uc_to_upper(const char* str, size_t size, ucb_error** perr);

/**
 * @brief Map a UTF-8 string to lower case.
 * @see ucb_uc_to_upper()
 * @param str valid UTF-8 string
 * @param size length of string in bytes; the string may contain embedded nulls
 * @param perr optional pointer that may be set on error
 * @return result; on success @ref ucb_uc_result::data must be freed by the caller
 */
UCB_API ucb_uc_result ucb_uc_to_lower(const char* str, size_t size, ucb_error** perr);

/**
 * @brief Map a UTF-8 string to title case.
 *
 * Each word starts with a titlecase mapping and continues in lower case. Word
 * boundaries are separators and punctuation; an apostrophe is not a separator.
 *
 * @see ucb_uc_to_upper()
 * @param str valid UTF-8 string
 * @param size length of string in bytes; the string may contain embedded nulls
 * @param perr optional pointer that may be set on error
 * @return result; on success @ref ucb_uc_result::data must be freed by the caller
 */
UCB_API ucb_uc_result ucb_uc_to_title(const char* str, size_t size, ucb_error** perr);

/**
 * @brief Case fold a UTF-8 string.
 *
 * Uses full case folding (Unicode C+F). The result is intended for
 * case-insensitive comparison, not for display.
 *
 * @see ucb_uc_to_upper()
 * @param str valid UTF-8 string
 * @param size length of string in bytes; the string may contain embedded nulls
 * @param perr optional pointer that may be set on error
 * @return result; on success @ref ucb_uc_result::data must be freed by the caller
 */
UCB_API ucb_uc_result ucb_uc_casefold(const char* str, size_t size, ucb_error** perr);

/**
 * @brief Normalize an UTF-8 string
 *
 * The string must already have been validated as UTF-8.
 *
 * If @p len is specified, the string may contain multiple null characters.
 * If @p len is UCB_NPOS, the string must only be null-terminated.
 * @param str string to normalize, must be a valid utf-8 string
 * @param len length of string or UCB_NPOS
 * @param form the normalization form to use
 * @param perr optional pointer that will be set on normalization error
 * @returns result, with the normalized string on success
 */
UCB_API ucb_uc_result ucb_uc_normalize(const char* str,
                                       size_t len,
                                       ucb_norm_form form,
                                       ucb_error** perr);

/**
 * @brief Case-insensitive comparison of two UTF-8 strings.
 *
 * Both strings are compared using full case folding, so the comparison is
 * independent of case. The strings should be in the same normalization form
 * for meaningful results.
 *
 * @param str1 first UTF-8 string
 * @param len1 length of @p str1 in bytes
 * @param str2 second UTF-8 string
 * @param len2 length of @p str2 in bytes
 * @return 0 if equal, a negative value if @p str1 sorts before @p str2 and a
 * positive value otherwise
 */
UCB_API int ucb_uc_icomp(const char* str1, size_t len1, const char* str2, size_t len2);

#endif // UCB_UNICODE_H
