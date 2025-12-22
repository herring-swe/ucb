# This file is part of the UCB project
# SPDX-FileCopyrightText: 2025 Åke Svedin <ake@svedin.org>
# SPDX-License-Identifier: MIT

# This script downloads any data file given input variables
# DATA_URL, DATA_DEST and optional DATA_HASH.
# The file will be downloaded if the destination file is missing or the hash
# does not match. The script will fail if the download fails or the hash does
# not match.

if(NOT DATA_URL)
    message(FATAL_ERROR "DATA_URL not defined")
endif()
if(NOT DATA_DEST)
    cmake_path(GET DATA_URL FILENAME DATA_DEST)
    if(NOT DATA_DEST)
        message(FATAL_ERROR "DATA_DEST not defined and could not be derived from DATA_URL")
    endif()
endif()
if(DATA_HASH)
    set(ARG_HASH EXPECTED_HASH SHA256=${DATA_HASH})
endif()

cmake_path(GET DATA_URL FILENAME DATA_FILENAME)

# This command will only download if the file is missing or not matching hash
file(DOWNLOAD ${DATA_URL} "${DATA_DEST}" ${ARG_HASH})
