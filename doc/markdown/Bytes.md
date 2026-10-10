# Bytes

`ucb/units/bytes.h` converts byte counts to human-readable strings and parses
CLI-style size strings back into raw byte counts. It is a standalone header with
no dependency on the filesystem or the argument parser, so other modules can
build on it (for example a future argparse size type).

It sits under `ucb/units/` so that other quantity converters (durations and so
on) can join the same family without sharing a prefix.

```c
#include <ucb/units/bytes.h>
```

The raw count type is `ucb_bytes` (a `uint64_t`), defined in `ucb/types.h`.

## Unit systems

SI (decimal, powers of 1000) and IEC (binary, powers of 1024) share a single
`ucb_bytes_unit` enum; the suffixes keep the family unambiguous.

| Unit | SI suffix | SI factor | IEC suffix | IEC factor |
|------|-----------|-----------|------------|------------|
| byte | `B`       | 1         | `B`        | 1          |
| kibi | `kB`      | 1000      | `KiB`      | 1024       |
| mebi | `MB`      | 1000^2    | `MiB`      | 1024^2     |
| gibi | `GB`      | 1000^3    | `GiB`      | 1024^3     |
| tebi | `TB`      | 1000^4    | `TiB`      | 1024^4     |
| pebi | `PB`      | 1000^5    | `PiB`      | 1024^5     |
| exbi | `EB`      | 1000^6    | `EiB`      | 1024^6     |

Note the lowercase `k`, which is the correct SI symbol.

The enum also carries two auto sentinels, `UCB_BYTES_AUTO_SI` and
`UCB_BYTES_AUTO_IEC`, which pick the largest unit of their own family that fits.

## Formatting

| Function | Purpose |
|----------|---------|
| `ucb_bytes_convert(bytes, unit)` | Scale a count to a unit, or auto-select. Returns `ucb_bytes_value`. |
| `ucb_bytes_to_str(value, precision)` | Render a `ucb_bytes_value` as a string. |

`ucb_bytes_value` is `{ double value; ucb_bytes_unit unit; }`. The unit it holds
is always concrete, never a sentinel.

```c
ucb_bytes_value v = ucb_bytes_convert(1536, UCB_BYTES_AUTO_IEC); /* 1.5 KiB */
ucb_str* s = ucb_bytes_to_str(v, 0);                              /* "2 KiB" */
ucb_str_free(s);

s = ucb_bytes_to_str(ucb_bytes_convert(2 * 1000 * 1000, UCB_BYTES_MB), 2); /* "2.00 MB" */
ucb_str_free(s);
```

Formatting always emits a single space between the number and the suffix, and the
byte suffix is always `B` (never empty, `Bi`, `iB` or `bytes`).

`precision` is a required argument:

- `0` prints an integer,
- a positive value prints that many decimals,
- a negative value is a programming error and aborts via `UCB_VERIFY`.

Auto selection is precision-independent: it chooses the largest unit of the
requested family whose factor is `<= bytes`. A caller that needs the exact
fractional value passes a precision of at least `1`; with `precision == 0`,
`ucb_bytes_convert(1536, UCB_BYTES_AUTO_IEC)` renders `"2 KiB"`.

## Parsing

`ucb_bytes_parse` is CLI-friendly:

- leading and trailing spaces/tabs are trimmed;
- a decimal fraction is accepted (`1.5M`);
- whitespace between the number and the unit is accepted (`2 kb`);
- the unit is case-insensitive;
- exponent notation is not accepted (`e`/`E` is the exa prefix).

The suffix rules:

| Input | Meaning |
|-------|---------|
| (none) | bytes |
| `B` | bytes |
| `K`, `M`, `G`, `T`, `P`, `E` | binary (e.g. `M` = MiB) |
| `Ki`, `Mi`, … | binary |
| `KiB`, `MiB`, … | binary |
| `KB`, `MB`, … | decimal (e.g. `MB` = MB) |

So a bare prefix letter is binary while the same letter followed by `B` is
decimal: `M` is MiB, `MB` is MB.

On success the result carries the rounded byte count and the parsed unit:

```c
ucb_bytes_parsed r;
ucb_error* err = UCB_NULL;
if (ucb_bytes_parse("1.5GiB", &r, &err))
{
    /* r.bytes == 1610612736, r.unit == UCB_BYTES_GIB */
}
```

## Error behaviour

`ucb_bytes_parse` is non-fatal and error-aware, matching the rest of the library:

| Condition | Error |
|-----------|-------|
| malformed input, no digits, unknown or partial suffix | `UCB_ERROR_INVALID_ARG` |
| the result does not fit a `ucb_bytes` | `UCB_ERROR_OUT_OF_BOUNDS` |

A fractional result is rounded half away from zero.

## Thread safety

These functions are pure and thread safe.

## Out of scope

`argparse` integration (a size option type that calls `ucb_bytes_parse`), other
quantity modules (durations, percentages) and a no-allocation `snprintf` output
variant are intentionally not part of this module yet.
