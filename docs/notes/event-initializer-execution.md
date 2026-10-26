# Event declaration initialization and shared identity

[日本語](event-initializer-execution.ja.md). Status: implemented and fully locally validated,
2026-10-27. Implementation `dfddb94b` and the approved oracle-only migration
`2da32daa` are on the local topic branch; not merged into main or pushed.
P1 remains incomplete.

## Contract and one lowering entry

The grounds are the approved automatic-capture contract 5/6, SIR_SPEC 7.4/8,
RUNTIME_ABI 6 and IEEE 1800-2023 6.8/6.21/10.9/13.5/15.5. Supported input and
initialization values share one event object before the new variable's waits.
All implementation, tests and controls are original; no external simulator
source/test, standard wording/example, dependency or installation is used.

`Lowerer::lowerEventCopy` replaces the input-only helper and is the single
event-value lowering entry for input copy-in, deferred snapshots and declaration
initialization. Named values, fixed-array selections and null retain the
existing typed place/index rules. A copy emits no `event.new`; null stays zero.
No general event-expression or rebinding entry is enabled by this helper.

Automatic scalar declarations call it at each entry. Static scalar declarations
call it in the existing pre-time-zero initialization process, once, preserving
the reviewed static initialization order and positioned diagnostics. Other
scalar types retain their previous conversion paths and diagnostic locations.

`assignArray` carries an explicit `initializeEvents` permission from the two
declaration-initialization call sites. Recursive assignment-pattern leaves
preserve that permission and use the same event-value entry. It defaults to
false elsewhere. Existing whole-array copying is unchanged. The flag is a
front lowering context, not a runtime option or an alternative value meaning.
Nested patterns, replication and default-null patterns retain existing left-bound
traversal and RHS-before-store behavior.

No VM/kernel, SIR instruction/version or C ABI changes are needed. Existing
typed fields, SSA roots, transfer pins, lexical extents and pending operations
retain the shared object through source calls, owner return, fork descendants
and event collection. No source-context tests are introduced in the VM.

## Validation and approved oracle migration

The [generator](../../tools/gen_event_initializer_matrix.py) adds 208 designs:
64 scalar/selected/null copies across eight contexts and legacy/managed values;
eight static-once checks with effectful selectors; 64 automatic/static array
patterns; and 72 retained-descendant cases across module/program, all three
join forms, three wait forms, legacy/managed origins and Active/NBA triggers.
The automatic cases repeatedly enter declarations without advancing time;
the descendant cases collect during churn after the owner can return.

The [new contract checker](../../tests/tools/check_event_initializer_contracts.py)
requires 14 positioned rejections (scalar assignment/comparison, invalid numeric
printing, pattern reassignment and null waits), and checks four SIR constructor
counts. It verifies that initialization copies/null emit no extra `event.new`
instructions. These 210
new CTest entries pass.

Seven compiled faults are caught: two new initializer faults, the four existing
event-input faults, and the adapted existing array-pattern snapshot fault.
The snapshot fault still removes the same container snapshot; only its anchor
changes to the refactored local type spelling. It is detected by eight existing
checks. Sources are restored exactly; all 386 selected event/aggregate checks
pass. [Control evidence](../measurements/2026-10-27/event-initializer-controls.json)
records source digests and detecting tests.

Implementation `dfddb94b` preserves every existing oracle. The user approved
[exactly three historical initializer migrations](event-initializer-expectation-migration.md),
applied separately in `2da32daa`. Executing both checker versions proves
that all 24 original programs are byte-identical; 19 strict rejections and
two existing acceptance oracles remain unchanged.

[Full local CI](../measurements/2026-10-27/event-initializer-validation.json)
passes 21/21 steps on `2da32daa`: dev/ASan/UBSan each pass 6,057/6,059 and
release passes 6,058/6,060, zero failures and the same two known skips.
PerfCount, 797 mutant anchors, formatting, all 115 clang-tidy translation
units and policy checks pass. The new topic initially had no upstream, so
both commits also received explicit individual expected-change checks; both pass.

Fresh [population](../measurements/2026-10-27/event-initializer-population.txt)
runs remain 1,456/1,466 with zero failures and ten unsupported; native
[sv-tests](../measurements/2026-10-27/event-initializer-svtests.json) remains
244/344, with no previous pass lost. Both
[Icarus 13](../measurements/2026-10-27/icarus13-event-initializer-probes.json) /
[Icarus 14](../measurements/2026-10-27/icarus14-event-initializer-probes.json)
comparisons rerun all 87 probes: seven gaps and four/seven reference
mismatches remain, with unchanged native stdout and no lost passing probe.
No fresh wall-clock benchmark, Linux comparison or hosted CI is claimed.

## Limits

This adds scalar and assignment-pattern declaration initialization. Existing
whole-array copy machinery remains unchanged; this change does not select
semantics for rebinding variables with existing waits. General scalar rebinding,
comparisons, scalar event-valued conditional/return expressions, automatic structure
event fields and event-containing dynamic containers remain work. No performance
budget, final collection exclusion or conservative SSA lifetime rule changes.
Finite matrices and comparison probes cannot establish complete P1 parity.
