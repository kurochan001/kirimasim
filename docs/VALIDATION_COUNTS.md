# Recorded validation counts

[日本語](VALIDATION_COUNTS.ja.md)

Each result below belongs to its recorded date and population; the [counting
FAQ](FAQ.md#why-do-probe-ctest-and-population-counts-differ) explains why the totals
cannot be combined into one coverage percentage.

These are the latest records stored in [counts.json](counts.json), with separate
measurement dates and populations. The original 95-probe result below is dated
2026-10-27; the [current support inventory](P1_SUPPORT.md) and
[188-probe scope record](measurements/2026-11-05/icarus13-star-functions-after.json) describe
later coverage. These datasets do not measure a percentage of language completion.
Repository CTest counts can change independently of external measurements.
See [Validation](VALIDATION.md) for gate rules, versions and reproduction.
The Linux/macOS sv-tests measurements dated 2026-11-01 UTC come from
[Nightly run 41355215502](notes/nightly-repair-2026-11-02.md); its count check
failed because both actual pass counts had improved from 244 to 252.

<!-- counts:begin -->
<!-- Written by tools/readme_counts.py from docs/counts.json; do not edit by hand. -->

| Criterion | Recorded result | Met at measurement |
| --- | --- | --- |
| [sv-tests](https://github.com/chipsalliance/sv-tests) simulation tests (pinned commit, hand-written tests, 344): at least as many as Icarus Verilog, on Linux and on macOS | Linux: kirimasim 253, Icarus Verilog 211; macOS: kirimasim 253, Icarus Verilog 210 (14.0 devel, 04cc493, the build the criterion uses, built on each) | Yes |
| kirimasim's own RTL test population (1,466 tests, fixed list): 90% pass | 1,456 pass (99.3%), 0 fail; 10 stop at constructs of later work | Yes |
| The mandatory test list (except the one that needs P4) | All pass | Yes |
| No disagreement between the fast value operations and the reference implementation (`valsem`): seven nightly fuzz runs in a row with 100 times the rounds | The nightly fuzz runs passed on 7 nights in a row, through 2026-10-27 | Yes |
| The fixed list of probes against Icarus Verilog 13.0 ([P1_SCOPE](spec/P1_SCOPE.md)), with approved exclusions by source hash | Historical 95-probe inventory: 94 P1-evaluated, 1 out of scope (`real_net`). Raw passing-reference gaps 6/6; required-scope gaps 5/5, reference mismatches 4/7 (13.0 stable/14.0 devel). Raw outcomes are unchanged; finite probes alone do not show complete coverage | Not yet |

The external sv-tests counts for kirimasim were measured on 2026-11-07 (Linux) and 2026-11-08 (macOS); the population on 2026-10-27; the Icarus probes on 2026-10-27. Icarus Verilog's and Verilator's in the last two columns on 2026-10-17, from their main branches that day. The nightly run measures sv-tests and the population again, and fails when they differ from these.

| Test set | Tests | kirimasim | Icarus Verilog | Verilator |
| --- | --- | --- | --- | --- |
| This repository's tests (CTest, dev configuration) | 13,730 | 13,730, 0 skipped | — | — |
| [sv-tests](https://github.com/chipsalliance/sv-tests) simulation tests | 344 | 253 | 213 | 321 |
| Own test suite, P1 population | 1,466 | 1,456 | 1,274 | 1,329 |

This repository's tests by kind: unit tests (GoogleTest) 1,468, `tests/suite/` 11,926 (made by 214 generators under `tools/`), tools 234, `tests/dump_sir/` 29, state and Contexts 21, command line and event counts 52.

P1 out of scope: `real_net` — Confirmed Icarus wire real extension; optional for P1. Policy reviewed 2026-11-01; this is a scope reassessment of the dated observations, not a new run or PASS.
<!-- counts:end -->

Use `python3 tools/readme_counts.py update` to synchronize both READMEs and both
copies of this document. Supply only inputs actually remeasured; do not replace
historical external numbers with current build counts. `check` validates all four
outputs. The [README archive](history/README_2026-11-01.md) preserves the tables
as they appeared before the documentation reorganization.

## Linux under WSL2, 2026-11-12

The dev configuration was built and run on Linux under WSL2 at `7dba044c`:
**13,730 / 13,730 CTest entries passed**, with 0 skipped and 0 failed, in
1,010.5 s. The build had no warnings, with warnings treated as errors and the
kernel's deep checks on (`KIRIMASIM_DEEP_CHECKS`, as the dev preset sets). It
rebuilt only what had changed in the existing `build/dev` (slang's pin
`6fbb158f` had not moved); configuring and building took 59 s.
`tools/readme_counts.py check` accepted this run's build tree and JUnit file:
the total, the passed and skipped counts and each kind equal the recorded dev
counts above. The
[machine-readable record](measurements/2026-11-12-wsl2/dev-ctest.json) holds the
commands and the results.

- **Environment:** AMD Ryzen 9 7950X (32 logical CPUs under WSL2), 110 GiB,
  Ubuntu 24.04.4 LTS on WSL2, kernel 6.18.58.1 (6.18.54.1 in the earlier runs
  below), glibc 2.39, x86-64. C++ is built with Ubuntu clang 20.1.8 and C with
  GCC 15.2.0, as the machine's `c++` and `cc` are; CMake 4.2.1.
- **Not in this run:** the release, asan and ubsan presets, the static checks,
  clang-format and clang-tidy of `tools/local_ci.py`, and sv-tests, the P1
  population, the Icarus probes, fuzz and speed. This run says nothing about them.
- **Full local CI, started 2026-11-12:** after this run,
  `tools/local_ci.py --llvm-bin /usr/lib/llvm-22/bin` passed every step at
  `7dba044c` with one documentation commit on top: the dev, release, asan and
  ubsan presets, the static checks, and clang-format and clang-tidy of
  LLVM 22.1.8 (6,491 s in all).
- **Earlier runs on this machine:**
  - 2026-11-08, at `33cee3fa`: 13,618 / 13,618 dev CTest entries passed
    ([record](measurements/2026-11-08-wsl2/dev-ctest.json)). Then
    `tools/local_ci.py --llvm-bin /usr/lib/llvm-22/bin` passed every step at
    that commit with one documentation commit on top: the dev, release, asan and
    ubsan presets, the static checks, and clang-format and clang-tidy of
    LLVM 22.1.8 (7,968 s in all).
  - 2026-11-07, at `60c26d7c`: 12,581 / 12,581 dev CTest entries passed
    ([record](measurements/2026-11-07-wsl2/dev-ctest.json)). The full local CI
    passed every step the same day at `bf081ac5` with one documentation commit
    on top (6,839 s in all).
- **The asan preset on this machine:** on 2026-10-30 the asan build stopped
  with "linked against incompatible ASan runtimes", while `cc` and `c++` were
  GCC and Clang. `build/asan` has been configured with `clang` and `clang++`
  since, and it built and passed in all three local CI runs above.
