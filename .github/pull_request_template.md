## Summary

<!-- What does this change do, and why? -->

## Checklist

- [ ] Spec: the design sections or `docs/spec/` documents this change follows
      are listed here. A change that departs from the spec was approved as a
      spec change first.
- [ ] Tests: new behavior has new tests; `ctest --preset dev` passes.
- [ ] PerfCount: `ctest --preset dev -L PerfCount` passes. Any new counter
      or test is registered in `tests/perf_counts/expected.json`.
- [ ] Expected values: this PR does **not** change, remove, or relax an
      existing expectation, **or** it is an expectation-only PR (no `src/`
      changes) that gives the reason and the IEEE 1800 section number.
- [ ] `Assisted-by: <tool>:<model and version>` is on every commit an AI
      tool helped with.
- [ ] `Signed-off-by` (added by a human, matching the author) is on every
      commit.
