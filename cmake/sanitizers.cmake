# This file is part of the UCB project
# SPDX-FileCopyrightText: 2025 Åke Svedin <ake@svedin.org>
# SPDX-License-Identifier: MIT

option(UCB_WITH_ASAN "Enable Address Sanitizer (ASan)" OFF)
if(NOT MSVC)
    option(UCB_WITH_UBSAN "Enable Undefined Behavior Sanitizer (UBSan)" OFF)
endif()

# Documentation is not fully checked, got from Mistral
# Hallicunations may occur
#
# ASAN:
# 
# GCC and Clang common:
#   -fsanitize=address,leak         Enable ASAN, explicity enabling leak detection
#   -fno-omit-frame-pointer         Enable for better stack traces
#   -fno-common                     Enables tracking of globals. (don't treat globals as variables)
#   -fsanitize-recover=address      Allows ASAN to continue after errors to track additional bugs
#   -fsanitize-address-use-odr-indicator Enabled ODR viotions (One Definition Rule)
#   -fno-optimize-sibling-calls     Improves stack traces
#
# Clang only:
#   -fsanitize-address-use-after-scope     Detect use-after-scope (may be unreliable)
#   -fsanitize-address-globals-dead-strip   (Self-describing)
#
# MSVC only:
#   /fsanitize=address              Enable ASAN, leak check not supported (as of 2025-12-14)

# UBSAN:
#
# GCC and Clang common:
#   -fsanitize=undefined            Enable UBSAN
#   -fsanitize-trap=all             Trap on all undefined behavior
#
# MSVC only:
#   /fsanitize=undefined            Enable UBSAN, not supported yet

set(FLAGS "")
set(LINK_FLAGS "")
set(FEATURE "")
if(UCB_WITH_ASAN)
    if(MSVC)
        set(FEATURE "ASan")
        list(APPEND FLAGS /fsanitize=address /Zi)
    elseif(CMAKE_C_COMPILER_ID MATCHES "GNU|Clang")
        set(FEATURE "ASan")
        list(APPEND FLAGS -fsanitize=address,leak -fno-omit-frame-pointer -fno-common)
    else()
        message(WARNING "Unknown compiler for ASan. Ingoring...")
    endif()
endif()

if(UCB_WITH_UBSAN)
    if(MSVC)
        message(WARNING "UBSan is not supported on MSVC. Ignoring...")
    elseif(CMAKE_C_COMPILER_ID MATCHES "GNU|Clang")
        if(FEATURE)
            set(FEATURE "${FEATURE}/UBSan")
        else()
            set(FEATURE "UBSan")
        endif()
        list(APPEND FLAGS -fsanitize=undefined -fsanitize-trap=all)
    else()
        message(WARNING "Unknown compiler for UBSan. Ignoring...")
    endif()
endif()

if(FLAGS)
    get_property(is_multi_config GLOBAL PROPERTY GENERATOR_IS_MULTI_CONFIG)
    # message("FLAGS: ${FLAGS}")
    # message("FEATURE: ${FEATURE}")
    # message("is_multi_config: ${is_multi_config}")
    # message("CMAKE_BUILD_TYPE: ${CMAKE_BUILD_TYPE}")

    if(is_multi_config)
        message(STATUS "Enabling ${FEATURE} for debug builds only")
    elseif(CMAKE_BUILD_TYPE STREQUAL "Debug")
        message(STATUS "Enabling ${FEATURE}")
    else()
        message(WARNING "Ignoring ${FEATURE} for non-debug builds")
        return()
    endif()

    add_compile_options("$<$<CONFIG:DEBUG>:${FLAGS}>")
    if(LINK_FLAGS)
        add_link_options("$<$<CONFIG:DEBUG>:${LINK_FLAGS}>")
    endif()
endif()
