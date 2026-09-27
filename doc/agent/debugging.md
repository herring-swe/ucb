# Debugging

- Tests are checked against memory leaks (using ucb memory functions).
- To get backtraces for leaks, temporarily enable the CMake option
  `UCB_MEMTRACK_BACKTRACE=ON`. This has a significant runtime performance impact.
