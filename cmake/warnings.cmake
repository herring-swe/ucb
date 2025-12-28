# This file is part of the UCB project
# SPDX-FileCopyrightText: 2025 Åke Svedin <ake@svedin.org>
# SPDX-License-Identifier: MIT

function(ucb_target_set_warnings name)
    # Warnings

    # GCC or Clang (including clang-cl)
    if(CMAKE_C_COMPILER_ID MATCHES "GNU|Clang")
        # Enable all warnings
        # Some are explicit, in case we want to reduce from -Wall -Wextra
        #
        # Counter-intuitive clang-cl:
        # -Wall is read as /Wall => -Weverything
        # /W4 => -Wall -Wextra
        target_compile_options(${name} PRIVATE -Wall -Wextra)

        target_compile_options(${name} PRIVATE
            -Wno-missing-field-initializers # Complains on = {0}
        )

        if(CMAKE_C_COMPILER_ID MATCHES "Clang")
            # Good flag, but not supported by GCC
            target_compile_options(${name} PRIVATE -Wnonportable-system-include-path)

            # Useful warnings
            target_compile_options(${name} PRIVATE
                -Werror=implicit-function-declaration
            )

            # Clang might add more warnings. Some are disabled here
            # add -Wno-unsafe_buffer_usage ?
            target_compile_options(${name} PRIVATE
                -Wno-declaration-after-statement
                -Wno-covered-switch-default
                -Wno-cast-qual # Use linter for this and mark with /NOLINT
                -Wno-c++98-compat # Clang-cl...
                -Wno-c++98-compat-pedantic # Clang-cl...
                # -Wno-format-nonliteral # Eh...
            )

            # Disable some silly warnings when compiling C code with clang-cl
            if(MSVC)
                target_compile_options(${name} PRIVATE
                    $<$<COMPILE_LANGUAGE:C>:
                    -Wno-pre-c11-compat
                    -Wno-c++-keyword
                    -Wno-unsafe-buffer-usage
                    >
                )
            endif()

            # And more silly warnings with C++ (doctest)
            target_compile_options(${name} PRIVATE
                $<$<COMPILE_LANGUAGE:CXX>:
                -Wno-unsafe-buffer-usage
                >
            )

        else()
            # GCC Only

            # Useful warnings, only supported for C
            target_compile_options(${name} PRIVATE
                $<$<COMPILE_LANGUAGE:C>:
                -Werror=implicit-function-declaration
                -Wmissing-field-initializers
                >
            )
        endif()
    elseif(MSVC)
        # Proper MSVC here
        # Default warnings
        target_compile_options(${name} PRIVATE /W4)
        target_compile_options(${name} PRIVATE /we4013) # implicit-function-declaration

        # Disable:
        # C4068 - Unknown pragma (like #pragma clang)
        # target_compile_options(${name} PRIVATE /wd4068)
    endif()
endfunction()
