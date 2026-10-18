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
set(GRAPHEME_TEST_FILE "${UNICODE_DATA_DIR}/GraphemeBreakTest.txt")

# if(NOT EXISTS ${NORM_TEST_FILE})
# message(FATAL_ERROR "Could not find ${NORM_TEST_FILE}")
# endif()
enable_language(CXX)

# Enable OpenMP in future maybe
find_package(OpenMP)

ucb_add_executable(ucb_tests LANGUAGE CXX)

ucb_target_sources(ucb_tests PRIVATE
    FILE_SET private_headers TYPE HEADERS BASE_DIRS . FILES
    common.h
    fs_tmp.h
    test_bytes_c.h
    test_cast.h
    test_string_c.h
    test_tgfunc.h
    test_threads.h
    test_types.h
    ucd_corpus.h

    container/my_type.h
)

ucb_target_sources(ucb_tests PRIVATE
    main_doctest.cpp
    ucd_corpus.cpp

    # Actual tests
    argparse.cpp
    btrace.cpp
    buffer.cpp
    bytes.cpp
    cast.cpp
    common.cpp
    cond.cpp
    dir.cpp
    env.cpp
    envmap.cpp
    error.cpp
    file.cpp
    fs.cpp
    fs_tmp.cpp
    memory.cpp
    mutex.cpp
    once.cpp
    path.cpp
    process.cpp
    string.cpp
    tgfunc.cpp
    threadpool.cpp
    threads.cpp
    tls.cpp
    types.cpp
    unicode.cpp
    version.cpp

    container/my_type.c
    container/pqueue.cpp
    container/vector.cpp

    # Pure C tests
    test_bytes_c.c
    test_cast.c
    test_string_c.c
    test_tgfunc.c
    test_threads.c
    test_types.c

    ${NORM_TEST_FILE}
    ${GRAPHEME_TEST_FILE}
)

target_compile_definitions(ucb_tests PRIVATE NORM_TEST_FILE="${NORM_TEST_FILE}")
target_compile_definitions(ucb_tests PRIVATE GRAPHEME_TEST_FILE="${GRAPHEME_TEST_FILE}")
target_compile_definitions(ucb_tests PRIVATE UCB_TEST_EXPECTED_VERSION="${PROJECT_VERSION}")

target_link_libraries(ucb_tests PRIVATE ucb)

target_include_directories(ucb_tests PRIVATE ${CMAKE_SOURCE_DIR}/src)

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

# ----------------------------------------------------------------------------
# Separate internal bench library and sample runner
# ----------------------------------------------------------------------------

add_library(ucb_bench STATIC bench/microbench.cpp bench/microbench.h)
ucb_target_init(ucb_bench LANGUAGE CXX)
target_include_directories(ucb_bench PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/bench)

ucb_add_executable(ucb_benchmarks LANGUAGE CXX)
target_sources(ucb_benchmarks PRIVATE
    bench/bench_main.cpp
    bench/pqueue_bench.cpp
    bench/threads_bench.cpp
    bench/vector_bench_int.cpp
    bench/vector_bench_ptr.cpp
    bench/unicode_bench.cpp
    bench/bench_c.c
    ucd_corpus.cpp
)
target_link_libraries(ucb_benchmarks PRIVATE ucb_bench ucb)
target_include_directories(ucb_benchmarks PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}/bench
    ${CMAKE_SOURCE_DIR}/src
)
target_compile_definitions(ucb_benchmarks PRIVATE NORM_TEST_FILE="${NORM_TEST_FILE}")

# ----------------------------------------------------------------------------
# Optional comparisons against external Unicode libraries
#
# Opt-in via UCB_BENCH_COMPARE. utf8proc falls back to the vendored copy in
# ext/ when no system package is found. ICU is assumed present on Windows (the
# in-box ICU) and otherwise located with FindICU; where it is missing the
# comparison is skipped with a status message, never a hard failure.
# ----------------------------------------------------------------------------

if(UCB_BENCH_COMPARE)
    find_package(utf8proc QUIET)
    if(utf8proc_FOUND)
        set(UCB_BENCH_UTF8PROC_TARGET utf8proc::utf8proc)
        set(UCB_BENCH_UTF8PROC_FOUND TRUE)
    elseif(EXISTS "${CMAKE_SOURCE_DIR}/ext/utf8proc/utf8proc.c")
        add_library(ucb_utf8proc "${CMAKE_SOURCE_DIR}/ext/utf8proc/utf8proc.c")
        target_include_directories(ucb_utf8proc SYSTEM PUBLIC "${CMAKE_SOURCE_DIR}/ext/utf8proc")
        if(BUILD_SHARED_LIBS)
            target_compile_definitions(ucb_utf8proc PRIVATE UTF8PROC_EXPORTS)
        else()
            target_compile_definitions(ucb_utf8proc PUBLIC UTF8PROC_STATIC)
        endif()
        set_target_properties(ucb_utf8proc PROPERTIES C_CLANG_TIDY "" OUTPUT_NAME utf8proc)
        if(MSVC)
            target_compile_options(ucb_utf8proc PRIVATE /utf-8)
        endif()
        set(UCB_BENCH_UTF8PROC_TARGET ucb_utf8proc)
        set(UCB_BENCH_UTF8PROC_FOUND TRUE)
    else()
        set(UCB_BENCH_UTF8PROC_FOUND FALSE)
    endif()

    if(UCB_BENCH_UTF8PROC_FOUND)
        message(STATUS "Benchmarks: utf8proc comparison enabled")
        target_compile_definitions(ucb_benchmarks PRIVATE UCB_BENCH_UTF8PROC)
        target_link_libraries(ucb_benchmarks PRIVATE ${UCB_BENCH_UTF8PROC_TARGET})
    else()
        message(STATUS "Benchmarks: utf8proc not found, comparison disabled")
    endif()

    # ICU. On Windows the in-box ICU (Windows 10 1703+) is assumed present and
    # used through its C API, which is all it ships. Everywhere else FindICU
    # locates a standard installation, or the comparison is skipped.
    if(WIN32)
        message(STATUS "Benchmarks: ICU comparison enabled (Windows in-box ICU)")
        target_compile_definitions(ucb_benchmarks PRIVATE UCB_BENCH_ICU)
        target_link_libraries(ucb_benchmarks PRIVATE icuuc)
    else()
        find_package(ICU QUIET COMPONENTS uc)
        if(ICU_FOUND)
            message(STATUS "Benchmarks: ICU comparison enabled (ICU ${ICU_VERSION})")
            target_compile_definitions(ucb_benchmarks PRIVATE UCB_BENCH_ICU)
            target_link_libraries(ucb_benchmarks PRIVATE ICU::uc)
        else()
            message(STATUS "Benchmarks: ICU not found, comparison disabled")
        endif()
    endif()
endif()

# Intentionally not registered with CTest: benchmarks are run manually, not as
# part of the default test suite.
