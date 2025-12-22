# UCB

Probably stands for: Unified C Base
It may, or may not, have another hidden meaning.

## Goal

The goal of this C library is to abstract common platform specific functionality
and provide a unified interface for the most common operations.

It is not intended to be a full replacement for the standard library. Focus will be on adding functionality that is either missing or do not behave the same on different platforms.

However, it will also add functionality where it makes sense to do.

The design is to use [UTF-8 everywhere](https://utf8everywhere.org/), unless otherwise
specified.

## Target

 - C11 for the core library
   - Supports calls from C++
   - Testing [doctest](https://github.com/doctest/doctest)] use C++
 - OS:
   - Windows
   - Linux
 - Compilers:
   - MSVC - Visual Studio 2022 or later tested.
   - GCC
   - Clang
 - Build systems:
   - CMake

Anything not listed above is not tested and is not a priority. Please help out if you want additional support and are able to maintain it.

## Features

 - Simplified error handling
   - Handles errno/GetLastError() and converts values to a unified error code.
   - Non-trivial functions either:
     - Return an error code.
     - Return a result struct containing error code and the expected result.
     - Calls an optional user-defined abort function on fatal errors (for instance ucb_malloc).
 - Memory management
   - Replacement for malloc/calloc/realloc/free
   - Optional memory tracking with manual leak detections.
     - Void macros for release builds to ensure optimal performance.
 - New string type
   - Always UTF-8 encoded
   - Keep tracks of C string length.
   - Internally NULL terminated allows use with C functions.
   - Large set of string operations and convenience functions.
 - Basic unicode support
   - Full Normalization (NFC, NFD, NFKC, NFKD)
   - Case Folding
   - Case Mapping
   - Possibly adding support for switchable backend.
   - Collation may use system calls if available (not there yet)
 - Threading
   - Threading and mutex
 - Buffer helpers

## Minor Features

 - Cross-platform C string function wrappers (like ucb_asprintf)
 - Platform macros
 - Utility macros

## Future plans

 - Add more string operations
 - File system functions
 - Process launching
 - Argument parsing
 - ???

## Limitations

 - Locale agnostic
   - No plan for date or time functions (yet).
 - Does not plan to be a full featured unicode library.
   - No plans for collation other than probably DUCET.
     - Use system calls if available.
   - Conversions to/from UTF-16/UTF-32 may be added later.
   - Currently expects UTF-8 on Linux and UTF-16 on Windows.
     - Use Windows APIs for conversions when needed.
 - No plan for networking, JSON, XML or such utilities.
 - No plan for GUI or graphics.
   - This is a console library only.

## Dependencies

This section needs verification.

 - CMake 3.28 or later
 - Python 3.6 or later
  
## Building

Simply use cmake.

Linux with GNU Make:

```bash
git clone git@github.com:herring-swe/ucb.git
mkdir ucb-build
cd ucb-build
cmake ../ucb -G "Unix Makefiles"
cmake --build .
```

For instance on Windows with Visual Studio 2022:

```bash
git clone git@github.com:herring-swe/ucb.git
mkdir ucb-build
cd ucb-build
cmake ../ucb -G "Visual Studio 17 2022" -A x64
cmake --build . --config Release
```

It is recommended to use Ninja.
  
## License

MIT License
Copyright (c) 2025 Åke Svedin <ake@svedin.org>

See [NOTICE.md](NOTICE.md) for details and [LICENSES](LICENSES) for the full license texts.

## Contributing

Please report bugs or request features by creating an issue.
Any help are welcome. Especially MacOS support.
