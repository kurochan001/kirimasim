# First typed implicit container observers

[日本語](implicit-container-execution.ja.md) · 2026-11-06.

The [contract, sections 1–8](implicit-container-observers-proposal.md) was
approved on 2026-11-06 and implemented in `8873c89a`. The
[eleven existing rejection judgments](implicit-container-expectation-migration.md)
were approved separately and migrated in `b3d20b8b`. These changes are included in the [2026-11-06 main integration](main-integration-2026-11-06.md). The measurements below precede that combined integration.

## Executable subset

Implicit sensitivity identifies the inputs whose changes rerun a body. Eligible
static integral-element dynamic arrays and queues now use typed whole-value
bindings in comb/latch, leading `always @*`, and direct continuous assignments.
Bounded queues and supported two/four-state widths share this path. Tested reads
include size queries, by-value arguments, ordinary/nested function reads in
comb/latch, and whole-identifier wildcard dependencies for element reads.
Module, interface, generate, unit/package, non-parameterized class-static and
eligible qualified static-subroutine storage are covered.

Front selects dependencies statically; SIR stores and verifies each slot/type
binding. VM binds it to the shared kernel. The kernel compares length and ordered
normalized element bits, including X/Z, without evaluating source code. An
element change can rerun a size-reading body even if the size stays equal.
An equal copy does not wake it, but attaches the observer to the replacement's
cells. Weak markers and raw value words add no GC roots or language copies.
Committed A→B→A changes remain observable inside a walk, and a multi-cell
operation exposes its completed value. Each wait wakes once in its existing
Active/Reactive region.

Contributing selected-element dependencies in comb/latch, automatic/ref bindings,
nested implicit controls, nonintegral/nested elements, associative/class-dependent
observers and continuous assignments with called-container-body reads remain
outside this first delivery. Preserve strict slang errors where a source form is
illegal. These boundaries remain P1 work where required, not new exclusions.
Explicit event-expression legality and result watches are unchanged.

## Verification and limits

The [source matrix](../measurements/2026-11-06/implicit-container-source-matrix.json)
contains **584 executions and 12 expected frontend rejections**. All 596 checks
pass; the rejected sources remain rejected. Models normalize explicitly declared
`bit`/`logic` widths before deciding value changes and wake counts. No simulator
output supplies an oracle. The earlier
[96-case candidate investigation](../measurements/2026-11-05/implicit-container-review.json)
is preserved unchanged: its `int`/X/Z candidates are not this acceptance gate.
The twenty minimal reference-passing gaps recorded there now execute.

The [validation record](../measurements/2026-11-06/implicit-container-validation.json)
records unfiltered dev **12,262/12,262**, with no skips or failures, and related
release/ASan/UBSan **1,003/1,003 each**. After extending the existing bounded/OR
test, all eight kernel/SIR tests pass again in all four presets; counts do not
change. Two additional front tests verify static closure, deduplication and
private/written exclusions. Counts were updated separately in `08a733f2`.

All **12 compiled faults** were caught: token-only/size-only comparison, collapsed
X/Z, delayed final-state comparison, missed binding publication, stale equal-copy
attachments, language cloning on arm, wildcard body execution while arming,
dropped qualified storage, missing descriptor verification, wrong wildcard call
closure and unreleased hook ownership. After restoring sources, the kernel/SIR,
front and all 596 source checks pass; original source hashes match.

Format, affected-TU clang-tidy, SPDX, banned/global-state, release flags and
expectation separation checks pass. All 1,216 mutation anchors apply. At this checkpoint, full `tools/local_ci.py` and GitHub CI had not been run. The [integration record](main-integration-2026-11-06.md) records the later complete local-CI attempt and its host-sleep timeouts.

The [188-probe checkpoint](../measurements/2026-11-06/icarus13-implicit-container-checkpoint.json)
keeps every existing source/oracle pair. It has 187 P1-evaluated cases, one
optional `real_net` exclusion, the existing required `dense_udp_coverage` timeout,
and 25 separate reference mismatches. The new source matrix is not a P1 pass
rate. This delivery does not establish complete Icarus parity or P1 completion;
the standard `real` scope and fixed external populations remain unchanged.

```sh
python3 tests/cli/run_implicit_container_tests.py --self-check
python3 tests/cli/run_implicit_container_tests.py --kirimasim build/dev/src/driver/kirimasim --json build/implicit-container-source-matrix.json
ctest --preset dev -R '^ImplicitContainer|^ImplicitFront|^cli/(dynamic_containers|implicit_containers)$' --output-on-failure
python3 tools/mutants.py --check
```

The compiled-fault run used a clean focused baseline and the registered fault
replacements, excluding mutation applicability checks as detectors. It was not
an all-mutant campaign or a complete local-CI run. No upstream report was sent.
