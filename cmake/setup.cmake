# This file is part of the UCB project
# SPDX-FileCopyrightText: 2025 Åke Svedin <ake@svedin.org>
# SPDX-License-Identifier: MIT

get_property(is_multi_config GLOBAL PROPERTY GENERATOR_IS_MULTI_CONFIG)
if(NOT is_multi_config)
    set(CMAKE_BUILD_TYPE "Release" CACHE STRING "Choose the build type")
endif()

# if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
#   message(STATUS "Setting build type to '${default_build_type}' as none was specified.")
#   set(CMAKE_BUILD_TYPE "${default_build_type}" CACHE
#       STRING "Choose the type of build." FORCE)
#   # Set the possible values of build type for cmake-gui
#   set_property(CACHE CMAKE_BUILD_TYPE PROPERTY STRINGS
#     "Debug" "Release" "MinSizeRel" "RelWithDebInfo")
# endif()
