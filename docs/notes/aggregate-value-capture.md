# Aggregate assignment value capture

[日本語](aggregate-value-capture.ja.md). Source `917c715`, 2026-10-24.
This restores the approved VALUE_SEMANTICS 3.1/3.13 and SIR_SPEC 7.3 rules.
IEEE 1800-2023 10.4.1/10.4.2 leave the relative evaluation order open;
kirimasim's approved choice evaluates the RHS before target selectors.

## Corrected behavior

An original structure-array probe has a RHS function that records digit 1
and sets an index to 1, and a target selector that records digit 2 and
increments that index. Both `a[choose()] = source_value()` and its delayed
NBA form previously recorded `21`, left the index at 1 and updated `a[0]`.
They now record `12`, leave the index at 2 and update `a[1]`.

All aggregate RHS leaves are captured before runtime target selectors.
Container leaves take independent snapshots immediately, so later pattern
members or a target function cannot mutate an earlier captured value.
The delayed NBA path keeps the existing scalar order: RHS, delay once,
target selection. Immediate and delayed NBAs still update in NBA/Re-NBA;
blocking assignments write immediately after capture and target selection.
Invalid selections retain expression effects and update no valid neighbour.

Static target layouts need no runtime selection and reuse their pending
places. Runtime targets use a private typed layout to evaluate the RHS;
its places are discarded and the captured SSA values feed the actual
stores. No new SIR instruction, kernel/VM policy, C ABI, dependency or
installation is introduced. Existing SIR dump expectations are unchanged.

The same traversal now supports whole fixed subarrays selected at run time,
including array members of selected structure elements. Hidden function-return
storage can hold fixed-array members in an unpacked structure, for both
automatic and static functions. Explicit automatic structure variables with
array members, arrays of structures as arguments, and structure refs retain
their existing unsupported forms. At this revision, blocking whole-aggregate
timing was unfinished; [subsequent work implements it](blocking-aggregate-timing.md).
Event-controlled NBAs remain unfinished.

## Source evidence

The [original generator](../../tools/gen_aggregate_nba_capture_matrix.py)
adds 72 source cases. Sixty-four cross eight execution contexts,
1/32/65/129-bit payloads and two/four-state domains. Each checks blocking,
immediate NBA, zero-delay NBA, effectful delayed NBA, invalid targets,
RHS/target order, snapshots and region timing, with real/string members.
Eight additional cases cover member arrays, different declared bounds,
runtime subarray prefixes, arrays of structures, queue leaves and input
patterns, source mutation, and automatic/static function returns.

Against the retained old binary reporting `87ae3a27b904`, 64 cases fail by
stdout differences and eight stop as unsupported. The corrected source
passes all 72 and the generator check. Existing suite bodies, expectations
and performance counts are unchanged; the 29 existing SIR dump tests pass.

Full local CI at `917c715` passes dev/release/ASan/UBSan and all static
checks, including clang-format, clang-tidy, expectation protection and README
counts. Dev/ASan/UBSan each run 2,980 tests (2,978 pass, two known skips);
release runs 2,981 (2,979 pass, the same skips). Unit/suite/tool/generator
counts are 298/2,493/130/125.

Nine aggregate-related mutations compile and are caught by the suite with
anchor/judging self-checks excluded. The six new controls each fail actual
new stdout expectations: early target selection, borrowed container leaves
in four aggregate paths, and repeated delay evaluation. The updated
delay-dropping control and an existing wrong-bound copy control also fail
the new cases. The existing equality control is caught by its older tests.
The restored source rebuilds. These nine executed controls are separate
from checking that all 489 mutation anchors apply.

## Comparison scope

Three original probes expand the finite inventory from 72 to 75. The two
RHS-order probes are tagged `project_policy`: their expected order is the
approved kirimasim policy, not the only order permitted by IEEE. The runtime
subarray probe is a standard mechanism. Installed Icarus 13.0 and 14.0 devel
reject these three source forms; kirimasim passes them. Their results do
not close an Icarus-passing gap. The [versioned comparison record](icarus-13-comparison.md)
keeps the 13.0 and development baselines separate. Complete parity remains
unconfirmed.

```sh
ctest --preset dev -R 'aggregate_nba_capture|dump_sir' --output-on-failure
python3 tools/gen_aggregate_nba_capture_matrix.py --check
python3 tools/mutants.py --only aggregate
```

The mutation runner requires clean `src/` and restores every modified file.
