# String handling

The string type `ucb_str` is a UTF-8 string that keeps track of its own length
and capacity. It is designed around [UTF-8 everywhere](https://utf8everywhere.org/)
and is null-terminated internally so it can be passed to C functions, while
still allowing embedded null characters when an explicit length is used.

## Ownership model

A `ucb_str` is either *owned* or *wrapped*:

- **Owned** (`alloc > 0`): the string owns `data`. The data is freed when the
  string is released or freed.
- **Wrapped** (`alloc == 0`): `data` points to memory owned elsewhere (for
  example a string literal). The data is never freed, and any modifying
  operation first detaches (copies) the data so that the literal is never
  written to.

The helper `ucb_str_is_owned()` reports the current state.

## Construction and lifetime

| Pattern        | Stack                                     | Heap                                  |
|----------------|-------------------------------------------|---------------------------------------|
| Owned copy     | `ucb_str_init(&s, cstr, len)`             | `ucb_str_new(cstr, len)`              |
| Wrapped        | `ucb_str_init_wrap(&s, cstr, len)`        | `ucb_str_new_wrap(cstr, len)`         |
| Empty wrapped  | `ucb_str_make()` / `ucb_str_init_empty()` | `ucb_str_new_empty()`                 |
| Deep copy      | `ucb_str_copy(&dst, &src)`                | `ucb_str_clone(&src)`                 |
| Adopt buffer   | `ucb_str_adopt(&s, data, len, alloc)`     | –                                     |
| Detach/release | `ucb_str_release(&s)`                     | `ucb_str_free(s)`                     |

`ucb_str_adopt()` takes ownership of memory allocated with the UCB allocator.
`ucb_str_abandon()` releases ownership back to the caller without freeing and
zeroes the `ucb_str`.

A stack `ucb_str` must always be initialized before use and released when done.
`ucb_str_release()` on a wrapped string only zeroes the struct; on an owned
string it also frees the data.

## Length semantics

`ucb_str_len()` returns the number of **bytes**, which matches `strlen` for
strings without embedded null characters. It is not the number of visible
characters.

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

`ucb_str_wrap()` and `ucb_str_adopt()` are not guarded against being handed a
pointer into the same string's allocation; do not re-wrap or re-adopt a string's
own buffer.

## Resolution of earlier findings

The issues found during the string/unicode review have been fixed:

- Self-aliasing no longer corrupts data (see above).
- `ucb_str_find()` now returns `UCB_NPOS` for "not found", consistent with the
  rest of the API.
- The undeclared `ucb_str_size()` duplicate of `ucb_str_len()` was removed.
- The `ucb_str_substr()`/`ucb_str_substr_wrapped()` documentation now correctly
  describes byte offsets and the ownership of the result.

## Proposed additions

- `ucb_str_num_cp()` as a convenience wrapper around `ucb_uc_num_cp()`, mirroring
  `ucb_str_num_char()`.
- `ucb_str_find_c()` / `ucb_str_startswith_c()` / `ucb_str_endswith_c()` for
  C-string operands.
- A grapheme-aware substring helper (see above).
- Aliasing-safe variants or documented preconditions for the mutating APIs.