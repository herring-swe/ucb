# Benchmarks

## Unicode Normalization

The normalization suite uses the [Unicode Normalization Test Suite 17.0.0](https://www.unicode.org/Public/17.0.0/ucd/NormalizationTest.txt) from UCD.
Differences between Unicode versions have not been considered but should be negligible.

We iterate the 20,034 cases for each of the source columns (source, NFC, NFD, NFKC, NFKD) and apply each normalization form.
In total 400,680 conversions per benchmark call.

Versions tested:
* UCB 0.4.0 (Unicode 17.0.0)
* utf8proc 2.12.0 (Unicode 18.0.0)
* ICU 
  * Windows: 72.1 (Windows Built-in, Unicode 15.1.0)
  * Fedora 42: 76.1 (Unicode 16.0.0)

All libraries are dynamically linked and tested against C-APIs.

Implementations compared:
- normalization — ucb_uc_normalize (quick-check fast path).
- normalization-without-quickpath — ucb_uc_normalize_full (control, always full algorithm, internal only).
- normalization-utf8proc — utf8proc_map with STABLE + COMPOSE/DECOMPOSE/COMPAT.
- normalization-icu — ICU C API unorm2_normalize with NFD/NFC/NFKD/NFKC instances; ICU is UTF-16, so this row includes the UTF-8→UTF-16→normalize→UTF-16→UTF-8 round trip. Not that fair, but since our philosophy is UTF-8 everywhere, this is what matters.

### Windows

```
Running 4 tests in 1 suites (samples=500, trim=5%)
name                                 mean    median   std dev   calls/s
-----------------------------------------------------------------------
normalization                    27.59 ms  27.56 ms  151.9 us  36.24 /s
normalization-without-quickpath  40.84 ms  40.74 ms  307.3 us  24.49 /s
normalization-utf8proc           34.29 ms  34.11 ms  453.2 us  29.16 /s
normalization-icu                49.74 ms  49.69 ms  460.3 us  20.10 /s
```

## Linux

```
Running 4 tests in 1 suites (samples=500, trim=5%)
name                                 mean    median   std dev   calls/s
-----------------------------------------------------------------------
normalization                    17.20 ms  17.15 ms  338.6 us  58.14 /s
normalization-without-quickpath  25.54 ms  25.51 ms  407.7 us  39.15 /s
normalization-utf8proc           17.94 ms  17.91 ms  287.6 us  55.73 /s
normalization-icu                42.01 ms  41.95 ms  504.3 us  23.80 /s
```

## Verdict

- UCB is faster than ICU on both Windows and Linux on this specific suite. The suite was not specifically chosen to beat anything.
- utf8proc seems penalized in lack of a quick normalization check. The core algorithm is fast (compare against normalization-without-quickpath).
- ICU is penalized since it expects UTF-16.
- This is before any targeted optimizations in the UCB library.

## Test system

| - | Windows | Linux |
| --- | --- | --- |
| OS | Windows 11 26H1 (26300.9711) | Fedora 42 (WSL) |
| Compiler | MSVC 19.44.35217 | GCC 15.2.1 |
| Release flags | /O2 | -O3 |

Machine specs:

```
AMD Ryzen 7 7700X 8-Core Processor
Kingston 32 GB DDR5 at 6000 MHz
```

## Note on sizes

Size comparison may be interesting in embedding.

The UCB library includes more functionality than just the unicode normalization.
ICU includes a lot more functionality than just normalization. The sizes here are only an indication and not investigated in full.

Windows:

| Binary | Size |
| --- | --- |
| ucb.dll | 519 KB |
| utf8proc.dll | 354 KB |
| icu.dll (System32) | 2.67 MB |
| icuuc.dll (System32) | 36 KB |

Note: icuuc.dll only forwards to icu.dll.

Linux:

| Binary | Size |
| --- | --- |
| libucb.so | 642 KB |
| libutf8proc.so | 361 KB |
| libicuuc.so.76 | 2.03 MB |
| libicudata.so.76 | 30.4 MB |
