# This file is part of the UCB project
# SPDX-FileCopyrightText: 2025 Åke Svedin <ake@svedin.org>
# SPDX-License-Identifier: MIT

# ----------------------------------------------------------------------------
# Minimal C program
# ----------------------------------------------------------------------------

# ucb_add_executable(ucb_main)
# ucb_target_sources(ucb_main PRIVATE main.c)
# target_link_libraries(ucb_main PRIVATE ucb)

# ----------------------------------------------------------------------------
# Actual tests
# ----------------------------------------------------------------------------

# Define data dir relative to build dir of target ucb
get_target_property(UCB_SOURCE_DIR ucb BINARY_DIR)
set(UNICODE_DATA_DIR "${UCB_SOURCE_DIR}/ucb_data")
set(NORM_TEST_FILE "${UNICODE_DATA_DIR}/NormalizationTest.txt")

# if(NOT EXISTS ${NORM_TEST_FILE})
#     message(FATAL_ERROR "Could not find ${NORM_TEST_FILE}")
# endif()

enable_language(CXX)

# Enable OpenMP in future maybe
find_package(OpenMP)

ucb_add_executable(ucb_tests LANGUAGE CXX)

ucb_target_sources(ucb_tests PRIVATE
    FILE_SET private_headers TYPE HEADERS BASE_DIRS . FILES
    test_math.h
    test_types.h
)

ucb_target_sources(ucb_tests PRIVATE
    main_doctest.cpp

    # Actual tests
    buffer.cpp
    math.cpp
    memory.cpp
    types.cpp
    unicode.cpp

    # Pure C tests
    test_math.c
    test_types.c

    ${NORM_TEST_FILE}
)

target_compile_definitions(ucb_tests PRIVATE NORM_TEST_FILE="${NORM_TEST_FILE}")

target_link_libraries(ucb_tests PRIVATE ucb)
if(OpenMP_FOUND)
    target_link_libraries(ucb_tests PRIVATE OpenMP::OpenMP_CXX)
    target_compile_definitions(ucb_tests PRIVATE USE_OPENMP)
    # target_link_libraries(ucb_tests PRIVATE $<$<CONFIG:Release>:OpenMP::OpenMP_CXX>)
    # target_compile_definitions(ucb_tests PRIVATE $<$<CONFIG:Release>:USE_OPENMP>)
endif()

set(doctest_VERSION 2.4.12)
# find_package(doctest ${doctest_VERSION} QUIET)

if(NOT doctest_FOUND)
    message(STATUS "Using internal doctest version ${doctest_VERSION}")
    target_include_directories(ucb_tests PRIVATE doctest)
    include(CTest)
    include(doctest/cmake/doctest.cmake)
else()
    message(STATUS "Using external doctest version ${doctest_VERSION}")
    target_link_libraries(ucb_tests PRIVATE doctest::doctest)
    include(CTest)

    # How to include doctest.cmake here?
    include(doctest)
endif()

if(NOT CMAKE_CROSSCOMPILING)
    doctest_discover_tests(ucb_tests)
endif()
