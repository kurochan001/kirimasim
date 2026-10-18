# Contributing to kirimasim

[日本語](CONTRIBUTING.ja.md) · This English text is the main version; the Japanese version follows it, and where they differ, this text holds.

Thank you for your interest. kirimasim is in early development: phase P1,
the first execution tier, is in progress. The README gives the status.

## What we accept for now

For now, kirimasim accepts **specification proposals and issues only**.
Only collaborators can open pull requests. When the project starts
accepting more, this file and the README will say so.

We accept:

- **Specification proposals**: changes or additions to the documents under
  `docs/spec/`, through the "Specification proposal" issue form.
- **Bug reports**, through the "Bug report" issue form.
- **Reports of wrong expected values** in tests, through the "Expected value
  change" issue form.
- **Discussion** in GitHub Discussions: questions in the Q&A category, and
  ideas that are not yet a concrete proposal in the Ideas category.

We do not accept:

- **Code pull requests** from outside the project.

In issues and discussions, do not paste text, figures, or example code from
the IEEE 1800 standard or code written by others, and do not include
confidential information from your employer. Cite section numbers, describe
things in your own words, and write your own minimal examples.

### When a specification proposal is accepted

kirimasim is implemented from its specifications. When a proposal is
accepted, a maintainer first updates the specification under `docs/spec/`,
and the implementation then follows the updated specification. The
proposal issue is linked from those changes.

The rest of this file describes the rules that collaborators follow when
they change the repository.

## Sign-off

Commits do not need a `Signed-off-by` line for now. The project will
introduce the Developer Certificate of Origin (DCO), or a contributor
license agreement, when it starts accepting code pull requests from
outside.

## Use of AI tools

AI tools are welcome, and most of kirimasim's code is AI-generated. If an AI
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

- C++23 for kirimasim itself; the kernel's public headers
  (`src/kernel/include/`) must compile as C++20.
- In `src/kernel/` and `src/vm/`, do not use `std::shared_ptr`,
  `dynamic_cast`, `std::function`, `getenv`, or `typeid`. `std::unordered_map`
  and `std::unordered_set` need a `// kirimasim: unordered-ok <reason>`
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
with sanitizers; `asan` builds slang without mimalloc, whose allocator the
address sanitizer cannot see into.

To run every check that CI runs (all four presets, the release flag check,
the banned-feature check, clang-format and clang-tidy) in one go:

```sh
python3 tools/local_ci.py
```

CI runs on Linux only, so if you work on macOS, run `tools/local_ci.py`
before pushing and paste its summary into the pull request.

## Pull requests

Pull requests come from collaborators only for now. Fill in the pull
request template. Keep one purpose per commit and write
commit messages in English with a summary line of 72 characters or less.

By participating you agree to follow the [Code of Conduct](CODE_OF_CONDUCT.md).
