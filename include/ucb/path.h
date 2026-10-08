/**
 * @file path.h
 *
 * This file is part of the UCB project
 * - SPDX-FileCopyrightText: © 2026 Åke Svedin <ake@svedin.org>
 * - SPDX-License-Identifier: MIT
 *
 * @brief Path type and related functions
 */

#ifndef UCB_PATH_H
#define UCB_PATH_H

#include <ucb/container/impl/vector_str.h>
#include <ucb/defines.h>
#include <ucb/error.h>
#include <ucb/export.h>
#include <ucb/string.h>
#include <ucb/types.h>

#include <stdbool.h>
#include <stddef.h>
#include <string.h>

/**
 * @struct ucb_path
 * @brief Represents a decomposed filesystem path.
 *
 * A path is stored as three owned components:
 * - @ref dir  - directory/anchor part, without a trailing separator except
 *   when it is exactly a root (`/`, `C:\`, `\\server\share`).
 * - @ref stem - file name without extension.
 * - @ref ext  - extension including the leading dot, or empty.
 *
 * The components are always valid @ref ucb_str instances and follow their
 * invariants. This is a lexical/type-level decomposition only; it never
 * touches the filesystem and never resolves symlinks.
 *
 * ### Ownership
 *
 * A @ref ucb_path is either stack initialized with @ref ucb_path_make() plus
 * the @ref ucb_path_init* functions, or heap allocated with @ref ucb_path_new*.
 * In both cases the contained strings own their data and are released with
 * @ref ucb_path_release() (stack) or @ref ucb_path_free() (heap).
 *
 * ### Private fields
 *
 * The @c _name, @c _cache and @c _dirty fields are private caches. Do not read
 * or modify them directly. Change the path through the provided setters and
 * mutating operations, which keep the caches coherent.
 *
 * ### Length contract
 *
 * For every function taking a @c (const char* cstr, size_t len) pair:
 * - If @p len is 0, @p cstr must be null-terminated and its length is measured.
 * - If @p len is non-zero, @p cstr must not contain an embedded null character;
 *   passing one is a user error and always aborts.
 *
 * This matches the contract of @ref ucb_str.
 *
 * ### Parsing and composition
 *
 * Parsing accepts `/` on all platforms and additionally `\` on Windows,
 * regardless of the configured @ref ucb_path_style. The style only controls
 * how a path is composed back into a string.
 *
 * Extensions follow the pathlib / std::filesystem defaults: the extension
 * includes the leading dot, a leading run of dots stays in the stem
 * (`.bashrc` has no extension), and a trailing dot alone is not an extension
 * (`a.` has no extension).
 *
 * ### Thread safety
 *
 * Methods are not thread safe. It's up to the user to ensure thread safety.
 *
 * Stack allocation
 * @code
 * ucb_path path = ucb_path_make();          // Empty path
 * ucb_path_init_c(&path, "a/b/c.txt");      // Parse into components
 * ucb_path_release(&path);                  // Free components and zero
 * @endcode
 *
 * Heap allocation
 * @code
 * ucb_path* path = ucb_path_new_c("a/b/c.txt"); // Allocates and parses
 * const char* cstr = ucb_path_cstr(path);       // Borrowed, rebuilds cache
 * ucb_path_free(path);                          // Frees components and path
 * @endcode
 */
typedef struct ucb_path
{
    ucb_str dir;          ///< Directory/anchor, no trailing separator except a root
    ucb_str stem;         ///< Filename without extension
    ucb_str ext;          ///< Extension including leading dot; empty when none
    ucb_path_style style; ///< Output style; DEFAULT resolves from config
    ucb_str _name;        ///< Private cache: stem + ext
    ucb_str _cache;       ///< Private cache: full composed path
    bool _dirty;          ///< Private: caches need rebuilding
} ucb_path;

/**
 * @name Construction
 * @{
 */

/**
 * @brief Helper function for stack initialization of an empty path.
 * @return an empty path with the default style
 */
static inline ucb_path ucb_path_make(void)
{
    ucb_path path;
    path.dir = ucb_str_make();
    path.stem = ucb_str_make();
    path.ext = ucb_str_make();
    path.style = UCB_PATH_STYLE_DEFAULT;
    path._name = ucb_str_make();
    path._cache = ucb_str_make();
    path._dirty = true;
    return path;
}

/**
 * @brief Allocate and initialize a new path from a C string.
 *
 * The style is taken from the thread-local @ref ucb_config default.
 *
 * @param cstr a C string or literal, or UCB_NULL for an empty path
 * @param len 0 or length of string excluding null-terminator
 * @return pointer to new ucb_path or UCB_NULL on error.
 */
UCB_API ucb_path* ucb_path_new(const char* cstr, size_t len);
static inline ucb_path* ucb_path_new_c(const char* cstr)
{
    return ucb_path_new(cstr, 0);
}

/**
 * @brief Allocate and initialize a new path from a C string with a style.
 * @param cstr a C string or literal, or UCB_NULL for an empty path
 * @param style output style, or UCB_PATH_STYLE_DEFAULT for the config default
 * @return pointer to new ucb_path or UCB_NULL on error.
 */
UCB_API ucb_path* ucb_path_new_style(const char* cstr, ucb_path_style style);

/**
 * @brief Allocate and initialize a heap path as a deep copy of another path.
 * @param src path to clone
 * @return pointer to new ucb_path or UCB_NULL on error.
 */
UCB_API ucb_path* ucb_path_clone(const ucb_path* src);

/**
 * @brief Initialize an owned path from a C string.
 *
 * The style is taken from the thread-local @ref ucb_config default.
 *
 * @warning It doesn't perform any verifications on @p path, which may lead to
 * leaks if the path is already initialized.
 * @param path path to initialize
 * @param cstr a C string or literal, or UCB_NULL for an empty path
 * @param len 0 or length of string excluding null-terminator
 * @return true on success, false on allocation error
 */
UCB_API bool ucb_path_init(ucb_path* path, const char* cstr, size_t len);
static inline void ucb_path_init_c(ucb_path* path, const char* cstr)
{
    ucb_path_init(path, cstr, 0);
}

/**
 * @brief Initialize an owned path from a C string with a style.
 * @param path path to initialize
 * @param cstr a C string or literal, or UCB_NULL for an empty path
 * @param style output style, or UCB_PATH_STYLE_DEFAULT for the config default
 * @return true on success, false on allocation error
 */
UCB_API bool ucb_path_init_style(ucb_path* path, const char* cstr, ucb_path_style style);

/** @} */

/**
 * @name Destruction
 * @{
 */

/**
 * @brief Release a path.
 *
 * Frees all owned components and zeroes the struct. The struct can be
 * reinitialized afterwards with any @ref ucb_path_init* function.
 * @param path path to release
 */
UCB_API void ucb_path_release(ucb_path* path);

/**
 * @brief Free a heap allocated path.
 * @param path path to free
 */
UCB_API void ucb_path_free(ucb_path* path);

/** @} */

/**
 * @name Assignment and data update
 * @{
 */

/**
 * @brief Deep copy a path.
 *
 * @p dst releases its current data and becomes a copy of @p src.
 * @param dst destination path
 * @param src source path
 * @return true on success, false on allocation error
 */
UCB_API bool ucb_path_copy(ucb_path* dst, const ucb_path* src);

/**
 * @brief Assign a C string, re-parsing it.
 *
 * Preserves the current output style of @p path.
 * @param path path to update
 * @param cstr a C string or literal, or UCB_NULL for an empty path
 * @param len 0 or length of string excluding null-terminator
 * @return true on success, false on allocation error
 */
UCB_API bool ucb_path_assign(ucb_path* path, const char* cstr, size_t len);
static inline bool ucb_path_assign_c(ucb_path* path, const char* cstr)
{
    return ucb_path_assign(path, cstr, 0);
}

/**
 * @brief Clear all components, keeping the style.
 * @param path path to clear
 */
UCB_API void ucb_path_clear(ucb_path* path);

/** @} */

/**
 * @name Querying and composition
 * @{
 */

/**
 * @brief Get the composed path as a borrowed C string.
 *
 * The private cache is rebuilt if needed. The returned pointer is valid until
 * the path is next modified or released.
 *
 * @note The argument is non-const because the cache may be rebuilt.
 * @param path path to query
 * @return borrowed null-terminated C string
 */
UCB_API const char* ucb_path_cstr(ucb_path* path);

/**
 * @brief Compose the path into a new owned string.
 *
 * Unlike @ref ucb_path_cstr(), this never mutates @p path.
 * @param path path to compose
 * @return pointer to new ucb_str or UCB_NULL on error
 */
UCB_API ucb_str* ucb_path_to_str(const ucb_path* path);

/**
 * @brief Get the byte length of the composed path.
 *
 * Always equals <tt>strlen(ucb_path_cstr(path))</tt>.
 * @param path path to query
 * @return length in bytes
 */
UCB_API size_t ucb_path_len(const ucb_path* path);

/**
 * @brief Check if the path has no components.
 * @param path path to query
 * @return true if dir, stem and ext are all empty
 */
UCB_API bool ucb_path_is_empty(const ucb_path* path);

/**
 * @brief Check if the path is absolute.
 *
 * On POSIX a leading `/` makes a path absolute. On Windows a leading
 * separator, a drive root (`X:\`) or a UNC anchor (`\\server\share`) makes a
 * path absolute.
 * @param path path to query
 * @return true if absolute
 */
UCB_API bool ucb_path_is_absolute(const ucb_path* path);

/**
 * @brief Check if the path is relative.
 * @see ucb_path_is_absolute()
 * @param path path to query
 * @return true if relative
 */
UCB_API bool ucb_path_is_relative(const ucb_path* path);

/**
 * @brief Set the output style.
 *
 * Any internal separators in the directory component are rewritten to the new
 * style and the caches are invalidated.
 * @param path path to update
 * @param style the new style
 */
UCB_API void ucb_path_set_style(ucb_path* path, ucb_path_style style);

/**
 * @brief Get the output style.
 * @param path path to query
 * @return the stored style
 */
UCB_API ucb_path_style ucb_path_get_style(const ucb_path* path);

/**
 * @brief Get the path separator character for the default style.
 *
 * Resolves the thread-local @ref ucb_config default style; when that is
 * @ref UCB_PATH_STYLE_DEFAULT, the native separator is used (`\` on Windows,
 * `/` elsewhere).
 * @return the separator character
 */
UCB_API char ucb_path_sep(void);

/**
 * @brief Get the path separator for the default style as a C string.
 *
 * Resolves the thread-local @ref ucb_config default style; when that is
 * @ref UCB_PATH_STYLE_DEFAULT, the native separator is used ("\"" on Windows,
 * "/"" elsewhere).
 * @return the separator character as a C string
 */
UCB_API const char* ucb_path_sep_cstr(void);

/** @} */

/**
 * @name Components
 * @{
 */

/**
 * @brief Get the directory component as a borrowed string.
 * @param path path to query
 * @return borrowed directory string
 */
UCB_API const ucb_str* ucb_path_dir(const ucb_path* path);

/**
 * @brief Get the stem (filename without extension) as a borrowed string.
 * @param path path to query
 * @return borrowed stem string
 */
UCB_API const ucb_str* ucb_path_stem(const ucb_path* path);

/**
 * @brief Get the extension (including leading dot) as a borrowed string.
 * @param path path to query
 * @return borrowed extension string
 */
UCB_API const ucb_str* ucb_path_ext(const ucb_path* path);

/**
 * @brief Get the filename (stem + extension) as a borrowed string.
 *
 * @note The argument is non-const because the cache may be rebuilt.
 * @param path path to query
 * @return borrowed filename string
 */
UCB_API const ucb_str* ucb_path_filename(ucb_path* path);

/**
 * @brief Parse a C string and return its directory as a new owned string.
 * @param cstr a C string or literal
 * @return pointer to new ucb_str or UCB_NULL on error
 */
UCB_API ucb_str* ucb_path_dir_c(const char* cstr);

/**
 * @brief Parse a C string with a style and return its directory as a new owned string.
 * @param cstr a C string or literal
 * @param style output style, or UCB_PATH_STYLE_DEFAULT for the config default
 * @return pointer to new ucb_str or UCB_NULL on error
 */
UCB_API ucb_str* ucb_path_dir_cstyle(const char* cstr, ucb_path_style style);

/**
 * @brief Parse a C string and return its stem as a new owned string.
 * @param cstr a C string or literal
 * @return pointer to new ucb_str or UCB_NULL on error
 */
UCB_API ucb_str* ucb_path_stem_c(const char* cstr);

/**
 * @brief Parse a C string and return its extension as a new owned string.
 * @param cstr a C string or literal
 * @return pointer to new ucb_str or UCB_NULL on error
 */
UCB_API ucb_str* ucb_path_ext_c(const char* cstr);

/**
 * @brief Parse a C string and return its filename as a new owned string.
 * @param cstr a C string or literal
 * @return pointer to new ucb_str or UCB_NULL on error
 */
UCB_API ucb_str* ucb_path_filename_c(const char* cstr);

/** @} */

/**
 * @name Parent
 * @{
 */

/**
 * @brief Get the parent path.
 *
 * Composes a new path with the filename removed, or with the last directory
 * segment removed when there is no filename. A root is its own parent.
 * @param path path to query
 * @return pointer to new ucb_path or UCB_NULL on error
 */
UCB_API ucb_path* ucb_path_parent(const ucb_path* path);

/**
 * @brief Parse a C string and return its parent as a new owned string.
 * @param cstr a C string or literal
 * @return pointer to new ucb_str or UCB_NULL on error
 */
UCB_API ucb_str* ucb_path_parent_c(const char* cstr);

/**
 * @brief Parse a C string with a style and return its parent as a new owned string.
 * @param cstr a C string or literal
 * @param style output style, or UCB_PATH_STYLE_DEFAULT for the config default
 * @return pointer to new ucb_str or UCB_NULL on error
 */
UCB_API ucb_str* ucb_path_parent_cstyle(const char* cstr, ucb_path_style style);

/** @} */

/**
 * @name Setters
 * @{
 */

/**
 * @brief Set the directory component.
 *
 * Repeated separators are collapsed and a trailing separator is removed unless
 * the value is a root.
 * @param path path to update
 * @param dir new directory string
 * @return true on success, false on allocation error
 */
UCB_API bool ucb_path_set_dir(ucb_path* path, const ucb_str* dir);
static inline bool ucb_path_set_dir_c(ucb_path* path, const char* dir)
{
    ucb_str tmp = ucb_str_make();
    if (!ucb_str_assign_c(&tmp, dir))
        return false;
    bool ok = ucb_path_set_dir(path, &tmp);
    ucb_str_release(&tmp);
    return ok;
}

/**
 * @brief Set the stem component.
 * @param path path to update
 * @param stem new stem string
 * @return true on success, false on allocation error
 */
UCB_API bool ucb_path_set_stem(ucb_path* path, const ucb_str* stem);
static inline bool ucb_path_set_stem_c(ucb_path* path, const char* stem)
{
    ucb_str tmp = ucb_str_make();
    if (!ucb_str_assign_c(&tmp, stem))
        return false;
    bool ok = ucb_path_set_stem(path, &tmp);
    ucb_str_release(&tmp);
    return ok;
}

/**
 * @brief Set the extension component.
 *
 * The value is treated as one opaque extension and normalized to a single
 * leading dot. An empty value (or only dots) clears the extension.
 * @param path path to update
 * @param ext new extension string
 * @return true on success, false on allocation error
 */
UCB_API bool ucb_path_set_ext(ucb_path* path, const ucb_str* ext);
static inline bool ucb_path_set_ext_c(ucb_path* path, const char* ext)
{
    ucb_str tmp = ucb_str_make();
    if (!ucb_str_assign_c(&tmp, ext))
        return false;
    bool ok = ucb_path_set_ext(path, &tmp);
    ucb_str_release(&tmp);
    return ok;
}

/**
 * @brief Set the filename, re-parsing stem and extension.
 * @param path path to update
 * @param name new filename string
 * @return true on success, false on allocation error
 */
UCB_API bool ucb_path_set_filename(ucb_path* path, const ucb_str* name);
static inline bool ucb_path_set_filename_c(ucb_path* path, const char* name)
{
    ucb_str tmp = ucb_str_make();
    if (!ucb_str_assign_c(&tmp, name))
        return false;
    bool ok = ucb_path_set_filename(path, &tmp);
    ucb_str_release(&tmp);
    return ok;
}

/**
 * @brief Replace the extension.
 *
 * Behaves as @ref ucb_path_set_ext(); the previous extension is discarded.
 * @param path path to update
 * @param ext new extension string
 * @return true on success, false on allocation error
 */
UCB_API bool ucb_path_replace_ext(ucb_path* path, const ucb_str* ext);
static inline bool ucb_path_replace_ext_c(ucb_path* path, const char* ext)
{
    ucb_str tmp = ucb_str_make();
    if (!ucb_str_assign_c(&tmp, ext))
        return false;
    bool ok = ucb_path_replace_ext(path, &tmp);
    ucb_str_release(&tmp);
    return ok;
}

/** @} */

/**
 * @name Operations
 * @{
 */

/**
 * @brief Join two paths.
 *
 * If @p other is absolute it replaces @p base entirely. Otherwise its
 * directory and filename are appended to @p base.
 * @param base base path
 * @param other path to join
 * @return pointer to new ucb_path or UCB_NULL on error
 */
UCB_API ucb_path* ucb_path_join(const ucb_path* base, const ucb_path* other);

/**
 * @brief Join a base path with a C string.
 * @param base base path
 * @param other a C string or literal
 * @return pointer to new ucb_path or UCB_NULL on error
 */
UCB_API ucb_path* ucb_path_join_c(const ucb_path* base, const char* other);

/**
 * @brief Append a path in place.
 *
 * If @p other is absolute it replaces @p path entirely; otherwise its
 * directory and filename are appended.
 * @param path path to modify
 * @param other path to append
 * @return true on success, false on allocation error
 */
UCB_API bool ucb_path_append(ucb_path* path, const ucb_path* other);

/**
 * @brief Append a C string in place.
 * @param path path to modify
 * @param other a C string or literal
 * @return true on success, false on allocation error
 */
UCB_API bool ucb_path_append_c(ucb_path* path, const char* other);

/**
 * @brief Normalize the path lexically, in place.
 *
 * Collapses repeated separators, removes `.` segments and resolves `..`
 * segments without crossing the anchor. This never touches the filesystem and
 * does not resolve symlinks.
 * @param path path to normalize
 * @return true on success, false on allocation error
 */
UCB_API bool ucb_path_normalize(ucb_path* path);

/**
 * @brief Parse and normalize a C string, returning a new owned string.
 * @param cstr a C string or literal
 * @return pointer to new ucb_str or UCB_NULL on error
 */
UCB_API ucb_str* ucb_path_normalize_c(const char* cstr);

/**
 * @brief Parse and normalize a C string with a style, returning a new owned string.
 * @param cstr a C string or literal
 * @param style output style, or UCB_PATH_STYLE_DEFAULT for the config default
 * @return pointer to new ucb_str or UCB_NULL on error
 */
UCB_API ucb_str* ucb_path_normalize_cstyle(const char* cstr, ucb_path_style style);

/**
 * @brief Compare two paths for equality by their composed form.
 * @param a first path
 * @param b second path
 * @return true if equal
 */
UCB_API bool ucb_path_equals(const ucb_path* a, const ucb_path* b);

/**
 * @brief Compare two C strings as paths for equality.
 * @param a first C string or literal
 * @param b second C string or literal
 * @return true if equal
 */
UCB_API bool ucb_path_equals_c(const char* a, const char* b);

/**
 * @brief Check whether one path is inside the other, lexically.
 *
 * Returns true when either @p a is a proper descendant of @p b or @p b is a
 * proper descendant of @p a, compared part by part. The comparison is
 * lexical/type-level: it never touches the filesystem, does not resolve
 * symlinks and does not normalize the inputs, so callers should normalize
 * beforehand when needed. Equal paths are not considered "inside" (use
 * @ref ucb_path_equals() for that).
 * @param a first path
 * @param b second path
 * @return true if one path is inside the other
 */
UCB_API bool ucb_path_is_inside(const ucb_path* a, const ucb_path* b);

/**
 * @brief Compare two paths by their composed form.
 * @param a first path
 * @param b second path
 * @return 0 if equal, negative if @p a sorts before @p b, positive otherwise
 */
UCB_API int ucb_path_comp(const ucb_path* a, const ucb_path* b);

/**
 * @brief Compare two C strings as paths.
 * @param a first C string or literal
 * @param b second C string or literal
 * @return 0 if equal, negative if @p a sorts before @p b, positive otherwise
 */
UCB_API int ucb_path_comp_c(const char* a, const char* b);

/**
 * @brief Compare two paths case-insensitively by their composed form.
 * @param a first path
 * @param b second path
 * @return 0 if equal, negative if @p a sorts before @p b, positive otherwise
 */
UCB_API int ucb_path_icomp(const ucb_path* a, const ucb_path* b);

/**
 * @brief Compare two C strings as paths case-insensitively.
 * @param a first C string or literal
 * @param b second C string or literal
 * @return 0 if equal, negative if @p a sorts before @p b, positive otherwise
 */
UCB_API int ucb_path_icomp_c(const char* a, const char* b);

/** @} */

/**
 * @name Parts and parents
 * @{
 */

/**
 * @brief Get the number of path parts.
 *
 * Parts are the anchor (if any), each directory segment and the filename
 * (if non-empty).
 * @param path path to query
 * @return number of parts
 */
UCB_API size_t ucb_path_num_parts(const ucb_path* path);

/**
 * @brief Get a single path part as a new owned string.
 * @param path path to query
 * @param index part index, must be less than @ref ucb_path_num_parts()
 * @return pointer to new ucb_str or UCB_NULL on error
 */
UCB_API ucb_str* ucb_path_part(const ucb_path* path, size_t index);

/**
 * @brief Get all path parts as a vector of new owned strings.
 *
 * The returned vector and its elements are owned by the caller. Free it with
 * @ref ucb_vector_str_free_full().
 * @param path path to query
 * @return pointer to new vector or UCB_NULL on error
 */
UCB_API ucb_vector_str* ucb_path_parts(const ucb_path* path);

/**
 * @brief Parse a C string and get all its parts.
 * @see ucb_path_parts()
 * @param cstr a C string or literal
 * @return pointer to new vector or UCB_NULL on error
 */
UCB_API ucb_vector_str* ucb_path_parts_c(const char* cstr);

/**
 * @brief Parse a C string with a style and get all its parts.
 * @see ucb_path_parts()
 * @param cstr a C string or literal
 * @param style output style, or UCB_PATH_STYLE_DEFAULT for the config default
 * @return pointer to new vector or UCB_NULL on error
 */
UCB_API ucb_vector_str* ucb_path_parts_cstyle(const char* cstr, ucb_path_style style);

/**
 * @brief Get all ancestors as a vector of new owned strings.
 *
 * The list starts with the immediate parent and ends at the anchor. The
 * returned vector and its elements are owned by the caller. Free it with
 * @ref ucb_vector_str_free_full().
 * @param path path to query
 * @return pointer to new vector or UCB_NULL on error
 */
UCB_API ucb_vector_str* ucb_path_parents(const ucb_path* path);

/**
 * @brief Parse a C string and get all its ancestors.
 * @see ucb_path_parents()
 * @param cstr a C string or literal
 * @return pointer to new vector or UCB_NULL on error
 */
UCB_API ucb_vector_str* ucb_path_parents_c(const char* cstr);

/**
 * @brief Parse a C string with a style and get all its ancestors.
 * @see ucb_path_parents()
 * @param cstr a C string or literal
 * @param style output style, or UCB_PATH_STYLE_DEFAULT for the config default
 * @return pointer to new vector or UCB_NULL on error
 */
UCB_API ucb_vector_str* ucb_path_parents_cstyle(const char* cstr, ucb_path_style style);

/** @} */

#endif // UCB_PATH_H
