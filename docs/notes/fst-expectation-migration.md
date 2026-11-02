# FST CLI expectation migration

[日本語](fst-expectation-migration.ja.md) · 2026-11-03.

Status: **the maintainer explicitly approved the one existing expectation migration on 2026-11-03**. Approval of
[FST contract sections 1–8](fst-p1-proposal.md) did not approve this migration; separate approval was obtained.

The unchanged input is `tests/cli/display_finish.sv`, SHA-256
`87760045f8ca4c00e31e5e84248adde4f4831d4a840a1487196f7ad4fe6b57d0`. The existing `tests/cli/run_cli_tests.py` row runs it with
`--wave=w.fst` and expects exit 2, empty stdout and the unsupported-FST diagnostic.

Native execution through existing RunOptions now ends with `Finish`, exact
stdout `hello\na`, and a readable FST with zero variables (this design has no
static signals). `Fst.ExistingRejectedCliInputRunsNativelyBeforeSeparateMigrationReview`
checks that input, output and decoded file. The broader 138 native FST tests
check actual values, times, aliases, events, controls, force/specify and Context
isolation; an openable empty file alone does not establish waveform correctness.

The approved **one-row** migration is:

```python
(["--wave=w.fst", DESIGN], 0, "=hello\na", "kirimasim: info: finish at 0 ns"),
```

Keep the exact source, arguments and all other existing assertions. Run this
new file-producing case in a temporary directory and check the artifact exists;
its decoded empty-design result is covered above. Add separate new CLI checks
for nonempty FST values, parse/dump-SIR producing no file, invalid suffixes and
empty basenames. Existing VCD traces, PerfCount, populations and P1 gates stay
unchanged. The invalid-extension message can append `.fst` while preserving its
current tested prefix. No unrelated historical diagnostic is migrated.

The grounds are the approved FST contract 1/8, WAVEFORM's FST amendment and
CLI 2.2/5. FST's binary format is a project contract, not an IEEE-mandated file
format; IEEE 1800 21.7 supplies the existing dump-control behavior being shared.
Commit the approved expectation change separately from simulator-source changes.
Public `.fst` CLI acceptance was activated after the separate approval. The
actual CLI also returned exit 0, exact stdout `hello\na` and the ordinary finish
diagnostic on this unchanged input; the pinned reader opened its output. A valid
zero-variable FST may have no text hierarchy conversion; empty conversion output
is not treated as corruption. Added CLI checks passed for 129-bit 0/1/X/Z values,
times, default/glitch modes, parse/SIR without output files and invalid names.
