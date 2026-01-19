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
    ucb/btrace.h
    ucb/buffer.h
    ucb/bufutil.h
    ucb/cond.h
    ucb/cstring.h
    ucb/debug.h
    ucb/defines.h
    ucb/error.h
    ucb/memdbg.h
    ucb/memory.h
    ucb/mutex.h
    ucb/string.h
    ucb/task.h
    ucb/tgfunc.h
    ucb/threadpool.h
    ucb/threads.h
    ucb/time.h
    ucb/ucb.h
    ucb/unicode.h

    ucb/container/pqueue.h
)

ucb_target_sources(ucb PRIVATE
    FILE_SET private_headers TYPE HEADERS BASE_DIRS . FILES
    ucb/buffer_private.h
    ucb/cond_private.h
    ucb/mutex_private.h
    ucb/string_private.h
    ucb/sys_private.h

    ucb/container/common_private.h
    ucb/container/pqueue_private.h

    FILE_SET generated_data TYPE HEADERS BASE_DIRS "${UNICODE_DATA_DIR}" FILES
    ${UNICODE_HEADERS}
)

ucb_target_sources(ucb PRIVATE
    ucb/btrace.c
    ucb/buffer_private.c
    ucb/buffer.c
    ucb/bufutil.c
    ucb/cond.c
    ucb/cstring.c
    ucb/error_enum.c
    ucb/error_errno.c
    ucb/error.c
    ucb/memdbg.c
    ucb/memory.c
    ucb/mutex.c
    ucb/string.c
    ucb/sys_private.c
    ucb/task.c
    ucb/threadpool.c
    ucb/threads.c
    ucb/time.c
    ucb/ucb.c
    ucb/unicode.c

    ucb/container/pqueue.c
)

if(WIN32)
    ucb_target_sources(ucb PRIVATE
        ucb/string_win32.c
        ucb/error_win32.c
    )
    target_link_libraries(ucb PRIVATE dbghelp.lib)
elseif(UNIX)
    # target_sources(ucb PRIVATE
    # unix/string_unix.c
    # )
    # target_compile_options(ucb PRIVATE -rdynamic)
endif()

ucb_install(ucb)
