# Unicode implementation

The unicode module operates on UTF-8 encoded strings. It is built on
compile-time generated Unicode Character Database (UCD) tables that are
downloaded and processed by `tools/ucparser.py` during the build. No runtime
data files or external dependencies are needed.

## Supported features

- UTF-8 validation, decoding (`ucb_uc_iter_utf8`) and encoding
  (`ucb_uc_encode_codepoint`, `ucb_uc_encode_codepoints`).
- Codepoint counting (`ucb_uc_num_cp`).
- Extended grapheme cluster support per UAX #29 (`ucb_uc_num_char`,
  `ucb_uc_char_index`, `ucb_uc_next_char`), including Hangul, regional
  indicators, emoji ZWJ sequences, emoji modifiers and Indic conjuncts.
- Normalization: NFC, NFD, NFKC and NFKD, with a quick-check fast path and a
  full fallback.
- Full case folding and simple case mapping (`ucb_uc_to_upper`,
  `ucb_uc_to_lower`, `ucb_uc_to_title`, `ucb_uc_casefold`).
- Case-insensitive comparison (`ucb_uc_icomp`).

## Design notes

- UTF-8 uses the same byte count as code units, so byte offsets are code unit
  offsets. All `_byte` parameters refer to validated boundaries.
- Property lookups use a two-stage table plus a compact property record. Only
  codepoints that carry a decomposition, mapping, combiner or grapheme/Indic
  property have extra data.
- The normalization quick check is conservative: it only reports "already
  normalized" when that is guaranteed, otherwise the full algorithm runs.
- Canonical reordering uses a stack window that grows to the heap for long
  segments; canonical segments have no upper bound and are not truncated.

## Conformance testing

The test suite validates against the official Unicode data files:

- `NormalizationTest.txt`: 20,034 test definitions, each exercised through 20
  normalization relationships (input, NFC, NFD, NFKC, NFKD in all four target
  forms).
- `GraphemeBreakTest.txt`: 766 grapheme break tests, also checking
  `ucb_uc_num_char()` and `ucb_uc_char_index()` against the expected boundaries.

Additional unit tests cover UTF-8 validation, encoding/decoding boundaries,
case mapping, normalization forms and the codepoint/grapheme counting helpers.

## Resolution of earlier findings

The following correctness issues found during the review have been fixed:

1. **Out-of-bounds read on non-null-terminated input.** Codepoint iteration no
   longer decodes at `str[len]`; the internal iterator stops at the end of the
   supplied length. `ucb_uc_num_cp`, `ucb_uc_num_char`, `ucb_uc_normalize` and
   the case-mapping functions are now safe for buffers of exactly `len` bytes
   without a trailing null.
2. **`ucb_uc_encode_codepoint()` no longer accepts surrogates.** Values in
   `U+D800..U+DFFF` return `-1`, matching validation.
3. **`ucb_uc_icomp()` handles multi-codepoint folds.** Folded output is buffered,
   so `"straße"` and `"STRASSE"` now compare equal and expansion ordering is
   correct. This also fixes `ucb_str_icomp()`.
4. **Truncated sequences report a precise message.** `ucb_uc_validate()` now
   distinguishes truncation from malformed sequences and reports the expected
   length.

## Remaining limitations and proposals

- **`ucb_uc_char_index(str, len, 0)` returns `len`.** Index 0 is not a valid
  1-based index; this is now documented, but a distinct sentinel or an error
  would be clearer.
- **`ucb_uc_iter_utf8()` has no end check.** It decodes a null byte as U+0000,
  which is indistinguishable from the terminator of a null-terminated string.
  Iterating strings with embedded nulls requires an explicit length. *Proposed
  addition:* a length-aware iterator or one returning a status.

## Possible additions

- Full case mapping (for example context-sensitive and locale-aware mappings).
- UTF-16 and UTF-32 conversion to/from UTF-8.
- Case-insensitive collation and, when available, system collation.
- A streaming, allocation-free case-insensitive comparison built on the
  corrected `ucb_uc_icomp()`.
- Optional backend selection for collation and segmentation.