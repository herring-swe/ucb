# UCB Roadmap TODO

## Scope and Versioning Rules

- This is a roadmap for the first stable release and the remaining 0.x development releases.
- 0.x is development: API/ABI breaks are allowed when needed.
- 1.0.0 means: API is proven and ABI is stable.
- After 1.0.0, release numbering means:
- Minor release (`1.x.0`): feature additions, no ABI break.
- Patch release (`1.0.x`): fixes only.
- This is a one-man, spare-time project: focus on functionality first, then testing growth, then release process.
- For this project, 1.0.0 is not considered complete unless filesystem API, process launching, argument parsing, and settings helper are included.

## Release Checklist (Compact)

Only checkboxes here. Details exist only in Feature List.
Order: release first, priority second.

### 0.1.0

#### P0
- [x] Core: Core base APIs available
- [x] Error Handling: Error model and error codes available
- [x] Memory: Memory and memdbg base available
- [x] String: UTF-8 string type available
- [x] Unicode: Unicode normalize/map/validate available
- [x] Threads: Threads, mutex, cond, threadpool, task available
- [x] Buffer: Buffer and bufutil available
- [x] Containers: Container pqueue available
- [x] Type-Generic Helpers: Type-generic helpers available

#### P1
- [x] Build: Cross-platform build baseline fully validated
- [x] Documentation: Doxygen completeness baseline

### 0.2.0

#### P0
- [x] Containers: Vector template and generation tool initial usable feature set
- [ ] Threads: Thread/mutex robustness pass
- [ ] Error Handling: Error handling behavior pass
- [ ] Build: ASAN/UBSAN integration in build/test flow
- [x] Environment: Environment routines initial usable feature set
- [x] Version: Generated version defines initial usable feature set

#### P1
- [ ] Memory: Memdbg quality improvements
- [ ] Unicode: Grapheme and unicode quality improvements
- [ ] Buffer: Buffer view semantics decision

### 0.3.0

#### P0
- [ ] File: File abstraction API initial usable feature set
- [ ] Filesystem: Filesystem API initial usable feature set
- [ ] Process: Process launching API initial usable feature set

#### P1
- [ ] Samples: Project integration samples for new features
- [ ] Build: Packaging artifacts include API docs and changelog

### 0.4.0

#### P0
- [ ] Argument Parsing: Argument parsing API initial usable feature set

### 0.5.0

#### P0
- [ ] Settings: Settings helper API initial usable feature set

### 0.6.0

### P0
- [ ] Encoding: Encoding API initial usable feature set

### 1.0.0

#### P0
- [ ] Release: Release gates for stable API/ABI
- [ ] Version: Generated version defines complete for 1.0.0 scope
- [ ] File: File abstraction API complete for 1.0.0 scope
- [ ] Filesystem: Filesystem API complete for 1.0.0 scope
- [ ] Process: Process launching complete for 1.0.0 scope
- [ ] Argument Parsing: Argument parsing complete for 1.0.0 scope
- [ ] Settings: Settings helper complete for 1.0.0 scope

#### P1
- [ ] Documentation: Non-Doxygen docs and migration/release notes complete
- [ ] Samples: Samples complete and verified

## Feature List

This is the only expanded section.
A feature is complete only when all of its sub-items are checked.
For Documentation only Doxygen is considered necessary.

### Core - Core Base API (Target: 0.1.0)
- [x] Public entry API (`ucb.h`) in place
- [x] Common macros/types/exports in place
- [ ] Testing
- [ ] Documentation

### Error Handling - Error Handling (Target: 0.1.0, harden in 0.2.0)
- [x] Unified error code system
- [x] Report vs throw model
- [x] Platform mapping (POSIX/Win32)
- [ ] Robustness pass for edge/error paths
- [ ] Testing
- [ ] Documentation

### Memory - Memory and Memdbg (Target: 0.1.0, improve in 0.2.0)
- [x] malloc/calloc/realloc/free wrappers
- [x] Memory tracking and leak report baseline
- [ ] Free call tracking improvements
- [ ] Configurability and diagnostics refinements
- [ ] Testing
- [ ] Documentation

### String - UTF-8 String Type (Target: 0.1.0)
- [x] Owned/wrapped string semantics
- [x] Core manipulation APIs
- [x] Case and normalization integration
- [ ] Testing
- [ ] Documentation

### Unicode - Unicode (Target: 0.1.0, improve in 0.2.0)
- [x] Validate, iterate, normalize, map baseline
- [ ] Proper Grapheme handling
- [ ] Quick normalization paths
- [ ] Testing
- [ ] Documentation

### Threads - Threads and Concurrency (Target: 0.1.0, harden in 0.2.0)
- [x] Threads API (create/join/config)
- [x] Mutex and condition variable API
- [x] Threadpool and task API
- [ ] Error-path robustness under resource pressure
- [ ] Testing
- [ ] Documentation

### Buffer - Buffer and Bufutil (Target: 0.1.0, refine in 0.2.0)
- [x] Static/heap buffer baseline
- [x] Grow/resize/transfer helpers
- [ ] Buffer view behavior finalized
- [ ] Testing
- [ ] Documentation

### Containers - Containers (Target: 0.1.0, extend in 0.2.0)
- [x] Priority queue baseline
- [x] Vector template header
- [x] Template header generation tool for typed implementations (e.g., `vector_int.h`, `vector_int.c`)
- [ ] Testing
- [ ] Documentation

### Type-Generic Helpers - Type-Generic Helpers (Target: 0.1.0)
- [x] Type generic helper macros/functions baseline
- [ ] Testing
- [ ] Documentation

### Version - Generated Version Defines (Target: 0.2.0, required by 1.0.0)
- [ ] `ucb/version.h` generated by CMake
- [ ] Combined version define available
- [ ] Separate version defines available (`MAJOR`, `MINOR`, `PATCH`)
- [ ] Build/compiler/platform/backend capability defines available
- [ ] Used as the authoritative capability query surface
- [ ] Testing
- [ ] Documentation

### Environment - Environment Routines API (Target: 0.2.0)
- [x] Get environment variable
- [x] Set environment variable
- [x] Unset environment variable
- [x] Environment map for copy/restore
- [x] Testing
- [x] Documentation

### File - File Abstraction API (Target: 0.3.0, required by 1.0.0)
- [ ] File object model (path, name, extension)
- [ ] Common operations (append path, join, split, parent)
- [ ] Name/stem/extension helpers
- [ ] Normalize and compare behavior contract
- [ ] Used by Filesystem and Process APIs
- [ ] Testing
- [ ] Documentation

### Filesystem - Filesystem API (Target: 0.3.0, required by 1.0.0)
- [ ] Path operations via File abstraction
- [ ] File operations (exists, stat, read/write, copy, move, remove)
- [ ] Directory operations (create, list, iterate, remove)
- [ ] Error integration with unified error model
- [ ] Cross-platform behavior contract defined
- [ ] Testing
- [ ] Documentation

### Process - Process Launching API (Target: 0.3.0, required by 1.0.0)
- [ ] Process spawn/start
- [ ] Exit code and wait handling
- [ ] Environment controls (integrates with Environment)
- [ ] Working directory controls (via File)
- [ ] Stdout/stderr capture and redirection
- [ ] Executable/redirection path handling via File
- [ ] Error integration with unified error model
- [ ] Testing
- [ ] Documentation

### Argument Parsing - Argument Parsing API (Target: 0.4.0, required by 1.0.0)
- [ ] Option/flag declaration model
- [ ] Parse flow and validation
- [ ] Help/usage generation
- [ ] Typed value conversion and defaults
- [ ] Error integration with unified error model
- [ ] Testing
- [ ] Documentation

### Settings - Settings Helper API (Target: 0.5.0, required by 1.0.0)
- NOTE: May require external libraries for JSON, XML, YAML
- [ ] Key/value settings model
- [ ] Load/save operations
- [ ] Merge precedence model (default/file/env/cli)
- [ ] Environment integration via Environment
- [ ] Validation hooks and schema approach
- [ ] Error integration with unified error model
- [ ] Testing
- [ ] Documentation

### Encoding - Encoding API (Target: 0.6.0 or later)
- NOTE: This is a wrapper around uchardet + iconv
- [ ] Encoding detection via uchardet
- [ ] Conversion via libiconv
- [ ] Our library handle BOM where needed (iconv expect no BOM)
- [ ] Update documenation about licensing, libiconv is LGPL.
- [ ] Must be an optional feature, due to dependencies and licensing.
- [ ] Testing
- [ ] Documentation

### Samples - Samples (Target: 0.3.0+)
- [ ] Minimal consumer project sample
- [ ] Feature-focused samples (selected components, not full surface)
- [ ] Build and run instructions

### Build - Build (Target: 1.0.0)
- [ ] CMake feature options and defaults reviewed
- [ ] Compiler support matrix validated (MSVC/GCC/Clang)
- [ ] Cross-platform behavior verified (Windows/Linux)
- [ ] ASAN integration
- [ ] UBSAN integration
- [ ] Install/export/package validation
- [ ] Release archive includes Doxygen and changelog

### Documentation - Documentation (non-Doxygen) (Target: 1.0.0)
- [ ] Add `doc/markdown/GettingStarted.md`
- [ ] Add `doc/markdown/ErrorHandling.md`
- [ ] Add `doc/markdown/BuildAndInstall.md`
- [ ] Add `doc/markdown/Limitations.md`
- [ ] Update `README.md` with 1.0 scope and support matrix
- [ ] Add first complete 1.0.0 entry in `CHANGELOG.md`

### Release - Release 1.0.0
- [ ] Create `RELEASE.md` with the exact release procedure
- [ ] Define API/ABI compatibility
