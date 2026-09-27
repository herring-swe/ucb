# C conventions

- Own functions (prefixed with `ucb_`) *should* be used instead of standard library
  functions when possible and where it makes sense.
- `ucb_malloc`/`ucb_calloc`/`ucb_realloc` must be paired with `ucb_free`. These
  functions track memory allocations and report leaks.
- Run as final step to apply formatting: `python tools/format.py --type c --fix`

## Header and API conventions

- Public headers live in `include/ucb/` and must be registered in
  `ucb_target_public_headers` in `src/CMakeLists.txt`.
- `_ex.h` is the convention for **optional, extended API** packaged as a
  companion to a main header. The companion header includes the main header.
