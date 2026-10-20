# Contributing to kirimasim

[日本語](CONTRIBUTING.ja.md) · This English text is the main version; the Japanese version follows it, and where they differ, this text holds.

Thank you for your interest. kirimasim is in early development: phase P1,
the first execution tier, is in progress. The README gives the status.

## What we accept for now

For now, kirimasim accepts **specification proposals and issues only**.
Pull requests are turned off for this repository for now; collaborators
push to `main`. When the project starts accepting more, this file and the
README will say so.

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
  section number in the PR. While pull requests are off, it is its own
  commit, with the reason and the section number in the commit message:
  CI checks each commit pushed to `main` on its own.
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
- No global state: state belongs to the Context (`kernel::Context`). In
  `src/`, a variable of static or thread storage duration is `constexpr` or
  `constinit const`; there are no singletons. The rules and their few
  exceptions are in [RUNTIME_ABI.md 9](docs/spec/RUNTIME_ABI.md).
  clang-tidy (with `--experimental-custom-checks`) and
  `tools/check_global_state.py` check them; a new exception needs a human's
  approval and a line in `tools/global_state_allowlist.txt`.
- Outside `src/driver/`, code does not read environment variables, the
  locale, the time of day or random seeds from outside, does not end the
  process, and does not write to the process's standard output or standard
  error; no code calls `abort`, `std::terminate` or `assert`.
  `tools/check_banned.py` checks this.
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
python3 tools/check_global_state.py
```

For release builds, also run `python3 tools/check_release_flags.py` after
`cmake --preset release`. The `asan` and `ubsan` presets run the same tests
with sanitizers; `asan` builds slang without mimalloc, whose allocator the
address sanitizer cannot see into. The `tsan` preset builds and runs only
the tests of state and Contexts (`tests/context/`, RUNTIME_ABI.md 9.8)
under the thread sanitizer, also without mimalloc.

kirimasim's own code builds with `-Wall -Wextra -Wpedantic`. Configure with
`-DKIRIMASIM_WERROR=ON` to make the warnings errors, as `tools/local_ci.py`
and CI's clang builds do.

To run every check that CI runs (all four presets, the release flag check,
the banned-feature check, the global-state check, clang-format, clang-tidy,
and the check that each commit not pushed yet keeps existing expectations)
in one go:

```sh
python3 tools/local_ci.py
```

CI runs on Linux and macOS after a push. Run `tools/local_ci.py` before
pushing, to find failures first; with a pull request, paste its summary
into it.

## Changelog

[CHANGELOG.md](CHANGELOG.md) is for people who use kirimasim. It lists what
changed as they see it; the commit history has the details.

- Record, in the Keep a Changelog sections: **Added** (constructs, system
  tasks and options that now work), **Changed** (the same input now gives
  different output, exit code or messages, including changes to a rule of
  the specifications, to the ABI version, and visible changes in speed or
  memory), **Fixed** (results that were wrong), **Removed** and
  **Deprecated**, and **Security** (fixes of reports made through
  SECURITY.md).
- Do not record new tests, CI, internal restructuring, changes to
  documents alone, or changes to PerfCount counts.
- One line per entry, in the user's words, citing the section of the
  specification it rests on, for example `(VALUE_SEMANTICS.md 3.2)`. The
  file is in English only.
- Add the entry under `[Unreleased]` in the same commit as the change.
  Reviewers check it; no tool enforces it, because most changes under
  `src/` are internal.
- The first release is planned as 0.1.0 at the end of phase P1. Then
  `[Unreleased]` becomes `[0.1.0] - <date>`, and comparison links go at
  the end of the file. While the version is 0.x, a new minor version may
  break compatibility, as Semantic Versioning allows.

## Pull requests

Pull requests are turned off for now: collaborators push to `main`, and
CI checks every pushed commit. When pull requests are turned on, fill in
the pull request template. Keep one purpose per commit and write
commit messages in English with a summary line of 72 characters or less.

By participating you agree to follow the [Code of Conduct](CODE_OF_CONDUCT.md).
