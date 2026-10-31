# Class property-target inventory (2026-10-31)

[日本語](class-property-target-inventory.ja.md)

The [15 original probes](../measurements/2026-10-31/icarus13-class-property-target-inventory.json)
compare clean checkpoint `8e0f8467` with installed Icarus Verilog 13.0 (v13_0).
Sources, SHA-256, written oracles, statuses and actual version strings are retained.
Only reference installation paths and shell process IDs in crash diagnostics are
redacted. No external simulator source or test was read or copied.

For the measured forms, Icarus rejects bit/part/indexed property targets on
8/129-bit vectors and int, property refs, a fixed-array field and a packed-record
field (nine compile failures). Whole-property NBAs, including delayed NBA and
two-state vector forms, compile but abort at runtime (five failures). Both
engines pass the delayed blocking scalar property assignment. Native also
passes the fixed-array property element. The other thirteen native forms remain
unsupported under the approved class contract; reference failures do not become
native oracles or remove that backlog.

None of these fifteen forms adds an Icarus-passing native gap. Prioritize the
seven confirmed named-fork disable gaps next (SCHEDULER_SPEC 10.4/10.7/10.8 and
SIR_SPEC 7.2/7.5). This is a finite inventory, not proof of full class coverage.
The earlier 116 sources/oracles are unchanged; these fifteen are appended to
the runner's extended inventory. No expectation or specification is changed.
