# This file is part of the UCB project
# SPDX-FileCopyrightText: 2025 Åke Svedin <ake@svedin.org>
# SPDX-License-Identifier: MIT

# Target definitions to download and create unicode data

# Define the files needed and their hashes.
# We use fixed version, to be able to verify the hashes and reproduce builds.
set(DATA_URLS "")
set(DATA_HASHES "")

list(APPEND DATA_URLS https://www.unicode.org/Public/17.0.0/ucd/UnicodeData.txt)
list(APPEND DATA_HASHES "2e1efc1dcb59c575eedf5ccae60f95229f706ee6d031835247d843c11d96470c")

list(APPEND DATA_URLS https://www.unicode.org/Public/17.0.0/ucd/CaseFolding.txt)
list(APPEND DATA_HASHES "ff8d8fefbf123574205085d6714c36149eb946d717a0c585c27f0f4ef58c4183")

list(APPEND DATA_URLS https://www.unicode.org/Public/17.0.0/ucd/CompositionExclusions.txt)
list(APPEND DATA_HASHES "2f239196ef3b5b61db5cc476e9bd80f534d15aa1b74e1be1dea5d042a344c85f")

list(APPEND DATA_URLS https://www.unicode.org/Public/17.0.0/ucd/SpecialCasing.txt)
list(APPEND DATA_HASHES "efc25faf19de21b92c1194c111c932e03d2a5eaf18194e33f1156e96de4c9588")

list(APPEND DATA_URLS https://www.unicode.org/Public/17.0.0/ucd/NormalizationTest.txt)
list(APPEND DATA_HASHES "5019ffd530751a741900c849c0e010332f142a3612234639bd200b82138a87db")


# Output folder
if(NOT UNICODE_DATA_DIR)
    set(UNICODE_DATA_DIR "${CMAKE_BINARY_DIR}/ucb_data")
endif()

# Define the processing script
find_package(Python 3.6 COMPONENTS Interpreter)

if(NOT Python_FOUND)
    message(FATAL_ERROR "Pyton 3.6 or later required for processing Unicode data")
endif()

# Make sure data directory exists
file(MAKE_DIRECTORY "${UNICODE_DATA_DIR}")

if(NOT EXISTS "${UNICODE_DATA_DIR}")
    message(FATAL_ERROR "Could not create directory ${UNICODE_DATA_DIR}")
endif()

# Define commands that will download each file
set(DATA_FILES "")

foreach(DATA_URL DATA_HASH IN ZIP_LISTS DATA_URLS DATA_HASHES)
    cmake_path(GET DATA_URL FILENAME DATA_FILENAME)
    list(APPEND DATA_FILES "${UNICODE_DATA_DIR}/${DATA_FILENAME}")
    add_custom_command(
        OUTPUT "${UNICODE_DATA_DIR}/${DATA_FILENAME}"
        COMMAND "${CMAKE_COMMAND}"
        -DDATA_URL=${DATA_URL}
        -DDATA_DEST=${DATA_FILENAME}
        -DDATA_HASH=${DATA_HASH}
        -P "${CMAKE_SOURCE_DIR}/cmake/scripts/cmd_download_file.cmake"
        WORKING_DIRECTORY "${UNICODE_DATA_DIR}"
        COMMENT "Downloading unicode data file: ${DATA_FILENAME}"
        VERBATIM
    )
endforeach()

set(PARSER_DIR ${CMAKE_SOURCE_DIR}/tools)
set(PARSER_FILES
    ${PARSER_DIR}/ucparser.py
    ${PARSER_DIR}/ucparser/cli.py
    ${PARSER_DIR}/ucparser/types.py
    ${PARSER_DIR}/ucparser/parser.py
    ${PARSER_DIR}/ucparser/util.py
)


set(TEMPLATE_DIR ${CMAKE_SOURCE_DIR}/tools/ucparser/templates)
set(TEMPLATE_FILES
    ${TEMPLATE_DIR}/combine.h.jinja
    ${TEMPLATE_DIR}/decomp.h.jinja
    ${TEMPLATE_DIR}/defines.h.jinja
    ${TEMPLATE_DIR}/mapping.h.jinja
    ${TEMPLATE_DIR}/props.h.jinja
)

set(UNICODE_HEADERS
    ${UNICODE_DATA_DIR}/unicode_combine.h
    ${UNICODE_DATA_DIR}/unicode_defines.h
    ${UNICODE_DATA_DIR}/unicode_props.h
    ${UNICODE_DATA_DIR}/unicode_mapping.h
    ${UNICODE_DATA_DIR}/unicode_decomp.h
)
set(UNICODE_HEADERS ${UNICODE_HEADERS} PARENT_SCOPE)

# Process unicode data with Python script
add_custom_command(
    OUTPUT ${UNICODE_HEADERS}
    DEPENDS "${DATA_FILES}" "${TEMPLATE_FILES}" "${PARSER_FILES}"
    COMMAND "${Python_EXECUTABLE}" "${CMAKE_SOURCE_DIR}/tools/ucparser.py"
    WORKING_DIRECTORY "${UNICODE_DATA_DIR}"
    # COMMENT "Processing unicode data"
    VERBATIM
)
add_custom_target(unicode_data DEPENDS ${UNICODE_HEADERS})
