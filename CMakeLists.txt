# This file is part of the UCB project
# SPDX-FileCopyrightText: 2025 Åke Svedin <ake@svedin.org>
# SPDX-License-Identifier: MIT

cmake_minimum_required(VERSION 3.28)

# Come before project call
include(cmake/setup.cmake)

project(ucb VERSION 0.1.0 LANGUAGES C)

option(BUILD_SHARED_LIBS "Build using shared libraries" ON)
option(BUILD_TESTING "Build tests" ON)

option(UCB_CLANG_TIDY "Use clang-tidy" OFF)
option(UCB_WITH_BEAVER "Beaver mode" OFF)
option(UCB_WITH_UTF8 "Enable UTF-8 library" ON)
option(UCB_WITH_ICONV "Enable iconv library" OFF)

# External includes
include(GNUInstallDirs)

# Internal includes
include(cmake/functions.cmake)
include(cmake/sanitizers.cmake)

set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)

if(UCB_CLANG_TIDY)
    if(WIN32)
        find_program(CLANG_TIDY_EXE clang-tidy HINTS "$ENV{ProgramFiles}/LLVM/bin" NO_CACHE)
    else()
        find_program(CLANG_TIDY_EXE clang-tidy)
    endif()
    if(CLANG_TIDY_EXE)
        message(STATUS "Using clang-tidy: ${CLANG_TIDY_EXE}")
        set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
        # set(CMAKE_C_CLANG_TIDY "${CLANG_TIDY_EXE}" -checks=-*,readability-*)
        set(CMAKE_C_CLANG_TIDY clang-tidy;-p=${CMAKE_BINARY_DIR})
    else()
        message(WARNING "Could not find clang-tidy")
    endif()
endif()

if(UCB_WITH_UTF8 AND UCB_WITH_ICONV)
    find_package(Iconv REQUIRED)
endif()

add_subdirectory(src)

if(BUILD_TESTING)
    enable_testing()
    add_subdirectory(tests)
endif()

# ---------------------------------- Export ---------------------------------- #
install(
    EXPORT ucbTargets
    FILE ucbTargets.cmake
    NAMESPACE ucb::
    DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/ucb
)

include(CMakePackageConfigHelpers)
configure_package_config_file(
    ${CMAKE_CURRENT_SOURCE_DIR}/cmake/ucbConfig.cmake.in
    ${CMAKE_CURRENT_BINARY_DIR}/ucbConfig.cmake
    INSTALL_DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/ucb
)
write_basic_package_version_file(
    ${CMAKE_CURRENT_BINARY_DIR}/ucbConfigVersion.cmake
    VERSION ${PROJECT_VERSION}
    COMPATIBILITY SameMajorVersion
)
install(
    FILES
    ${CMAKE_CURRENT_BINARY_DIR}/ucbConfig.cmake
    ${CMAKE_CURRENT_BINARY_DIR}/ucbConfigVersion.cmake
    DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/ucb
)
