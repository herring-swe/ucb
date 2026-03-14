# GitHub Copilot Instructions

## Code Style
This project uses **clang-format** for all C and C++ formatting.
Do **not** comment on formatting, indentation, brace style, line length, or whitespace — clang-format enforces those automatically.
Focus code review on correctness, logic, safety, and API consistency instead.

## Language Standards
- C code targets **C11**
- C++ code targets **C++11**

## Platform
- Primary targets: Linux (GCC/Clang) and Windows (MSVC)
- POSIX-specific code must be guarded with `#ifndef _WIN32`
