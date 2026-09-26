# String handling

The string type `ucb_str` is a UTF-8 string that keeps track of its own length
and capacity. It is designed around [UTF-8 everywhere](https://utf8everywhere.org/)
and is always null-terminated with no embedded null characters, so it can be
passed directly to C functions.

## Ownership model

A `ucb_str` is always owned, with one special case:

- **Owned** (`alloc > 0`): the string owns `data`. The data is freed when the
  string is released or freed.
- **Interned empty** (`alloc == 0`): `data` is the shared `""` literal, `size`
  is 0. It requires no deallocation.

There is no borrowed/wrapped state. Every explicit-length constructor and
mutator rejects an embedded null character as a user error.

Use `UCB_CSTR(x)` to coerce a C string, a `ucb_str` pointer, or (in C++) a type
exposing `c_str()` such as `std::string` to a `const char*`. It is part of the
optional extended API; include `<ucb/string_ex.h>` instead of `<ucb/string.h>`
when you need it (see the `_ex.h` header convention).

## Construction and lifetime

| Pattern        | Stack                                     | Heap                                  |
|----------------|-------------------------------------------|---------------------------------------|
| Owned copy     | `ucb_str_init(&s, cstr, len)`             | `ucb_str_new(cstr, len)`              |
| Empty          | `ucb_str_make()` / `ucb_str_init_empty()` | `ucb_str_new_empty()`                 |
| Deep copy      | `ucb_str_copy(&dst, &src)`                | `ucb_str_clone(&src)`                 |
| Adopt buffer   | `ucb_str_adopt(&s, data, len, alloc)`     | –                                     |
| Release/free   | `ucb_str_release(&s)`                     | `ucb_str_free(s)`                     |

`ucb_str_adopt()` takes ownership of memory allocated with the UCB allocator.
The allocation must be null-terminated at `data[len]` and at least `len + 1`
bytes. `ucb_str_abandon()` releases ownership back to the caller without freeing
and zeroes the `ucb_str`; an interned empty returns `false` and must not be
freed.

A stack `ucb_str` must always be initialized before use and released when done.
`ucb_str_release()` on an interned empty string only zeroes the struct; on an
owned string it also frees the data.

## Length semantics

`ucb_str_len()` returns the number of **bytes**, which always matches
`strlen(ucb_str_cstr(s))`. It is not the number of visible characters.

- `ucb_str_num_char()` returns the number of *perceived characters* (extended
  grapheme clusters, UAX #29). Use this for cursor movement, truncation and
  column widths.
- `ucb_str_len()` / `ucb_str_cstr()` operate on bytes and are suitable for
  serialization and byte-oriented algorithms.

Because grapheme clusters can consist of several codepoints, `ucb_str_num_char()`
can be smaller than the codepoint count. Use `ucb_uc_num_cp()` on the underlying
C string for the codepoint count.

## Comparison and normalization contract

Strings are compared byte-wise. For meaningful results the inputs should be in
the same normalization form. Use `ucb_str_normalize()` before comparing when the
input form is unknown.

`ucb_str_icomp()` compares case-insensitively using full case folding. When the
same strings are compared repeatedly, case fold them once with
`ucb_str_casefold()` and use the case-sensitive `ucb_str_comp()` instead.

`ucb_str_find()` returns a byte offset (not a character index) and uses
`SIZE_MAX` for "not found". Operations that take a character index, such as
`ucb_str_insert()`, convert it internally with `ucb_uc_char_index()`.

## Thread safety

`ucb_str` is not thread safe. It is up to the caller to synchronize access to a
string shared between threads.

## Aliasing behaviour

Mutating operations take a temporary copy when the source aliases the
destination, so the following are well defined:

- `ucb_str_copy(&s, &s)`
- `ucb_str_assign(&s, s.data, ...)` (including substrings of `s`)
- `ucb_str_append(&s, &s)` and `ucb_str_append_cstr(&s, s.data, ...)`
- `ucb_str_insert(&s, index, &s)` and `ucb_str_insert_cstr(&s, index, s.data, ...)`

`ucb_str_adopt()` is not guarded against being handed a pointer into the same
string's allocation; do not re-adopt a string's own buffer.

## Resolution of earlier findings

The issues found during the string/unicode review have been fixed:

- Self-aliasing no longer corrupts data (see above).
- `ucb_str_find()` now returns `UCB_NPOS` for "not found", consistent with the
  rest of the API.
- The undeclared `ucb_str_size()` duplicate of `ucb_str_len()` was removed.
- The `ucb_str_substr()` documentation now correctly describes byte offsets and
  the ownership of the result.
- Borrowed/wrapped strings were removed; `ucb_str` is owned-only, always
  null-terminated and free of embedded nulls. `ucb_str_detach()` and
  `ucb_str_is_owned()` no longer exist.

## Proposed additions

- `ucb_str_num_cp()` as a convenience wrapper around `ucb_uc_num_cp()`, mirroring
  `ucb_str_num_char()`.
- `ucb_str_find_c()` / `ucb_str_startswith_c()` / `ucb_str_endswith_c()` for
  C-string operands.
- A grapheme-aware substring helper (see above).
- Aliasing-safe variants or documented preconditions for the mutating APIs.