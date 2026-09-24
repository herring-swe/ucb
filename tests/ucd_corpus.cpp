/**
 * @file ucd_corpus.cpp
 *
 * This file is part of the UCB project
 * - SPDX-FileCopyrightText: © 2026 Åke Svedin <ake@svedin.org>
 * - SPDX-License-Identifier: MIT
 *
 * @brief Shared loader for the official UCD NormalizationTest corpus.
 */

#include "ucd_corpus.h"

#include <ucb/unicode.h>

#include <cctype>
#include <cstdint>
#include <fstream>

namespace {

std::string trim(const std::string& str)
{
    const std::size_t start = str.find_first_not_of(" \t\n\r");
    if (start == std::string::npos)
        return "";
    const std::size_t end = str.find_last_not_of(" \t\n\r");
    return str.substr(start, end - start + 1);
}

std::vector<std::string> split_line(const std::string& line, char delimiter, std::size_t max)
{
    std::vector<std::string> parts;
    std::size_t start = 0;
    for (std::size_t count = 0; count < max; ++count)
    {
        const std::size_t end = line.find(delimiter, start);
        if (end == std::string::npos)
            break;
        parts.push_back(line.substr(start, end - start));
        start = end + 1;
    }
    parts.push_back(line.substr(start));
    return parts;
}

} // namespace

std::string ucd_hex_to_utf8(const char* hex)
{
    std::string out;
    const unsigned char* ptr = reinterpret_cast<const unsigned char*>(hex);
    while (*ptr)
    {
        while (std::isspace(*ptr))
            ++ptr;
        if (!*ptr)
            break;

        std::uint32_t cp = 0;
        int digits = 0;
        while (*ptr && std::isxdigit(*ptr) && digits < 6)
        {
            cp = (cp << 4) |
                 static_cast<std::uint32_t>(std::isdigit(*ptr) ? (*ptr - '0')
                                                               : (std::tolower(*ptr) - 'a' + 10));
            ++ptr;
            ++digits;
        }

        char buf[4];
        const int written =
            ucb_uc_encode_codepoint(reinterpret_cast<uint8_t*>(buf), static_cast<ucb_cp>(cp));
        if (written > 0)
            out.append(buf, static_cast<std::size_t>(written));
    }
    return out;
}

bool ucd_load_normalization_tests(const char* path, std::vector<ucd_norm_case>& tests)
{
    tests.clear();

    std::ifstream stream(path, std::ios::in);
    if (!stream.is_open())
        return false;

    std::string line;
    std::string part_name;
    int part_index = -1;
    int line_no = 0;
    int test_no = 0;

    while (std::getline(stream, line))
    {
        line_no++;
        if (line.empty() || line.at(0) == '#')
            continue;

        if (line.at(0) == '@')
        {
            part_index++;
            const std::vector<std::string> header = split_line(line, '#', 1);
            part_name = "Part " + std::to_string(part_index) + " - " + trim(header.back());
            continue;
        }

        const std::vector<std::string> parts = split_line(line, ';', 5);
        if (parts.size() < 5)
            continue;

        test_no++;

        ucd_norm_case data;
        data.line_no = line_no;
        data.test_no = test_no;
        data.part_name = part_name;
        data.input = ucd_hex_to_utf8(parts[0].c_str());
        data.nfc = ucd_hex_to_utf8(parts[1].c_str());
        data.nfd = ucd_hex_to_utf8(parts[2].c_str());
        data.nfkc = ucd_hex_to_utf8(parts[3].c_str());
        data.nfkd = ucd_hex_to_utf8(parts[4].c_str());
        data.comment = parts.size() > 5 ? trim(parts[5]) : "";

        tests.push_back(std::move(data));
    }
    return true;
}
