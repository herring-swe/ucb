# This file is part of the UCB project
# SPDX-FileCopyrightText: 2025 Åke Svedin <ake@svedin.org>
# SPDX-License-Identifier: MIT

set(UNICODE_DATA_DIR "${CMAKE_CURRENT_BINARY_DIR}/ucb_data")
include(${CMAKE_SOURCE_DIR}/cmake/unicode_data.cmake)

# ---------------------------------------------------------------------------- #
#                             Pre-generate sources                             #
# ---------------------------------------------------------------------------- #

set(_types int float double)
set(_suffixes int flt dbl)
foreach(_type _suffix IN ZIP_LISTS _types _suffixes)
    ucb_generate_from_template(
        TEMPLATE container/templates/vector_template.h
        HEADER container/impl/vector_${_suffix}.h
        SOURCE container/impl/vector_${_suffix}.c
        COMMON_DEFINES UCB_T_SUFFIX=${_suffix} UCB_T_EXPORT=UCB_API UCB_T_IS_POD UCB_T_CMP_FUNC=ucb_comp_func_${_suffix}
        HEADER_DEFINES "UCB_T_DECLARE=${_type}" 
        SOURCE_DEFINES "UCB_T_DEFINE=${_type}"
        HEADER_INCLUDES "<ucb/export.h>"
    )
endforeach()

ucb_generate_from_template(
    TEMPLATE container/templates/vector_template.h
    HEADER container/impl/vector_ptr.h
    SOURCE container/impl/vector_ptr.c
    COMMON_DEFINES UCB_T_SUFFIX=ptr UCB_T_EXPORT=UCB_API UCB_T_CMP_FUNC=ucb_comp_func_ptr 
    HEADER_DEFINES "UCB_T_DECLARE=void*" 
    SOURCE_DEFINES "UCB_T_DEFINE=void*"
    HEADER_INCLUDES "<ucb/export.h>"
)

ucb_generate_from_template(
    TEMPLATE container/templates/vector_template.h
    HEADER container/impl/vector_str.h
    SOURCE container/impl/vector_str.c
    COMMON_DEFINES UCB_T_SUFFIX=str UCB_T_EXPORT=UCB_API UCB_T_FREE_FUNC=ucb_str_free UCB_T_CLONE_FUNC=ucb_str_clone UCB_T_CMP_FUNC=ucb_str_cmp_func 
    HEADER_DEFINES "UCB_T_DECLARE=struct ucb_str*" 
    SOURCE_DEFINES "UCB_T_DEFINE=struct ucb_str*"
    HEADER_INCLUDES "<ucb/string.h>" "<ucb/export.h>"
)

# ---------------------------------------------------------------------------- #
#                                    Library                                   #
# ---------------------------------------------------------------------------- #

ucb_add_library(ucb LANGUAGE C)

if(TARGET unicode_data)
    add_dependencies(ucb unicode_data)
endif()

# if(TARGET container_data)
#     add_dependencies(ucb container_data)
# endif()

ucb_target_public_headers(ucb FILES
    ucb/container/impl/vector_dbl.h
    ucb/container/impl/vector_flt.h
    ucb/container/impl/vector_int.h
    ucb/container/impl/vector_ptr.h
    ucb/container/impl/vector_str.h
    
    ucb/container/templates/vector_template.h

    ucb/container/pqueue.h
    ucb/container/vector_generic.h
    ucb/container/vector.h
    
    ucb/algorithm.h
    ucb/btrace.h
    ucb/buffer.h
    ucb/bufutil.h
    ucb/cond.h
    ucb/cstring.h
    ucb/debug.h
    ucb/defines.h
    ucb/env.h
    ucb/envmap.h
    ucb/error.h
    ucb/memdbg.h
    ucb/memory.h
    ucb/mutex.h
    ucb/once.h
    ucb/string.h
    ucb/task.h
    ucb/tgfunc.h
    ucb/threadpool.h
    ucb/threads.h
    ucb/time.h
    ucb/tls.h
    ucb/ucb.h
    ucb/unicode_enum.h
    ucb/unicode.h
)

ucb_target_sources(ucb PRIVATE
    FILE_SET private_headers TYPE HEADERS BASE_DIRS . FILES
    ucb/buffer_private.h
    ucb/cond_private.h
    ucb/mutex_private.h
    ucb/once_private.h
    ucb/sys_private.h
    ucb/unicode_private.h

    ucb/container/common_private.h
    ucb/container/pqueue_private.h

    FILE_SET generated_data TYPE HEADERS BASE_DIRS "${UNICODE_DATA_DIR}" FILES
    ${UNICODE_HEADERS}
)

ucb_target_sources(ucb PRIVATE
    ucb/container/impl/vector_dbl.c
    ucb/container/impl/vector_flt.c
    ucb/container/impl/vector_int.c
    ucb/container/impl/vector_ptr.c
    ucb/container/impl/vector_str.c
    
    ucb/container/pqueue.c
    ucb/container/vector_generic.c

    ucb/algorithm.c
    ucb/btrace.c
    ucb/buffer_private.c
    ucb/buffer.c
    ucb/bufutil.c
    ucb/cond.c
    ucb/cstring.c
    ucb/env.c
    ucb/envmap.c
    ucb/error_enum.c
    ucb/error_errno.c
    ucb/error.c
    ucb/memdbg.c
    ucb/memory.c
    ucb/mutex.c
    ucb/once.c
    ucb/string.c
    ucb/sys_private.c
    ucb/task.c
    ucb/threadpool.c
    ucb/threads.c
    ucb/time.c
    ucb/tls.c
    ucb/ucb.c
    ucb/unicode_enum.c
    ucb/unicode.c
)

if(WIN32)
    ucb_target_sources(ucb PRIVATE
        ucb/cstring_win32.c
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
