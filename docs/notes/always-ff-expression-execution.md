# always_ff expression events

[日本語](always-ff-expression-execution.ja.md)

IEEE 1800-2023 9.2.2.4 and 9.4.2 allow expression event controls. SIR_SPEC
7.1 retains the ff kind and its single-writer/no-additional-control rules;
7.2 already defines typed watch/iff helpers and kernel monitor evaluation.
This implementation connects those existing semantics to explicit ff triggers.
It does not turn always_ff into an unrestricted general process.

`Lowerer::signalOperand` selects a fixed slot range or a typed watch, shared
by ordinary wait.event lowering and ff trigger lowering. A trigger describes
one range or one watch plus its edge and optional iff. Watch references must
have the correct kind, return an integral value and borrow their process's
frame. The verifier also checks exclusive source selection, frame activation
before entry, single-writer effects and the absence of waits in the body.
Watch trigger metadata is verified again before VM runtime values are created.
Format-0 text prints `posedge(watch @N)` while existing fixed-trigger text stays
the same. The existing guards retain their meaning and call-effect accounting.

The VM builds monitors with the existing watchMonitor path and combines them
with fixed subscriptions under one kernel wait generation. It reserves callback
storage before publishing pointers. Expression triggers arm after all frames
and final procedures are registered, before scheduled time-zero initials run.
Arming evaluates a baseline and runs no ff body. A source arming error therefore
still runs final. After a body completes, a fresh wait samples a new baseline.
The kernel keeps four-state edges, full-value unqualified changes, iff sampling,
normalized writes, private-function-local filtering and heap-path rebinding.
Instrumentation of a formerly fixed ff selector retains ff restrictions too.
Class-object/event observation remains behind the existing separate boundary.

`gen_ff_expression_matrix.py` adds 814 original sources: module/interface/generate
placements, 1/32/65/129/257-bit two/four-state values, XOR/indexed/fixed-array and
function sources, conditional/reduction/inversion forms, all edge modes, iff,
startup changes, 2/9/17 process/item lists, and dynamic-array/queue rebinding.
Counts are derived from the written four-state edge table, one source write per
step and observations after NBA; the reference simulator does not set oracles.
Ten unit tests inspect producer metadata, invalid references/kinds/owners/ranges,
writer restrictions with a valid ordinary-always control, extra waits, inactive
frames, instrumented selectors, arming errors/final, private locals and preflight.
Twelve compiled mutation controls target these contracts.

The maintainer approved accepting the unchanged historical always_ff_expression
input with exit 0 and no stdout/error diagnostics, replacing three unsupported
messages. That expectation is isolated in `e56de308`; other rejection inputs,
existing suite bodies and performance budgets are preserved. P1 coverage,
specify and VPI are not established by these finite tests.

## Validation checkpoint (2026-11-01)

Implementation `c5c37590` passes 814 generated sources, ten units and all 9,989
dev tests. All twelve mutation controls are actually built and caught; the
129 existing unit checks and eight existing source matrices pass for each.
Original source/build state is restored before final configuration checks.
The historical acceptance migration remains isolated from implementation.

The [131-probe comparison](../measurements/2026-11-01/icarus13-ff-expression.json)
uses actual Icarus Verilog 13.0 (v13_0) executables and the recorded source
revision. Both computed-XOR and runtime-bit always_ff probes pass on both
engines. Every prior source hash and expected byte is unchanged; gaps fall from
five to three, with the same four reference mismatches. The old 95-name cohort
still has three gaps. Icarus 14.0, the fixed population, sv-tests and seven-night
fuzz evidence were not rerun here. P1 remains open.
