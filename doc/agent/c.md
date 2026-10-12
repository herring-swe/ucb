# C conventions

- Own functions (prefixed with `ucb_`) *should* be used instead of standard library
  functions when possible and where it makes sense.
- `ucb_malloc`/`ucb_calloc`/`ucb_realloc` must be paired with `ucb_free`. These
  functions track memory allocations and report leaks.
- Run as final step to apply formatting: `python tools/format.py --type c --fix`

## Header and API conventions

- Public headers live in `include/ucb/` and must be registered in
  `ucb_target_public_headers` in `src/CMakeLists.txt`.
- Internal declarations live in `src/ucb/*_private.h` and are never installed.
  They carry `UCB_API` only so tests can link them; tests and benchmarks reach
  them through the `src/` include directory
  (`target_include_directories(... PRIVATE ${CMAKE_SOURCE_DIR}/src)`).
- `_ex.h` is the convention for **optional, extended API** packaged as a
  companion to a main header. The companion header includes the main header.

## API input and naming conventions

### Prefer the typed type as input

String- and path-domain functions take the typed type (`ucb_str`, `ucb_path`,
...) by default, as both input and output. This is the API we drive towards:

```c
bool ucb_fs_get_stat(const ucb_str* path, unsigned flags, ucb_fs_stat* out, ucb_error** perr);
```

A typed value already carries valid UTF-8, no embedded null character, a known
length and clear ownership. A typed API therefore **must not re-validate or
assert** its input validity: validation and length rules live once, at the
C-string boundary. This is the main reason to prefer the typed form.

`const char*` is a **boundary** type, not the default input. It is allowed only
for:

- the codec/basis layer that `ucb_str` is built on: `unicode.h`, `cstring.h`,
  `buffer.h`;
- constructors and entry points that establish a typed value: `*_new`,
  `*_init`, `*_assign`, plus their `_c`/`_cstr` forms;
- genuine representation conversions (`_from_*` / `_to_*`);
- OS / environment / CLI interfaces where a C string is the native exchange
  format (`fs.h`, `file.h`, `dir.h`, `env.h`, `process.h`, `argparse.h`). Even
  there, prefer a typed primary with a `_c` convenience variant.

Some of those modules still take `const char*` as their primary signature.
Migrating them to a typed primary plus a `_c` variant is the direction of this
convention, not a completed change.

### Suffixes

| Suffix          | Meaning                                                              | Examples |
|-----------------|----------------------------------------------------------------------|----------|
| *(none)*        | Primary. Takes and/or returns the typed type.                        | `ucb_fs_get_stat`, `ucb_str_append`, `ucb_path_dir`, `ucb_path_join` |
| `_c`            | Same operation with a single `const char*` argument, length from `strlen`. Also the literal-constructor form. | `ucb_fs_get_stat_c`, `ucb_str_append_c`, `ucb_path_set_dir_c`, `ucb_str_new_c`, `ucb_path_new_c`, `ucb_str_init_c`, `ucb_str_assign_c` |
| `_cstr`         | Same operation with an explicit `(const char*, size_t)` argument; as an accessor, returns the underlying `const char*`. | `ucb_str_append_cstr`, `ucb_str_insert_cstr`, `ucb_str_cstr`, `ucb_path_cstr` |
| `_from_X` / `_to_X` | Conversion between different representations or types `X`.       | `ucb_str_from_wchar` / `_to_wchar`, `ucb_cstr_from_wchar` / `_to_wchar`, `ucb_file_from_fd`, `ucb_file_from_handle`, `ucb_envmap_init_from_current`, `ucb_uc_norm_form_from_str` |

Rules:

- A `_c` / `_cstr` variant is a **thin wrapper** over the typed primary. It must
  not change the operation, the ownership, or the return category. Keep the
  logic and the input validation in the primary.
- A C string is the underlying representation of `ucb_str` / `ucb_path`, not a
  separate type, so there is no `_from_c` / `_to_c`. Wrap with `*_new_c` /
  `*_init_c`; unwrap with `*_cstr()`.
- Do not add static `_c` helpers that parse a literal and return a component or
  a new object (such as the `ucb_path_dir_c` family). Parsing a C string is
  `*_new_c` / `*_init_c` followed by the typed method, so construction and
  ownership stay in one place. Existing helpers of this shape are to be retired
  in favor of that pattern.
- `_from_*` / `_to_*` is reserved for a real representation change (UTF-16, an
  OS handle or file descriptor, an enum). Same representation, different view →
  no conversion verb.
- A `_c` variant always returns owned data because it has no receiver to borrow
  from. A typed method may borrow (`const ucb_str* ucb_path_dir(const ucb_path*)`)
  or return a new owned object (`ucb_path* ucb_path_parent(const ucb_path*)`).
  This difference is expected, not an inconsistency.
