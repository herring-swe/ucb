# This file is part of the UCB project
# SPDX-FileCopyrightText: 2025 Åke Svedin <ake@svedin.org>
# SPDX-License-Identifier: MIT

set(UNICODE_DATA_DIR "${CMAKE_CURRENT_BINARY_DIR}/ucb_data")
include(${CMAKE_SOURCE_DIR}/cmake/unicode_data.cmake)

ucb_add_library(ucb LANGUAGE C)

if(TARGET unicode_data)
    add_dependencies(ucb unicode_data)
endif()

ucb_target_public_headers(ucb FILES
    ucb/buffer.h
    ucb/bufutil.h
    ucb/cstring.h
    ucb/defines.h
    ucb/error.h
    ucb/memdbg.h
    ucb/memory.h
    ucb/mutex.h
    ucb/string.h
    ucb/thread.h
    ucb/ucb.h
    ucb/unicode.h
)

ucb_target_sources(ucb PRIVATE
    FILE_SET private_headers TYPE HEADERS BASE_DIRS . FILES
    ucb/error_private.h
    ucb/mutex_private.h
    ucb/string_private.h

    FILE_SET generated_data TYPE HEADERS BASE_DIRS "${UNICODE_DATA_DIR}" FILES
    ${UNICODE_HEADERS}
)

ucb_target_sources(ucb PRIVATE
    ucb/buffer.c
    ucb/bufutil.c
    ucb/cstring.c
    ucb/error_private.c
    ucb/error.c
    ucb/memdbg.c
    ucb/memory.c
    ucb/mutex.c
    ucb/string.c
    ucb/thread.c
    ucb/ucb.c
    ucb/unicode.c
)

if(WIN32)
    ucb_target_sources(ucb PRIVATE
        ucb/string_win32.c
    )
elseif(UNIX)
    # target_sources(ucb PRIVATE
    # unix/string_unix.c
    # )
endif()

ucb_install(ucb)
