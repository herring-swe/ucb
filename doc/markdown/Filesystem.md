# Filesystem

The filesystem API is split into three headers:

- `ucb/fs.h` — the main surface: predicates, error-aware queries and
  modification (`ucb_fs_*`).
- `ucb/fs_ex.h` — optional, opt-in additions following the `_ex.h` convention.
- `ucb/dir.h` — directory iteration (`ucb_dir`), an opaque heap-allocated
  iterator that sits beside `fs.h` and reuses its entry kinds.

All path-like inputs are UTF-8 `const char*` values, like `ucb/file.h`,
`ucb/env.h` and `ucb/pipe.h`. You do not need a `ucb_path` to use this module,
but you can pass one through `UCB_CSTR` from `ucb/string_ex.h` when convenient.
`ucb/path.h` itself stays lexical-only and never touches the filesystem.

## Path semantics

Every input path is canonicalized **lexically** before it reaches the operating
system:

- Repeated separators are collapsed.
- A trailing separator is stripped, except for a filesystem root.
- A path containing a `..` component is normalized lexically, resolving `..`
  without crossing the anchor. This is required for safety: leaving `..`
  unresolved would let a guard observe one path while the operating system acts
  on another (for example `"/tmp/.."` names the root). A `.` component with no
  `..` is left as written.

Because canonicalization is lexical, the byte string you pass in is not echoed
back verbatim, and a trailing separator carries no meaning:

- `"/my/path/"` and `"/my/path"` name the same entry.
- A destination is always the **exact** result path. There is no "into" or
  "contents-only" interpretation: `ucb_fs_copy_tree("a", "b/c")` makes `b/c` a
  copy of `a`. To copy into an existing directory, compose the final name first,
  for example with `ucb_path_join_c()`.

An empty path string is invalid for every operation and throws
`UCB_ERROR_INVALID_ARG`. The trivial predicates are the exception: they return
`false` for an empty path. Passing `UCB_NULL` is misuse for every function,
including the predicates, and is caught by `UCB_VERIFY_ARGS`.

### Symbolic link policy

- `ucb_fs_exists()`, `ucb_fs_is_file()` and `ucb_fs_is_dir()` follow a final
  symbolic link; `ucb_fs_is_symlink()` tests the entry itself.
- `ucb_fs_get_kind()` and `ucb_fs_get_stat()` follow a final link by default.
  Pass `UCB_FS_NOFOLLOW` to classify the link itself.
- Intermediate links are always resolved by the operating system; the follow
  flag only affects the final path component.
- A broken link therefore yields `exists == false`, `is_symlink == true` and
  `is_file == false`.
- On Windows a symbolic link and a junction are both reparse points and are
  reported as `UCB_FS_KIND_SYMLINK`.

## Entry kinds

`ucb_fs_kind` is deliberately separate from `ucb_file_kind`. A `ucb_file_kind`
describes an open handle, which always resolves to the target of a link, while
`ucb_fs_kind` can describe a link itself:

| `ucb_fs_kind`        | Meaning                                        | Handle equivalent |
|----------------------|------------------------------------------------|-------------------|
| `UCB_FS_KIND_FILE`   | Regular file                                   | `UCB_FILE_KIND_REGULAR` |
| `UCB_FS_KIND_DIR`    | Directory                                      | — |
| `UCB_FS_KIND_SYMLINK`| Symbolic link / Win32 reparse point            | — |
| `UCB_FS_KIND_OTHER`  | FIFO, socket, device or anything unmapped      | pipe/console/device |
| `UCB_FS_KIND_UNKNOWN`| No metadata or an unrecognized entry           | — |

The convenience helpers `ucb_fs_kind_is_file()` and `ucb_fs_kind_is_dir()` are
`static inline` and compile away.

## Error behaviour

The trivial predicates (`ucb_fs_exists()`, `ucb_fs_is_file()`,
`ucb_fs_is_dir()`, `ucb_fs_is_symlink()`) never throw into an error pointer: any
filesystem failure, including a permission error, resolves to `false`, so they
cannot distinguish "missing" from "unreadable". Use the error-aware queries when
that distinction matters. A `UCB_NULL` path is still misuse and aborts via
`UCB_VERIFY_ARGS`, matching the rest of the API. `ucb_fs_cwd()` is trivial in the
same spirit: it returns `UCB_NULL` on failure without setting an error.

The remaining functions throw into the optional `ucb_error** perr`. The table
lists the notable codes; an OS error not listed here is mapped by
`ucb/error.h` and surfaced as its `UCB_ERRSYS_*` equivalent.

| Function | Notable errors |
|----------|----------------|
| `ucb_fs_get_kind` / `ucb_fs_get_stat` / `ucb_fs_get_stat_ns` | `UCB_ERROR_INVALID_ARG` (empty path), `UCB_ERRSYS_ENOENT` (missing) |
| `ucb_fs_mkdir` | `UCB_ERRSYS_EEXIST` (any existing entry) |
| `ucb_fs_mkdir_p` | `UCB_ERRSYS_ENOTDIR` (existing non-directory component) |
| `ucb_fs_remove` | `UCB_ERRSYS_ENOENT` (missing), `UCB_ERRSYS_ENOTEMPTY` (non-empty dir) |
| `ucb_fs_remove_all` | `UCB_ERROR_INVALID_ARG` (root, including one reached through `..`). Missing path is a successful no-op |
| `ucb_fs_copy_file` | `UCB_ERRSYS_ENOENT`, `UCB_ERRSYS_EISDIR` (dir source or dest), `UCB_ERRSYS_EEXIST` (dest exists without `UCB_FS_OVERWRITE`) |
| `ucb_fs_copy_tree` | `UCB_ERROR_INVALID_ARG` (dest equal to or inside src), `UCB_ERRSYS_ENOTDIR` (non-dir source or dest), `UCB_ERRSYS_EEXIST` (dest dir exists without `UCB_FS_OVERWRITE`) |
| `ucb_fs_move` | `UCB_ERRSYS_ENOENT`, `UCB_ERRSYS_EEXIST` (dest is a directory, even with `UCB_FS_OVERWRITE`), `UCB_ERRSYS_ENOTDIR` (dir onto file) |
| `ucb_fs_chdir` | OS errors only |

## Copy and move contract

`ucb_fs_copy_file()` streams the source with `ucb_file_*` and, on success,
applies the source POSIX mode (where available) and modification time to the
destination. A symbolic-link source is recreated as a link by default and
traversed only with `UCB_FS_FOLLOW`. With `UCB_FS_OVERWRITE` the new contents are
staged in a temporary sibling and renamed over the destination, so a failed copy
leaves the original intact and a path copied onto itself is safe.

`ucb_fs_copy_tree()` creates missing destination parents, walks the source with
`ucb_dir`, and recreates links by default. Directory metadata is applied
post-order after the contents are copied. A destination equal to or inside the
source is refused with `UCB_ERROR_INVALID_ARG`, so a copy cannot recurse into
itself.

`ucb_fs_move()` first attempts an atomic rename. When the source and destination
are on different devices the rename reports `UCB_ERRSYS_EXDEV` and the move
falls back to a copy followed by a delete. Only the same-device rename is
atomic; if the copy succeeds but the delete fails, the destination is retained,
the move reports failure and the operation is documented as non-atomic.

`UCB_FS_OVERWRITE` replaces an existing file destination. Copying onto an
existing directory always fails, even with `UCB_FS_OVERWRITE` for
`ucb_fs_copy_file()`; use `ucb_fs_copy_tree()` with `UCB_FS_OVERWRITE` to merge
into an existing directory.

## Directory iteration

`ucb_dir` is an opaque iterator. `ucb_dir_open()` canonicalizes its path the same
way `fs.h` does and skips `.` and `..` unless `UCB_DIR_INCLUDE_DOT` is set.
Each `ucb_dir_next()` fills a `ucb_dir_entry` whose `name` points to a borrowed
buffer that is valid until the next call or a close. It returns `false` both at
the end of the directory and on error; an error is signalled by a value thrown
into `perr`, which can be told apart with `UCB_IS_THROWN`.

Iteration order is defined by the operating system and must never be relied on.
`ucb_dir_list()` is a convenience wrapper that collects every name into a new
`ucb_vector_str` of owned `ucb_str` values; free it with
`ucb_vector_str_free_full()`.

## Ownership and threading

- Returned strings, positions and vectors own their memory; release them with
  the matching function (`ucb_str_free`, `ucb_free`, `ucb_vector_str_free_full`).
- `ucb_fs_cwd()` returns a new owned `ucb_str`.
- These functions are not thread-safe with respect to each other when they
  mutate a shared tree. `ucb_fs_chdir()` is process-global and must be treated
  as such.

## Deferred work

The following are intentionally not part of the initial surface and are tracked
in `IDEAS.md`: public symbolic-link creation, a public mode/times setter,
globbing, hard links, filesystem-space queries and a recursive walker callback.
Realpath resolution is also deferred; canonicalization here is lexical only.
