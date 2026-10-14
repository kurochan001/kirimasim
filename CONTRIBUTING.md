# Contributing to haremasim

Thank you for your interest. haremasim is in early development (P0), so
the most useful contributions right now are reviews of the specifications
and new tests.

## Developer Certificate of Origin (DCO)

Every commit must carry a `Signed-off-by:` line that matches the commit
author, certifying the [Developer Certificate of Origin](https://developercertificate.org/):

```sh
git commit -s
```

**Only humans add `Signed-off-by`.** By signing off you state that you have
read and understood the change and take responsibility for it. AI tools must
never add a `Signed-off-by` line, even when they write the commit. CI
(`tools/check_dco.py`) checks every non-merge commit in a pull request.

## Use of AI tools

AI tools are welcome, and most of haremasim's code is AI-generated. If an AI
tool helped with a commit, say so with an `Assisted-by` trailer:

```text
Assisted-by: <tool>:<model and version>
```

Do not use `Co-authored-by` for AI tools. You remain responsible for every
line you submit: review it, test it, and be ready to explain it. See
[docs/spec/AI_POLICY.md](docs/spec/AI_POLICY.md).

## Changing expected results

Test expectations are the project's definition of correct behavior, so
they are protected:

- A change that modifies, removes, or relaxes an **existing** expectation
  (`tests/perf_counts/expected.json` entries, `tests/**/expected*` lines,
  `:expected_stdout:` lines, or the body of an existing test-suite file
  under `tests/suite/`) must be its **own pull request**, separate from
  implementation changes under `src/`. The same applies to deleting or
  renaming an existing test file. Explain the reason and cite the IEEE 1800
  section number in the PR.
- Unit tests (`tests/unit/`) may be edited together with the code they
  call, for example when a function is renamed. CI lists every changed or
  removed `EXPECT_*`/`ASSERT_*` line as a warning, and reviewers check that
  no assertion was weakened.
- Adding new tests and their expectations in the same PR as the
  implementation is fine. Reviewers check where the expectation comes from.
- CI enforces this with `tools/check_expected_change.py`.

## Rules for the code

- C++23 for haremasim itself; the kernel's public headers
  (`src/kernel/include/`) must compile as C++20.
- In `src/kernel/` and `src/vm/`, do not use `std::shared_ptr`,
  `dynamic_cast`, `std::function`, `getenv`, or `typeid`. `std::unordered_map`
  and `std::unordered_set` need a `// haremasim: unordered-ok <reason>`
  comment on the same line or the line above, and their iteration order must
  never affect results. `tools/check_banned.py` checks this.
- No mutable global or static state and no singletons; state belongs to a
  context object.
- Libraries depend on each other only in the directions listed in
  [docs/ARCHITECTURE.md](docs/ARCHITECTURE.md).
- Do not copy text, figures, or example code from the IEEE 1800 standard.
  Cite section numbers and describe behavior in your own words.
- Do not read or copy the source code of other simulators (for example,
  Icarus Verilog or Verilator). Learn their behavior from their output only.
- Format with `clang-format` (the repository's `.clang-format`).

## Building and testing

```sh
cmake --preset dev
cmake --build --preset dev
ctest --preset dev                 # unit tests, test suite, PerfCount
ctest --preset dev -L PerfCount    # event-count regression check only
python3 tools/check_banned.py
```

For release builds, also run `python3 tools/check_release_flags.py` after
`cmake --preset release`. The `asan` and `ubsan` presets run the same tests
with sanitizers.

## Pull requests

Fill in the pull request template. Keep one purpose per commit and write
commit messages in English with a summary line of 72 characters or less.

By participating you agree to follow the [Code of Conduct](CODE_OF_CONDUCT.md).
