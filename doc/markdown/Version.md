# Version and capabilities

The generated header `ucb/version.h` is the authoritative compile-time capability
query surface of the library. Every capability is defined as `0` or `1`, so it can
be tested directly without `defined()`:

```c
#include <ucb/version.h>

#if UCB_OS_WINDOWS
// Windows specific code
#endif
```

The defines are split by their source of truth:

- **Baked by CMake** (describe the shipped library)
  - `UCB_VER_MAJOR`, `UCB_VER_MINOR`, `UCB_VER_PATCH`
  - `UCB_VER_STRING` — version as a string literal, e.g. `"0.2.1"`
  - `UCB_VER` — numeric `MAJOR*10000 + MINOR*100 + PATCH`
  - `UCB_BUILD_SHARED` — `1` for a shared build, `0` for static
  - `UCB_WITH_<COMPONENT>` — optional component availability
  - `UCB_BACKEND_<NAME>` — backend availability

- **Derived at include time** (describe the consumer's environment)
  - `UCB_OS_WINDOWS`, `UCB_OS_LINUX`
  - `UCB_COMPILER_CLANG`, `UCB_COMPILER_MSVC`, `UCB_COMPILER_GCC`

Platform and compiler defines are resolved from predefined macros in the header
itself, so a consumer compiled with a different toolchain than the shipped library
still sees correct values.

## Optional components

An optional component `<COMPONENT>` exposes `UCB_WITH_<COMPONENT>` and a backend
`<NAME>` exposes `UCB_BACKEND_<NAME>`, both as `0`/`1`. No optional component
exists yet; the first planned is `UCB_WITH_ENCODING` (Encoding API).

At runtime `ucb_get_version()` returns the same value as `UCB_VER_STRING`.