# This file is part of the UCB project
# SPDX-FileCopyrightText: 2025 Åke Svedin <ake@svedin.org>
# SPDX-License-Identifier: MIT

include(cmake/warnings.cmake)

function(ucb_target_init name)
    set(options "")
    set(oneValueArgs "")
    set(multiValueArgs LANGUAGE)
    cmake_parse_arguments(ARGS "${options}" "${oneValueArgs}" "${multiValueArgs}" ${ARGN})

    foreach(_lang ${ARGS_LANGUAGE})
        if(NOT ${_lang} MATCHES "^C|CXX$")
            message(FATAL_ERROR "Unknown LANGUAGE argument ${_lang}")
        endif()
    endforeach()

    # Change to PUBLIC if features are needed in headers
    if("C" IN_LIST ARGS_LANGUAGE)
        target_compile_features(${name} PRIVATE c_std_11)
    endif()

    if("CXX" IN_LIST ARGS_LANGUAGE)
        target_compile_features(${name} PRIVATE cxx_std_11)
        if(NOT MSVC AND NOT APPLE AND CMAKE_CXX_COMPILER_ID STREQUAL "Clang")
            # 
            target_compile_options(${name} PRIVATE $<$<COMPILE_LANGUAGE:CXX>:-stdlib=libstdc++>)
            target_link_libraries(${name} PUBLIC -lstdc++ -lm)
        endif()
    endif()

    # Request access to memcpy_s and friends
    target_compile_definitions(${name} PRIVATE __STDC_WANT_LIB_EXT1__=1)

    # Enable Unicode support
    if(MSVC)
        # Source and execution character sets
        target_compile_options(${name} PRIVATE /utf-8)
        target_compile_definitions(${name} PRIVATE _UNICODE UNICODE)
    endif()

    ucb_target_set_warnings(${name})
endfunction()

function(ucb_add_executable name)
    set(options "")
    set(oneValueArgs "")
    set(multiValueArgs LANGUAGE)
    cmake_parse_arguments(ARGS "${options}" "${oneValueArgs}" "${multiValueArgs}" ${ARGN})

    add_executable(${name} ${ARGS_UNPARSED_ARGUMENTS})

    if(ARGS_LANGUAGE)
        set(init_args LANGUAGE "${ARGS_LANGUAGE}")
    endif()

    ucb_target_init(${name} ${init_args})
endfunction()

function(ucb_add_library name)
    set(options "")
    set(oneValueArgs "")
    set(multiValueArgs LANGUAGE)
    cmake_parse_arguments(ARGS "${options}" "${oneValueArgs}" "${multiValueArgs}" ${ARGN})

    add_library(${name} ${ARGS_UNPARSED_ARGUMENTS})

    if(ARGS_LANGUAGE)
        set(init_args LANGUAGE "${ARGS_LANGUAGE}")
    endif()

    ucb_target_init(${name} ${init_args})

    target_compile_definitions(${name} PRIVATE UCB_BUILD_LIB)

    if(BUILD_SHARED_LIBS)
        target_compile_definitions(${name} PUBLIC UCB_SHARED_LIB)
    else()
        target_compile_definitions(${name} PUBLIC UCB_STATIC_LIB)
    endif()

    if(NOT MSVC)
        set_target_properties(${name} PROPERTIES
            POSITION_INDEPENDENT_CODE ON
            C_VISIBILITY_PRESET hidden
            VISIBILITY_INLINES_HIDDEN ON
        )
        get_target_property(_type ${name} TYPE)

        if(_type STREQUAL "SHARED_LIBRARY")
            target_link_options(${name} PRIVATE -Wl,--exclude-libs,ALL)
        endif()
    endif()
endfunction()

# Convenience function to add public headers residing in another directory than CMAKE_CURRENT_SOURCE_DIR
function(ucb_target_public_headers target)
    cmake_parse_arguments(
        ARGS # prefix of output variables
        "" # list of names of the boolean arguments (only defined ones will be true)
        "DIRECTORY" # list of names of mono-valued arguments
        "FILES" # list of names of multi-valued arguments (output variables are lists)
        ${ARGN} # arguments of the function to parse, here we take the all original ones
    )

    if(NOT TARGET ${target})
        message(FATAL_ERROR "Target ${target} does not exist")
    endif()

    set(BASE_DIR ${CMAKE_SOURCE_DIR}/include)

    foreach(_header ${ARGS_FILES})
        if(ARGS_DIRECTORY)
            list(APPEND REL_FILES ${BASE_DIR}/${ARGS_DIRECTORY}/${_header})
        else()
            list(APPEND REL_FILES ${BASE_DIR}/${_header})
        endif()
    endforeach()

    target_sources(${target} PUBLIC FILE_SET HEADERS BASE_DIRS ${BASE_DIR} FILES ${REL_FILES})
endfunction()

# Nothing special for now
function(ucb_target_sources target)
    target_sources(${target} ${ARGN})
endfunction()

function(ucb_target_private_headers target)
    message(FATAL_ERROR "Not implemented")
endfunction()

function(ucb_install target)
    install(
        TARGETS ${target}
        EXPORT ucbTargets
        RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
        LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
        ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
        FILE_SET HEADERS DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}
    )
endfunction()
