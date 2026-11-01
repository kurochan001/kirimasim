# Recorded validation counts

[日本語](VALIDATION_COUNTS.ja.md)

These are the latest records stored in [counts.json](counts.json), with separate
measurement dates and populations. The original 95-probe result below is dated
2026-10-27; the [current support inventory](P1_SUPPORT.md) and
[176-probe scope record](measurements/2026-11-01/icarus13-wire-real-optional.json) describe
later coverage. These datasets do not measure a percentage of language completion.
Repository CTest counts can change independently of external measurements.
See [Validation](VALIDATION.md) for gate rules, versions and reproduction.

<!-- counts:begin -->
<!-- Written by tools/readme_counts.py from docs/counts.json; do not edit by hand. -->

| Criterion | Recorded result | Met at measurement |
| --- | --- | --- |
| [sv-tests](https://github.com/chipsalliance/sv-tests) simulation tests (pinned commit, hand-written tests, 344): at least as many as Icarus Verilog, on Linux and on macOS | Linux: kirimasim 244, Icarus Verilog 211; macOS: kirimasim 244, Icarus Verilog 210 (14.0 devel, 04cc493, the build the criterion uses, built on each) | Yes |
| kirimasim's own RTL test population (1,466 tests, fixed list): 90% pass | 1,456 pass (99.3%), 0 fail; 10 stop at constructs of later work | Yes |
| The mandatory test list (except the one that needs P4) | All pass | Yes |
| No disagreement between the fast value operations and the reference implementation (`valsem`): seven nightly fuzz runs in a row with 100 times the rounds | The nightly fuzz runs passed on 7 nights in a row, through 2026-10-27 | Yes |
| Standard execution coverage at least as broad as Icarus Verilog 13.0 ([P1_SCOPE](spec/P1_SCOPE.md)), with the approved optional `wire real` exception | Historical 95-probe inventory: 94 P1-evaluated, 1 out of scope (`real_net`). Raw passing-reference gaps 6/6; required-scope gaps 5/5, reference mismatches 4/7 (13.0 stable/14.0 devel). Raw outcomes are unchanged; finite probes alone do not show complete coverage | Not yet |

The external sv-tests counts for kirimasim were measured on 2026-10-26 (Linux) and 2026-10-27 (macOS); the population on 2026-10-27; the Icarus probes on 2026-10-27. Icarus Verilog's and Verilator's in the last two columns on 2026-10-17, from their main branches that day. The nightly run measures sv-tests and the population again, and fails when they differ from these.

| Test set | Tests | kirimasim | Icarus Verilog | Verilator |
| --- | --- | --- | --- | --- |
| This repository's tests (CTest, dev configuration) | 11,447 | 11,447, 0 skipped | — | — |
| [sv-tests](https://github.com/chipsalliance/sv-tests) simulation tests | 344 | 244 | 213 | 321 |
| Own test suite, P1 population | 1,466 | 1,456 | 1,274 | 1,329 |

This repository's tests by kind: unit tests (GoogleTest) 1,217, `tests/suite/` 9,948 (made by 176 generators under `tools/`), tools 193, `tests/dump_sir/` 29, state and Contexts 21, command line and event counts 39.

P1 out of scope: `real_net` — Confirmed Icarus wire real extension; optional for P1. Policy reviewed 2026-11-01; this is a scope reassessment of the dated observations, not a new run or PASS.
<!-- counts:end -->

Use `python3 tools/readme_counts.py update` to synchronize both READMEs and both
copies of this document. Supply only inputs actually remeasured; do not replace
historical external numbers with current build counts. `check` validates all four
outputs. The [README archive](history/README_2026-11-01.md) preserves the tables
as they appeared before the documentation reorganization.
