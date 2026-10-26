# Event declaration initialization and shared identity

[日本語](event-initializer-execution.ja.md). Status: implemented and focused checks
passed, 2026-10-27; complete integration validation follows the separately
approved migration of three historical rejection oracles. P1 remains incomplete.

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

## Checks completed before oracle migration

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
counts. It verifies that aliases/null do not create extra objects. These 210
new CTest entries pass.

Seven compiled faults are caught: two new initializer faults, the four existing
event-input faults, and the adapted existing array-pattern snapshot fault.
The snapshot fault still removes the same container snapshot; only its anchor
changes to the refactored local type spelling. It is detected by eight existing
checks. Sources are restored exactly; all 386 selected event/aggregate checks
pass. [Control evidence](../measurements/2026-10-27/event-initializer-controls.json)
records source digests and detecting tests.

The implementation commit preserves every existing oracle. Three historical
initializer rejections need the user's approved migration in a separate commit.
Complete CI and the updated count rendering will be recorded after that
migration; this checkpoint does not claim a green complete integration yet.

## Limits

This adds scalar and assignment-pattern declaration initialization. Existing
whole-array copy machinery remains unchanged; this change does not select
semantics for rebinding variables with existing waits. General scalar rebinding,
comparisons, event-valued conditional/return expressions, automatic structure
event fields and event-containing dynamic containers remain work. No performance
budget, final collection exclusion or conservative SSA lifetime rule changes.
Finite matrices and comparison probes cannot establish complete P1 parity.
