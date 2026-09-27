/**
 * @file fs_tmp.h
 *
 * This file is part of the UCB project
 * - SPDX-FileCopyrightText: © 2026 Åke Svedin <ake@svedin.org>
 * - SPDX-License-Identifier: MIT
 *
 * @brief Shared temporary-directory helper for filesystem and directory tests
 */

#ifndef TESTS_FS_TMP_H
#define TESTS_FS_TMP_H

#include <string>

/**
 * @brief RAII temporary directory rooted under the system temp directory.
 *
 * The directory is created on construction and removed recursively on
 * destruction. All paths are composed with `/`, accepted on every platform.
 */
class FsTmp
{
public:
    explicit FsTmp(const char* prefix = "ucb_fs_tmp");
    ~FsTmp();

    FsTmp(const FsTmp&) = delete;
    FsTmp& operator=(const FsTmp&) = delete;

    /// @brief Absolute-ish root directory of this temporary tree.
    const std::string& dir() const
    {
        return m_dir;
    }

    /// @brief Compose a path inside the temporary tree. An empty leaf yields dir().
    std::string path(const char* leaf) const;

    /// @brief Create or truncate a file with @p data, returning success.
    bool write(const char* leaf, const std::string& data) const;

    /// @brief Read a file fully into @p out, returning success.
    bool read(const char* leaf, std::string& out) const;

private:
    std::string m_dir;
};

/**
 * @brief Create a symbolic link, returning false when unsupported.
 *
 * On Windows this needs either developer mode or a privileged process; tests
 * must skip their symlink coverage when this returns false.
 */
bool fs_create_symlink(const char* target, const char* linkpath, bool target_is_dir);

#endif // TESTS_FS_TMP_H
