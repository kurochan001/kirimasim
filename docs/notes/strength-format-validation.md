# Strength formatting: validation record

[日本語](strength-format-validation.ja.md). 2026-11-01.

Implementation `a4646ef9` connects approved MOS/switch contract 5/7/8.
The maintainer approved the five existing expectation migrations, applied
separately in `5e484d9e`. No source input was removed, renamed or weakened.
The [machine record](../measurements/2026-11-01/strength-format-execution.json)
contains source revisions, binary versions/hashes, mutation hashes and scope.

## Native results

The final dev run at `5e484d9e` passes **11,364/11,364**, with zero failures,
skips or exclusions (202.68 seconds). Successful JUnit supplies the separate
count update: unit 1,135, suite 9,948, Tools 193, DumpSir 29, Context 21,
CLI/PerfCount 38; generators 175. External inventories, fixed populations,
reference counts, the optional `wire real` decision and fuzz history are unchanged.

Release, ASan and UBSan each pass all 63 related tests. After the approved
expectation migration and refreshed version metadata, each also passes four
checks covering both migrated native tests, all six source MOS boundaries,
and the complete strength-format CLI matrix. These are focused profile runs,
not full preset suites. All four final binaries report `5e484d9eea2c`.

The source matrix contains 153 exact-output programs across twelve contexts
and twelve output APIs (ineligible postponed combinations are omitted from
construction, not skipped at execution). Two literal-width inputs must retain
positioned frontend errors. New unit tests cover all 120 intervals using a
literal table, 1/8/9/65/129-bit packing, complete widths/alignment, raw-value
independence, invalid metadata, selected monitor subscriptions and force owners.
Both public feature examples execute with exactly `We1`, `St0`, `We1`.

The initial full run retained historical rejections and exposed legacy native
hold admission broadening. The implementation now requires explicit opt-in to
the verified strength hold bridge; the original legacy hold rejection tests
pass unchanged. The final full run has no temporary exclusions. CTest placed
JUnit relative to its preset directory; that successful file was recovered
before the count updater ran. No passing count was fabricated or taken from
the earlier partial/failed runs.

## Compiled controls and policies

All eight registered faults compile and fail real semantic checks: lost
interval endpoints, reversed vector order, lost runtime provenance, repeated
selector evaluation, missing strength-only monitor updates, leaked monitor
subscriptions, unreleased virtual roots and incorrect per-bit force values.
The focused 63-test baseline passes before and after the experiment. Original
and restored source SHA-256 maps are identical. No other preset build overlaps
the source mutations.

All 1,111 mutation anchors apply. Changed translation units pass clang-tidy
23.1.4 after fixes; all 257 tracked C++ files pass clang-format. Global-state
checking and its probe, banned APIs (84 files), SPDX (537 files), release
flags (182 units), per-commit expectation protection and document counterpart/
local-target checks pass. The expectation-only commit reports two changed unit
assertions for review: they are replaced by the explicitly approved quiescent,
exact-interval and no-active-force checks described in the
[migration](strength-format-expectations.md).

Full `tools/local_ci.py` is not run for this implementation request; it is
required on the frozen candidate before a later push. The feature branch is
not integrated into main or pushed. Slang reporting remains on hold. No tools
or dependencies were installed. Complete external-reference equivalence for
the new matrix, broader net domains and full P1 parity remain unconfirmed.
