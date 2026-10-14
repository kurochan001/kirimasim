# PerfCount: event-count regression tests

`expected.json` records, for each listed test file, the counters that
`haremasim --stats=json parse <file>` reports. The `PerfCount` CTest label
(`ctest --preset dev -L PerfCount`) runs `tools/perfcount_check.py`, which
fails when a counter is **10% or more above** its expected value, when an
expected counter is missing, or when a run reports a counter that
`expected.json` does not list.

Counts are used instead of wall-clock time because they do not depend on
machine load or optimization level, so they are stable on a busy laptop and
in CI.

## Updating the expected values

1. Build and run the test to see the new counts:

   ```sh
   cmake --build --preset dev
   build/dev/src/driver/haremasim --stats=json parse tests/suite/<path>.sv
   ```

2. Edit `expected.json` by hand. Keys are paths relative to the repository
   root; values map counter names to counts.
3. Commit with a message that says **why** the counts changed (for example,
   which change made a counter go up and why that is acceptable, or that a
   new test or counter is being registered).

Rules (see CONTRIBUTING.md):

- Adding a new test or a new counter to `expected.json` may go in the same
  pull request as the implementation change.
- Changing or removing an **existing** entry, including raising a budget,
  must be a separate pull request. `tools/check_expected_change.py` enforces
  this in CI.
- When counts go down, lower the expected values in a separate pull request
  so the improvement is locked in.
