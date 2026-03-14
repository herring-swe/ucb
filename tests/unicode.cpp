/**
 * @file unicode.cpp
 *
 * This file is part of the UCB project
 * - SPDX-FileCopyrightText: © 2026 Åke Svedin <ake@svedin.org>
 * - SPDX-License-Identifier: MIT
 *
 * @brief unicode tests
 */

#include "doctest.h"

#include <ucb/errcodes.h>
#include <ucb/memdbg.h>
#include <ucb/memory.h>
#include <ucb/unicode.h>

#include <chrono>
#include <climits>
#include <cstring>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <sstream>
#include <string>
#include <vector>

#ifdef USE_OPENMP
#include <atomic>
#include <omp.h>
#endif

#ifdef __INTELLISENSE__
#define NORM_TEST_FILE "dummy"
#endif

typedef ucb_uc_result (*mapping_func)(const char* str, size_t size, ucb_error** perr);

static int split_line(const std::string& line, char c, std::vector<std::string>& result,
                      int max = -1)
{
    result.clear();
    size_t start = 0;
    if (max == -1)
        max = INT_MAX;
    int count = 0; // Number of splits
    while (count < max)
    {
        size_t end = line.find(c, start);
        if (end == std::string::npos)
            break;
        result.push_back(line.substr(start, end - start));
        start = end + 1;
        count++;
    }
    result.push_back(line.substr(start));
    return count;
}

static std::string trim(const std::string& str)
{
    size_t start = str.find_first_not_of(" \t\n\r");
    if (start == std::string::npos)
        return "";
    size_t end = str.find_last_not_of(" \t\n\r");
    return str.substr(start, end - start + 1);
}

/**
 * Converts string of form "41 325 61" to utf-8 encoded string
 */
static inline void hexstr_to_utf8(const char* input, char** output)
{
    size_t in_len = strlen(input);

    ucb_buffer buf;
    ucb_buffer_init_heap(&buf, in_len);

    // Decode whitespace separated hex string (without 0x prefix) into ucb_buffer
    const unsigned char* ptr = reinterpret_cast<const unsigned char*>(input);
    while (*ptr)
    {
        // Skip whitespace
        while (isspace(*ptr))
        {
            ptr++;
        }
        if (!*ptr)
            break;

        // Parse a single codepoint (1–6 hex digits)
        uint32_t cp = 0;
        int digits  = 0;
        while (*ptr && isxdigit(*ptr) && digits < 6)
        {
            cp = (cp << 4) |
                 static_cast<uint32_t>((isdigit(*ptr) ? (*ptr - '0') : (tolower(*ptr) - 'a' + 10)));
            ptr++;
            digits++;
        }

        // Store the codepoint
        ucb_buffer_push(&buf, &cp, sizeof(uint32_t));
    }

    size_t count = buf.size / sizeof(uint32_t);
    buf.size     = 0; // reset buffer
    ucb_uc_encode_codepoints(&buf, reinterpret_cast<uint32_t*>(buf.data), count, nullptr);
    ucb_buffer_push(&buf, "\0", 1);
    ucb_buffer_transfer(&buf, reinterpret_cast<void**>(output), nullptr, nullptr, nullptr);
    ucb_buffer_release(&buf);
}

static std::string hexstr_to_utf8(std::string& cp_str)
{
    std::string result;
    char* output = nullptr;
    hexstr_to_utf8(cp_str.c_str(), &output);
    if (output)
    {
        result = output;
        ucb_free(output);
    }
    return result;
}

static inline bool check_with_null(const char* input, size_t len)
{
    for (size_t i = 0; i < len; i++)
    {
        if (input[i] == '\0')
            return true;
    }
    return false;
}

static void print_null(const char* input, size_t len)
{
    for (size_t i = 0; i < len; i++)
    {
        if (input[i] == '\0')
            printf("\\0");
        else
            printf("%c", input[i]);
    }
    printf("\n");
}

static inline void test_basics(const char* input, size_t len, size_t num_cp, size_t num_chars)
{
    bool with_null = check_with_null(input, len);

    if (with_null)
        print_null(input, len);
    else
        printf("%s\n", input);
    if (!with_null)
        CHECK(len == strlen(input));
    REQUIRE(ucb_uc_validate(input, len, UCB_NULL));
    CHECK(ucb_uc_num_cp(input, len) == num_cp);
    if (!with_null)
        CHECK(ucb_uc_num_cp(input, UCB_NPOS) == num_cp);
    CHECK(ucb_uc_num_chars(input, len) == num_chars);
}

static inline void test_grapheme(const char* input, size_t len, size_t num_cp, size_t num_chars,
                                 size_t num_fail_chars)
{
    bool with_null = check_with_null(input, len);

    if (with_null)
        print_null(input, len);
    else
        printf("%s\n", input);
    if (!with_null)
        CHECK(len == strlen(input));
    REQUIRE(ucb_uc_validate(input, len, UCB_NULL));
    CHECK(ucb_uc_num_cp(input, len) == num_cp);

    // This should not fail with grapheme clusters
    CHECK(ucb_uc_num_chars(input, len) != num_chars);
    // This is the wrong report
    CHECK(ucb_uc_num_chars(input, len) == num_fail_chars);
}

static inline void test_mapping(const char* input, const char* lower, const char* upper,
                                const char* title, const char* casefold)
{
    size_t len;
    ucb_uc_result ucres;
    ucb_error* err = nullptr;

    const char* strings[5] = {input, lower, upper, title, casefold};
    mapping_func func[5]   = {nullptr, ucb_uc_to_lower, ucb_uc_to_upper, ucb_uc_to_title,
                              ucb_uc_casefold};
    // const char* names[5]   = {"", "to_lower", "to_upper", "to_title", "casefold"};

    for (int i = 0; i < 5; i++)
    {
        len = strlen(strings[i]);
        REQUIRE(ucb_uc_validate(strings[i], len, UCB_NULL));
        REQUIRE(len == strlen(strings[i]));

        if (func[i] == nullptr)
            continue;

        ucres = func[i](strings[0], len, &err);
        CHECK(!UCB_IS_THROWN(err));
        ucb_error_clear(&err);
        CHECK(ucres.data != nullptr);
        if (ucres.data)
        {
            // printf("Checking ucb_uc_%s(\"%s\") == \"%s\"\n", names[i], strings[i], ucres.data);
            CHECK(ucres.size == strlen(strings[i]));
            CHECK(std::string(strings[i]) == std::string(ucres.data));
            ucb_free(ucres.data);
        }
    }
}

static inline unsigned int test_norm_bench(const std::string& input, const std::string& correct,
                                           ucb_norm_form type)
{
    ucb_error* err = nullptr;
    ucb_uc_result ucres  = ucb_uc_normalize(input.c_str(), input.size(), type, &err);

    unsigned int success = 0;
    if (!UCB_IS_THROWN(err))
    {
        if (std::string(ucres.data) == correct)
            success = 1;
        ucb_free(ucres.data);
    }
    ucb_error_clear(&err);
    return success;
}

static inline void test_norm(const char* input, const char* correct, ucb_norm_form type)
{
    ucb_error* err = nullptr;
    ucb_uc_result ucres  = ucb_uc_normalize(input, strlen(input), type, &err);
    std::string type_str(ucb_uc_norm_form_to_str(type));

    CAPTURE(type_str);

    CHECK(!UCB_IS_THROWN(err));
    ucb_error_clear(&err);

    std::stringstream ss_inp("");
    const unsigned char* inp_iter = reinterpret_cast<const unsigned char*>(input);
    uint32_t cp;
    while ((cp = ucb_uc_iter_utf8(&inp_iter)))
    {
        int width = 4;
        if (cp > 0xFFFF)
            width = 6;
        ss_inp << std::setw(width) << std::setfill('0') << std::hex << cp << " ";
    }

    if (std::string(ucres.data) != std::string(correct))
    {
        const unsigned char* iter_res = reinterpret_cast<const unsigned char*>(ucres.data);
        const unsigned char* iter_cor = reinterpret_cast<const unsigned char*>(correct);
        uint32_t cp_res               = 1;
        uint32_t cp_cor               = 1;
        std::stringstream ss_res("");
        std::stringstream ss_cor("");

        do
        {
            int width = 4;
            if (cp_res > 0xFFFF || cp_cor > 0xFFFF)
                width = 6;
            if (cp_res)
                cp_res = ucb_uc_iter_utf8(&iter_res);
            if (cp_cor)
                cp_cor = ucb_uc_iter_utf8(&iter_cor);

            if (cp_res)
            {
                if (cp_cor && cp_res != cp_cor)
                    ss_res << "!!>";
                ss_res << std::setw(width) << std::setfill('0') << std::hex << cp_res << " ";
            }
            if (cp_cor)
            {
                if (cp_res && cp_res != cp_cor)
                    ss_cor << "!!>";
                ss_cor << std::setw(width) << std::setfill('0') << std::hex << cp_cor << " ";
            }
        } while (cp_res && cp_cor);

        std::string cps_res = ss_res.str();
        std::string cps_cor = ss_cor.str();
        CAPTURE(ss_inp.str());
        CAPTURE(cps_res);
        CAPTURE(cps_cor);
        REQUIRE(std::string(ucres.data) == std::string(correct));
    }
    ucb_free(ucres.data);
}

static inline void test_norm_hex(const char* input, const char* hex_correct, ucb_norm_form type)
{
    char* correct = nullptr;
    hexstr_to_utf8(hex_correct, &correct);
    test_norm(input, correct, type);
    ucb_free(correct);
}

TEST_SUITE_BEGIN("unicode");

TEST_CASE("unicode basics")
{
    UCB_MEMTRACK_PUSH();

    test_basics("", 0, 0, 0);
    test_basics("A", 1, 1, 1);
    test_basics("é", 2, 1, 1);       // 'é' as a single codepoint (e.g., U+00E9)
    test_basics("e\u0301", 3, 2, 1); // 'e' + combining acute accent (U+0301)
    test_basics("Hello", 5, 5, 5);
    test_basics("Héllø", 7, 5, 5);            // 'é' and 'ø' as single codepoints
    test_basics("Ž̌", 5, 3, 1);                // 'Z' + two combining carons (U+030C)
    test_basics("🌍", 4, 1, 1);               // Emoji (single codepoint)
    test_basics("a̐̀", 5, 3, 1);                // 'a' + two combining marks
    test_basics("日本語", 9, 3, 3);           // CJK ideographs (3 codepoints, 3 chars)
    test_basics("A\u0300\u0316", 5, 3, 1);    // 'A' + combining grave + combining inverted breve
    test_basics("\xF0\x9F\x98\x80", 4, 1, 1); // Grinning face emoji (U+1F600)
    test_basics("A\u0301B\u0302", 6, 4, 2);   // 'A' + acute, 'B' + circumflex
    test_basics("a\0b\0c", 5, 5, 5);          // String with embedded nulls

    UCB_MEMTRACK_POP();
}

TEST_CASE("unicode grapheme")
{
    UCB_MEMTRACK_PUSH();

    test_grapheme("👨‍👩‍👧‍👦", 25, 7, 1, 7); // Family emoji (ZWJ sequences)
    test_grapheme("🏳️‍🌈", 14, 4, 1, 3);            // Gaydar flag (ZWJ + emoji)
    // Will not fail due to combining marks
    test_basics("©\uFE0F", 5, 2, 1);           // '©' + emoji variation selector
    test_basics("#\u20E3", 4, 2, 1);           // '#' + Combining Enclosing Keycap
    test_grapheme("👩‍💻", 11, 3, 1, 3); // Woman technologist (ZWJ sequence)
    test_grapheme("🇺🇸", 8, 2, 1, 2);           // US flag (regional indicator symbols)
    test_grapheme("👨🏽", 8, 2, 1, 2);         // Man + medium skin tone modifier
    test_grapheme("\xF0\x9F\x91\xA8\xE2\x80\x8D\xF0\x9F\x91\xA9\xE2\x80\x8D\xF0\x9F\x91\xA6", 18, 5,
                  1, 5);

    UCB_MEMTRACK_POP();
}

TEST_CASE("unicode mapping")
{
    UCB_MEMTRACK_PUSH();

    test_mapping("Hello World!", "hello world!", "HELLO WORLD!", "Hello World!", "hello world!");
    test_mapping("Héllö Wörld!", "héllö wörld!", "HÉLLÖ WÖRLD!", "Héllö Wörld!", "héllö wörld!");
    test_mapping("straße", "straße", "STRAßE", "Straße", "strasse");

    test_mapping("O'Reilly", "o'reilly", "O'REILLY", "O'reilly", "o'reilly");
    test_mapping("Over-achiever", "over-achiever", "OVER-ACHIEVER", "Over-achiever",
                 "over-achiever");
    test_mapping("Simply \"The Best\"", "simply \"the best\"", "SIMPLY \"THE BEST\"",
                 "Simply \"The Best\"", "simply \"the best\"");

    UCB_MEMTRACK_POP();
}

TEST_CASE("unicode normalization")
{
    UCB_MEMTRACK_PUSH();

    test_norm("Héllö Wörld!", "Héllö Wörld!", UCB_NORM_NFD);
    test_norm("Héllö Wörld!", "Héllö Wörld!", UCB_NORM_NFC);

    test_norm("ḀḁḂḃḄḅḆḇḈḉḊḋḌḍḎḏḐḑḒḓḔḕḖḗḘḙḚḛḜḝḞḟḠḡḢḣḤḥḦḧḨḩḪḫḬḭḮḯḰḱḲḳḴḵḶḷḸḹḺḻḼḽḾḿṀṁṂṃṄṅṆṇṈṉṊṋṌṍṎṏṐṑṒṓ"
              "ṔṕṖṗṘṙṚṛṜṝṞṟṠṡṢṣṤṥṦṧṨṩṪṫṬṭṮṯṰṱṲṳṴṵṶṷṸṹṺṻṼṽṾṿ",
              "ḀḁḂḃḄḅḆḇḈḉḊḋḌḍḎḏḐḑḒḓḔḕḖḗḘḙḚḛḜḝḞḟḠḡḢḣḤḥḦḧḨḩḪḫḬḭḮḯḰḱḲḳḴḵḶḷḸḹḺḻḼḽḾḿṀṁṂṃṄṅṆṇṈṉṊṋṌṍṎṏṐṑṒṓ"
              "ṔṕṖṗṘṙṚṛṜṝṞṟṠṡṢṣṤṥṦṧṨṩṪṫṬṭṮṯṰṱṲṳṴṵṶṷṸṹṺṻṼṽṾṿ",
              UCB_NORM_NFD);
    test_norm_hex(
        "ḀḁḂḃḄḅḆḇḈḉḊḋḌḍḎḏḐḑḒḓḔḕḖḗḘḙḚḛḜḝḞḟḠḡḢḣḤḥḦḧḨḩḪḫḬḭḮḯḰḱḲḳḴḵḶḷḸḹḺḻḼḽḾḿṀṁṂṃṄṅṆṇṈṉṊṋṌṍṎṏ",
        "41 325 61 325 42 307 62 307 42 323 62 323 42 331 62 331 43 327 301 63 327 301 44 307 64 "
        "307 44 323 64 323 44 331 64 331 44 327 64 327 44 32d 64 32d 45 304 300 65 304 300 45 "
        "304 301 65 304 301 45 32d 65 32d 45 330 65 330 45 327 306 65 327 306 46 307 66 307 47 "
        "304 67 304 48 307 68 307 48 323 68 323 48 308 68 308 48 327 68 327 48 32e 68 32e 49 "
        "330 69 330 49 308 301 69 308 301 4b 301 6b 301 4b 323 6b 323 4b 331 6b 331 4c 323 6c "
        "323 4c 323 304 6c 323 304 4c 331 6c 331 4c 32d 6c 32d 4d 301 6d 301 4d 307 6d 307 4d "
        "323 6d 323 4e 307 6e 307 4e 323 6e 323 4e 331 6e 331 4e 32d 6e 32d 4f 303 301 6f 303 "
        "301 4f 303 308 6f 303 308",
        UCB_NORM_NFKD);
    test_norm_hex(
        "ḀḁḂḃḄḅḆḇḈḉḊḋḌḍḎḏḐḑḒḓḔḕḖḗḘḙḚḛḜḝḞḟḠḡḢḣḤḥḦḧḨḩḪḫḬḭḮḯḰḱḲḳḴḵḶḷḸḹḺḻḼḽḾḿṀṁṂṃṄṅṆṇṈṉṊṋṌṍṎṏ",
        "1e00 1e01 1e02 1e03 1e04 1e05 1e06 1e07 1e08 1e09 1e0a 1e0b 1e0c 1e0d 1e0e 1e0f 1e10 1e11 "
        "1e12 1e13 1e14 1e15 1e16 1e17 1e18 1e19 1e1a 1e1b 1e1c 1e1d 1e1e 1e1f 1e20 1e21 1e22 1e23 "
        "1e24 1e25 1e26 1e27 1e28 1e29 1e2a 1e2b 1e2c 1e2d 1e2e 1e2f 1e30 1e31 1e32 1e33 1e34 1e35 "
        "1e36 1e37 1e38 1e39 1e3a 1e3b 1e3c 1e3d 1e3e 1e3f 1e40 1e41 1e42 1e43 1e44 1e45 1e46 1e47 "
        "1e48 1e49 1e4a 1e4b 1e4c 1e4d 1e4e 1e4f",
        UCB_NORM_NFC);

    test_norm("ḀḁḂḃḄḅḆḇḈḉḊḋḌḍḎḏḐḑḒḓḔḕḖḗḘḙḚḛḜḝḞḟḠḡḢḣḤḥḦḧḨḩḪḫḬḭḮḯḰḱḲḳḴḵḶḷḸḹḺḻḼḽḾḿṀṁṂṃṄṅṆṇṈṉṊṋṌṍṎṏṐṑṒṓ"
              "ṔṕṖṗṘṙṚṛṜṝṞṟṠṡṢṣṤṥṦṧṨṩṪṫṬṭṮṯṰṱṲṳṴṵṶṷṸṹṺṻṼṽṾṿ",
              "ḀḁḂḃḄḅḆḇḈḉḊḋḌḍḎḏḐḑḒḓḔḕḖḗḘḙḚḛḜḝḞḟḠḡḢḣḤḥḦḧḨḩḪḫḬḭḮḯḰḱḲḳḴḵḶḷḸḹḺḻḼḽḾḿṀṁṂṃṄṅṆṇṈṉṊṋṌṍṎṏṐṑṒṓ"
              "ṔṕṖṗṘṙṚṛṜṝṞṟṠṡṢṣṤṥṦṧṨṩṪṫṬṭṮṯṰṱṲṳṴṵṶṷṸṹṺṻṼṽṾṿ",
              UCB_NORM_NFD);
    test_norm("ḀḁḂḃḄḅḆḇḈḉḊḋḌḍḎḏḐḑḒḓḔḕḖḗḘḙḚḛḜḝḞḟḠḡḢḣḤḥḦḧḨḩḪḫḬḭḮḯḰḱḲḳḴḵḶḷḸḹḺḻḼḽḾḿṀṁṂṃṄṅṆṇṈṉṊṋṌṍṎṏṐṑṒṓ"
              "ṔṕṖṗṘṙṚṛṜṝṞṟṠṡṢṣṤṥṦṧṨṩṪṫṬṭṮṯṰṱṲṳṴṵṶṷṸṹṺṻṼṽṾṿ",
              "ḀḁḂḃḄḅḆḇḈḉḊḋḌḍḎḏḐḑḒḓḔḕḖḗḘḙḚḛḜḝḞḟḠḡḢḣḤḥḦḧḨḩḪḫḬḭḮḯḰḱḲḳḴḵḶḷḸḹḺḻḼḽḾḿṀṁṂṃṄṅṆṇṈṉṊṋṌṍṎṏṐṑṒṓ"
              "ṔṕṖṗṘṙṚṛṜṝṞṟṠṡṢṣṤṥṦṧṨṩṪṫṬṭṮṯṰṱṲṳṴṵṶṷṸṹṺṻṼṽṾṿ",
              UCB_NORM_NFKD);
    test_norm("ḀḁḂḃḄḅḆḇḈḉḊḋḌḍḎḏḐḑḒḓḔḕḖḗḘḙḚḛḜḝḞḟḠḡḢḣḤḥḦḧḨḩḪḫḬḭḮḯḰḱḲḳḴḵḶḷḸḹḺḻḼḽḾḿṀṁṂṃṄṅṆṇṈṉṊṋṌṍṎṏṐṑṒṓ"
              "ṔṕṖṗṘṙṚṛṜṝṞṟṠṡṢṣṤṥṦṧṨṩṪṫṬṭṮṯṰṱṲṳṴṵṶṷṸṹṺṻṼṽṾṿ",
              "ḀḁḂḃḄḅḆḇḈḉḊḋḌḍḎḏḐḑḒḓḔḕḖḗḘḙḚḛḜḝḞḟḠḡḢḣḤḥḦḧḨḩḪḫḬḭḮḯḰḱḲḳḴḵḶḷḸḹḺḻḼḽḾḿṀṁṂṃṄṅṆṇṈṉṊṋṌṍṎṏṐṑṒṓ"
              "ṔṕṖṗṘṙṚṛṜṝṞṟṠṡṢṣṤṥṦṧṨṩṪṫṬṭṮṯṰṱṲṳṴṵṶṷṸṹṺṻṼṽṾṿ",
              UCB_NORM_NFC);
    test_norm("ḀḁḂḃḄḅḆḇḈḉḊḋḌḍḎḏḐḑḒḓḔḕḖḗḘḙḚḛḜḝḞḟḠḡḢḣḤḥḦḧḨḩḪḫḬḭḮḯḰḱḲḳḴḵḶḷḸḹḺḻḼḽḾḿṀṁṂṃṄṅṆṇṈṉṊṋṌṍṎṏṐṑṒṓ"
              "ṔṕṖṗṘṙṚṛṜṝṞṟṠṡṢṣṤṥṦṧṨṩṪṫṬṭṮṯṰṱṲṳṴṵṶṷṸṹṺṻṼṽṾṿ",
              "ḀḁḂḃḄḅḆḇḈḉḊḋḌḍḎḏḐḑḒḓḔḕḖḗḘḙḚḛḜḝḞḟḠḡḢḣḤḥḦḧḨḩḪḫḬḭḮḯḰḱḲḳḴḵḶḷḸḹḺḻḼḽḾḿṀṁṂṃṄṅṆṇṈṉṊṋṌṍṎṏṐṑṒṓ"
              "ṔṕṖṗṘṙṚṛṜṝṞṟṠṡṢṣṤṥṦṧṨṩṪṫṬṭṮṯṰṱṲṳṴṵṶṷṸṹṺṻṼṽṾṿ",
              UCB_NORM_NFKC);

    // Hangul + Latin tests
    test_norm("가각 Héllö", "가각 Héllö", UCB_NORM_NFD);
    test_norm("가각 Héllö", "가각 Héllö", UCB_NORM_NFC);

    // Hangul + Latin + Combining Marks + Ligatures
    test_norm("갛é각Åﬁ가́̀Ź̌한글LigaturesﬃﬄﬅℵἄΩﬂῴ", "갛é각Åﬁ가́̀Ź̌한글LigaturesﬃﬄﬅℵἄΩﬂῴ",
              UCB_NORM_NFD);
    test_norm("갛é각Åﬁ가́̀Ź̌한글LigaturesﬃﬄﬅℵἄΩﬂῴ", "갛é각Åﬁ가́̀Ź̌한글LigaturesﬃﬄﬅℵἄΩﬂῴ",
              UCB_NORM_NFC);
    test_norm("갛é각Åﬁ가́̀Ź̌한글LigaturesﬃﬄﬅℵἄΩﬂῴ", "갛é각Åfi가́̀Ź̌한글LigaturesffifflstאἄΩflῴ",
              UCB_NORM_NFKC);
    test_norm("갛é각Åﬁ가́̀Ź̌한글LigaturesﬃﬄﬅℵἄΩﬂῴ", "갛é각Åfi가́̀Ź̌한글LigaturesffifflstאἄΩflῴ",
              UCB_NORM_NFKD);

    UCB_MEMTRACK_POP();
}

struct ucd_norm_test
{
    int line_no;
    int test_no;
    std::string part_name;
    std::string input;
    std::string nfc;
    std::string nfd;
    std::string nfkc;
    std::string nfkd;
    std::string form;
    std::string comment;
};

static void read_ucd_norm_tests(std::vector<ucd_norm_test>& tests)
{
    std::string line;
    std::vector<std::string> parts;

    std::ifstream ifs(NORM_TEST_FILE, std::ios::in);
    REQUIRE(ifs.is_open());

    int part_index = -1;
    ucd_norm_test data;
    data.line_no = 0;
    data.test_no = 0;

    while (std::getline(ifs, line))
    {
        data.line_no++;
        if (line.empty() || line.at(0) == '#')
            continue;

        if (line.at(0) == '@')
        {
            part_index++;
            // New part, first is just index @Part0, @Part1, etc.
            split_line(line, '#', parts, 1);
            REQUIRE(parts.size() == 2);
            data.part_name = "Part " + std::to_string(part_index) + " - " + trim(parts[1]);
            continue;
        }

        data.test_no++;

        split_line(line, ';', parts, 5);
        REQUIRE(parts.size() >= 4);

        // parts 0 is input
        // parts 1 is NFC
        // parts 2 is NFD
        // parts 3 is NFKC
        // parts 4 is NFKD
        // parts 5 is comment, including initial #
        data.input   = hexstr_to_utf8(parts[0]);
        data.nfc     = hexstr_to_utf8(parts[1]);
        data.nfd     = hexstr_to_utf8(parts[2]);
        data.nfkc    = hexstr_to_utf8(parts[3]);
        data.nfkd    = hexstr_to_utf8(parts[4]);
        data.comment = parts.size() > 5 ? trim(parts[5]) : "";

        tests.push_back(data);
    }
}

TEST_CASE("unicode official normalization test")
{
    std::vector<ucd_norm_test> tests;
    read_ucd_norm_tests(tests);

    std::string form;
    for (const auto& test : tests)
    {
        CAPTURE(test.test_no);
        CAPTURE(test.line_no);
        CAPTURE(test.part_name);
        CAPTURE(test.comment);
        CAPTURE(form);

        // NFD
        form = "NFD = toNFD(INPUT)";
        test_norm(test.input.c_str(), test.nfd.c_str(), UCB_NORM_NFD);
        form = "NFD = toNFD(NFC)";
        test_norm(test.nfc.c_str(), test.nfd.c_str(), UCB_NORM_NFD);
        form = "NFD = toNFD(NFD)";
        test_norm(test.nfd.c_str(), test.nfd.c_str(), UCB_NORM_NFD);
        form = "NFKD = toNFD(NFKC)";
        test_norm(test.nfkc.c_str(), test.nfkd.c_str(), UCB_NORM_NFD);
        form = "NFKD = toNFD(NFKD)";
        test_norm(test.nfkd.c_str(), test.nfkd.c_str(), UCB_NORM_NFD);

        // NFC
        form = "NFC = toNFC(INPUT)";
        test_norm(test.input.c_str(), test.nfc.c_str(), UCB_NORM_NFC);
        form = "NFC = toNFC(NFC)";
        test_norm(test.nfc.c_str(), test.nfc.c_str(), UCB_NORM_NFC);
        form = "NFC = toNFC(NFD)";
        test_norm(test.nfd.c_str(), test.nfc.c_str(), UCB_NORM_NFC);
        form = "NFKC = toNFC(NFKC)";
        test_norm(test.nfkc.c_str(), test.nfkc.c_str(), UCB_NORM_NFC);
        form = "NFKC = toNFC(NFKD)";
        test_norm(test.nfkd.c_str(), test.nfkc.c_str(), UCB_NORM_NFC);

        // NFKD
        form = "NFKD = toNFKD(INPUT)";
        test_norm(test.input.c_str(), test.nfkd.c_str(), UCB_NORM_NFKD);
        form = "NFKD = toNFKD(NFC)";
        test_norm(test.nfc.c_str(), test.nfkd.c_str(), UCB_NORM_NFKD);
        form = "NFKD = toNFKD(NFD)";
        test_norm(test.nfd.c_str(), test.nfkd.c_str(), UCB_NORM_NFKD);
        form = "NFKD = toNFKD(NFKC)";
        test_norm(test.nfkc.c_str(), test.nfkd.c_str(), UCB_NORM_NFKD);
        form = "NFKD = toNFKD(NFKD)";
        test_norm(test.nfkd.c_str(), test.nfkd.c_str(), UCB_NORM_NFKD);

        // NFKC
        form = "NFKC = toNFKC(INPUT)";
        test_norm(test.input.c_str(), test.nfkc.c_str(), UCB_NORM_NFKC);
        form = "NFKC = toNFKC(NFC)";
        test_norm(test.nfc.c_str(), test.nfkc.c_str(), UCB_NORM_NFKC);
        form = "NFKC = toNFKC(NFD)";
        test_norm(test.nfd.c_str(), test.nfkc.c_str(), UCB_NORM_NFKC);
        form = "NFKC = toNFKC(NFKC)";
        test_norm(test.nfkc.c_str(), test.nfkc.c_str(), UCB_NORM_NFKC);
        form = "NFKC = toNFKC(NFKD)";
        test_norm(test.nfkd.c_str(), test.nfkc.c_str(), UCB_NORM_NFKC);
    }

    std::cout << "Number of tests definitions: " << tests.size() << std::endl;
    std::cout << "Number of normalizations run: " << tests.size() * 20 << std::endl;
}

TEST_CASE("benchmark normalization" * doctest::test_suite("benchmark") * doctest::skip())
{
    constexpr int iterations = 1000;

    std::vector<ucd_norm_test> tests;
    read_ucd_norm_tests(tests);

    auto start = std::chrono::high_resolution_clock::now();

    std::string form;
    uint64_t passed       = 0;
    uint64_t test_strlens = 0;
    for (const auto& test : tests)
    {
        test_strlens += test.input.size();
        test_strlens += test.nfc.size();
        test_strlens += test.nfd.size();
        test_strlens += test.nfkc.size();
        test_strlens += test.nfkd.size();
    }
    UCB_DIAG_PUSH()
    UCB_DIAG_IGN_IMPL_INT_FLOAT()
    UCB_DIAG_CLANG_IGN("-Wsource-uses-openmp")
    double test_avg_strlen = test_strlens / 5.0;

#ifdef USE_OPENMP
    omp_set_num_threads(8);
#endif

#pragma omp parallel for reduction(+ : passed)
    for (int i = 0; i < iterations; i++)
    {
        for (const auto& test : tests)
        {
            // NFD
            passed += test_norm_bench(test.input, test.nfd, UCB_NORM_NFD);
            passed += test_norm_bench(test.nfc, test.nfd, UCB_NORM_NFD);
            passed += test_norm_bench(test.nfd, test.nfd, UCB_NORM_NFD);
            passed += test_norm_bench(test.nfkc, test.nfkd, UCB_NORM_NFD);
            passed += test_norm_bench(test.nfkd, test.nfkd, UCB_NORM_NFD);

            // NFC
            passed += test_norm_bench(test.input, test.nfc, UCB_NORM_NFC);
            passed += test_norm_bench(test.nfc, test.nfc, UCB_NORM_NFC);
            passed += test_norm_bench(test.nfd, test.nfc, UCB_NORM_NFC);
            passed += test_norm_bench(test.nfkc, test.nfkc, UCB_NORM_NFC);
            passed += test_norm_bench(test.nfkd, test.nfkc, UCB_NORM_NFC);

            // NFKD
            passed += test_norm_bench(test.input, test.nfkd, UCB_NORM_NFKD);
            passed += test_norm_bench(test.nfc, test.nfkd, UCB_NORM_NFKD);
            passed += test_norm_bench(test.nfd, test.nfkd, UCB_NORM_NFKD);
            passed += test_norm_bench(test.nfkc, test.nfkd, UCB_NORM_NFKD);
            passed += test_norm_bench(test.nfkd, test.nfkd, UCB_NORM_NFKD);

            // NFKC
            passed += test_norm_bench(test.input, test.nfkc, UCB_NORM_NFKC);
            passed += test_norm_bench(test.nfc, test.nfkc, UCB_NORM_NFKC);
            passed += test_norm_bench(test.nfd, test.nfkc, UCB_NORM_NFKC);
            passed += test_norm_bench(test.nfkc, test.nfkc, UCB_NORM_NFKC);
            passed += test_norm_bench(test.nfkd, test.nfkc, UCB_NORM_NFKC);
        }
    }

    auto end           = std::chrono::high_resolution_clock::now();
    auto duration      = std::chrono::duration_cast<std::chrono::milliseconds>(end - start).count();
    uint64_t num_tests = tests.size() * iterations;
    uint64_t num_ops   = num_tests * 20;

    std::cout << "Total time: " << duration << " ms" << std::endl;
    std::cout << "Passed: " << std::fixed << std::setprecision(1)
              << 100 * passed / static_cast<double>(num_ops) << " %" << std::endl;
    std::cout << "Tests per second: " << std::fixed << std::setprecision(2)
              << (num_tests * 1000) / static_cast<double>(duration) << " s" << std::endl;
    std::cout << "Ops per second: " << std::fixed << std::setprecision(2)
              << (num_ops * 1000) / static_cast<double>(duration) << " s" << std::endl;
    std::cout << "Characters per second: " << std::fixed << std::setprecision(2)
              << 1000 * test_avg_strlen * iterations / static_cast<double>(duration) << " s"
              << std::endl;
    std::cout << "Average test time: " << duration / static_cast<double>(iterations) << " ms"
              << std::endl;

    UCB_DIAG_POP()
}

TEST_SUITE_END();
