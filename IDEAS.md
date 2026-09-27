# Ideas

Deferred or proposed additions that are not yet part of the API. Nothing here is
committed work; items move into `TODO.md` when they are scheduled.

## Filesystem

- Public symbolic-link creation (`ucb_fs_symlink`). Link creation currently
  exists only as an internal copy detail and, on Windows, needs either developer
  mode or a privileged process.
- Public mode/times setters (`ucb_fs_set_mode`, `ucb_fs_set_mtime`). Copying
  already applies metadata internally.
- Realpath / symlink-resolving canonicalization, as a distinct opt-in function.
  `fs.h` canonicalization is deliberately lexical only.
- Glob and wildcard matching (`ucb_fs_glob`), layered on `ucb_dir`.
- Hard link creation and detection.
- Filesystem-space queries (total/free/available).
- A recursive walker with a callback, for callers who do not want to drive
  `ucb_dir` themselves.
- A `ucb_dir_entry` statistics variant that already carries a full
  `ucb_fs_stat`, avoiding a second query per entry.
- Windows junction creation and reparse-point helpers beyond read/create symlink.

## Filesystem: TOCTOU ("secure" opens)

Time-of-check/time-of-use races matter when a privileged principal resolves a
path in a directory a less-privileged principal can write, then acts on it. This
is mostly a deployment concern, not a normal application one: it only bites a
root- or service-running process working on a shared or attacker-writable tree.
A team that truly needs this hardening would likely reach for Rust (`cap-std`)
today instead of choosing UCB. Treat everything below as a possible opt-in
"secure open" mode: low priority, not a default.

### The pattern it fixes

```c
if (ucb_fs_is_file("config.json"))     /* resolves the path once */
{
    /* a concurrent writer can swap the name here */
    ucb_file_open("config.json", ...); /* resolves it again */
}
```

The check and the open resolve independently, so the check says nothing about
what the open returns. UCB's simple predicates also always follow a final
symlink and take no flags, so they cannot be used as a safe gate.

### Highlights of the approach

- Push constraints into the open so the kernel enforces them atomically, and
  never pre-check what a flag can express: `O_EXCL` (must not exist),
  `O_NOFOLLOW` (not a symlink), `O_DIRECTORY` / `FILE_FLAG_BACKUP_SEMANTICS`
  (is a directory). Create/write paths need no separate existence check at all.
- The one gap is "is a regular file", which POSIX has no flag for: open with
  least-privilege access, then classify the pinned object via a new
  `ucb_file_get_stat` (`fstat` / `GetFileInformationByHandle`) and reject.
- For a sequence of operations in an untrusted directory, pin the directory and
  act relative to it: `ucb_dir_open` -> `ucb_dir_get_stat` / `ucb_dir_open_file`
  (`openat`), never re-resolving parents.
- For a delayed decision (validate during a listing, use later by name), carry
  `dev`+`ino` identity and compare, or better, hold the object.

### Feasibility

- POSIX: full-fidelity. `fstat`, the `*at` family and a directory fd cover it;
  whole-tree safety needs a component-by-component walk with
  `O_NOFOLLOW | O_DIRECTORY`, or `openat2` with
  `RESOLVE_BENEATH | RESOLVE_NO_SYMLINKS` on Linux.
- Windows: no public relative open, but implementable. `NtCreateFile` with
  `OBJECT_ATTRIBUTES.RootDirectory` is the directory-anchored open (used by
  Rust's `cap-std` and Go's `os.Root`; load from `ntdll` via `GetProcAddress`,
  map `NTSTATUS`). `ReOpenFile` gives "query lightly, then open for I/O on the
  same object" natively. A joined `CreateFileW` is the best-effort fallback.
- Not one portable model: POSIX cannot upgrade a descriptor, Windows has no
  public `openat`. Any guarantee is per-platform and must be documented as such.

### Examples it could solve

- Creating a temp or lock file in a shared directory without a symlink swap
  (already covered by `UCB_FILE_EXCL`).
- Extracting an untrusted archive into a directory another user can write.
- A root/service process reading a config or secret by path from a
  less-privileged tree.
- Validate-then-use across a directory listing, where entries are acted on later
  by name.

### Candidate additions (only if pursued)

- `ucb_file_get_stat` (`fstat` / `GetFileInformationByHandle`) and
  `UCB_FILE_NOFOLLOW`.
- `ucb_dir_get_stat` / `ucb_dir_open_file` (`fstatat` / `openat`) plus
  `ucb_dir_get_fd`.
- Identity in `ucb_fs_stat` (`dev`+`ino`) and `ucb_fs_stat_same_object`.
- By-handle mutation: `ucb_file_fchmod` / `ucb_file_futimens`.
