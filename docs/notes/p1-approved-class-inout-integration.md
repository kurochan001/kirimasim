# Approved class-observer and partial-inout integration

[日本語](p1-approved-class-inout-integration.ja.md)

Status: **both expectation migrations approved and applied (2026-11-01)**.
Full dev regression passes. This is a working-branch integration; main and push
remain separate, and slang posting remains on hold.

## Approval and preserved inputs

The maintainer explicitly approved the two presented items, covering three
existing tests. [Class observation](class-observer-expectation-migration.md)
uses its own expectation commit `dd7a36b6`; [partial inout](partial-inout-expectation-migration.md)
uses `5766887f`, separately from implementation. The constant-wait SIR, CLI SV
and embedded port-unit SV are unchanged. The new expectations require normal
execution, exact initialization/identity and the documented zero-allocation or
rootless properties; unrelated rejection tests and performance budgets remain.

The source bytes of class implementation `dfce0f33` and partial-inout
implementation `85535e72` are preserved. The shared class observation preflight,
physical net identity and existing scheduler/kernel rules keep their approved
meaning. Integration adds no dependency, semantic option or ABI version.

## Combined verification

The [validation record](../measurements/2026-11-01/p1-approved-class-inout-validation.json)
records source hashes and the actual integrated implementation revision.

- All 525 selected class/port/CLI checks pass without exclusions.
- Full dev passes **11,299/11,299**, zero failures/skips and no exclusions.
- The generated count record is updated only from that successful full JUnit:
  1,074 units, 9,948 suite cases, 172 generators, 192 tools, 29 SIR-dump checks,
  21 Context checks and 35 CLI/event-count checks.
- All 1,082 mutation anchors match. The separately recorded six class and five
  inout compiled controls remain applicable because implementation bytes are
  unchanged. Their original checkpoints retain their own scopes.

The complete candidate check is:

```sh
CMAKE_BUILD_PARALLEL_LEVEL=8 CTEST_PARALLEL_LEVEL=8 python3 tools/local_ci.py
```

It checks a clean frozen candidate across dev/release/ASan/UBSan, static rules,
expectation separation, generated counts, formatting and clang-tidy. The exact
head and run result are kept with that candidate in
`build/p1-approved-publication-candidate.json` and
`build/p1-approved-local-ci.log`; the run summary accompanies the report.

## Frozen Icarus 13.0 inventory

The same 176 original sources/oracles run against baseline `6affd5a9` and
integrated `5766887f`, explicitly selecting the 13.0/v13_0 compiler and runtime.
Passing-reference gaps decrease **19 → 3**. Prior 156/161/171 native results,
source hashes, oracles and stdout are preserved. The 25 reference mismatches
remain separate; the four added wired readouts retain their unconfirmed cause.
See the [baseline](../measurements/2026-11-01/icarus13-approved-class-inout-baseline.json)
and [integrated run](../measurements/2026-11-01/icarus13-approved-class-inout.json).
Remaining gaps are specify paths, dense UDP frontend compilation limits and
the deliberately deferred Icarus real-net extension. No reference output is
adopted as an oracle. These finite checks do not establish P1 completion;
population, sv-tests and nightly fuzz are not remeasured.

## Earlier public checkpoint

Public main `6affd5a9` passed all 21 local-CI steps, with 10,785 checks each in
dev/ASan/UBSan, 10,786 in release and clang-tidy over 165 files. This is the
predecessor record, not the current integrated test count.
