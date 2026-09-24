# This file is part of the UCB project
# SPDX-FileCopyrightText: 2025 Åke Svedin <ake@svedin.org>
# SPDX-License-Identifier: MIT

cmake_minimum_required(VERSION 3.26)

# Come before project call
include(cmake/setup.cmake)

project(ucb VERSION 0.2.1 LANGUAGES C)

# clangd, clang-tidy, and other tools. The Visual Studio generator still
# will not write this file; use Ninja or a Makefile generator for that.
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# Special options, do not rename as they have a meaning in CMake
option(BUILD_SHARED_LIBS "Build using shared libraries" ON)
option(BUILD_TESTING "Build tests" ON)

# Internal options and details
option(UCB_CLANG_TIDY "Use clang-tidy" OFF)
option(UCB_WITH_BEAVER "Beaver mode" OFF)
option(UCB_DEVEL "Enables development logging and assertions for debug builds" ON)
option(UCB_MEMTRACK_BACKTRACE "Enable backtrace support for memory tracking (SLOW)" OFF)
option(UCB_FAST "Enable performance optimizations" ON)
set(UCB_BTRACE_MAX_FRAMES 64 CACHE STRING "Maximum number of frames to capture in a backtrace")
set(UCB_MEMTRACK_FREE_CAPACITY 1024 CACHE STRING "Max retained free records for memory tracking")
set(UCB_MEMTRACK_MAX_TRACEPOINTS 10 CACHE STRING "Max nested memory tracking tracepoints")
set(UCB_MEMTRACK_MAX_TRACEPOINT_NAME 64 CACHE STRING "Max length of a memory tracking tracepoint name")

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

        # set(CMAKE_C_CLANG_TIDY "${CLANG_TIDY_EXE}" -checks=-*,readability-*)
        set(CMAKE_C_CLANG_TIDY clang-tidy;-p=${CMAKE_BINARY_DIR})
    else()
        message(WARNING "Could not find clang-tidy")
    endif()
endif()

# Internal config.h
configure_file(${CMAKE_SOURCE_DIR}/cmake/headers/config.h.cmake.in ${CMAKE_BINARY_DIR}/include/ucb/config.h)

# Public version.h
configure_file(${CMAKE_SOURCE_DIR}/cmake/headers/version.h.cmake.in ${CMAKE_BINARY_DIR}/include/ucb/version.h)
install(
    FILES ${CMAKE_BINARY_DIR}/include/ucb/version.h
    DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}/ucb
)

include_directories(${CMAKE_BINARY_DIR}/include)
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
