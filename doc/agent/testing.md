# Testing

- Unit tests use doctest, live in `tests/` and are the `ucb_tests` target.
  Benchmarks live in `tests/bench/` (`ucb_benchmarks`) and are **not** registered
  with CTest; run them manually.
- Doctest test case names follow one nomenclature:
  - Lowercase.
  - Naming: `<subject> - <aspect>`.
  - Subject: short name of the subject. Corresponds to the test file name.
  - Aspect: short description of the test case. A subject with a single test case
    uses `general`.
- Non-trivial code shared by the tests and the benchmarks belongs in a shared helper 
  under `tests/` and is compiled into both targets instead of being duplicated.
