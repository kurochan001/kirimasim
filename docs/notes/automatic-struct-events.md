# Automatic structure event fields

[日本語](automatic-struct-events.ja.md). 2026-10-30.

Automatic unpacked structures now carry scalar event leaves through the
existing typed frame, capture and managed-event paths. This implements the
structure-field connection under the approved [automatic-capture contract
1–6](automatic-capture-proposal.md); it adds no event-value semantics, SIR
instructions, kernel dependencies or VM changes.

Each default-initialized event leaf gets a distinct object when the declaration
executes. Nested records retain their leaf order. A declared null default
remains null. Declaration copies, assignment-pattern initializers and input
record arguments share event identities while copying ordinary fields by
their existing rules. These initialized leaves do not construct unused event
objects before receiving their values. Member defaults and all four
`--init-values` modes retain their existing meanings.

Captured record fields share the enclosing activation. Waits, `.triggered`,
blocking/nonblocking triggers, waiting input calls and deferred input snapshots
use the existing loaded event operands. Returning from an owner does not
replace the event or end its retained descendants. Cancelling descendants
releases their waits and bindings, and automatic collection reuses event rows.

```systemverilog
module top;
  typedef struct { event ready; int value; } packet_t;
  task automatic launch(input int number);
    packet_t packet;
    packet.value = number;
    fork
      begin #1; ->packet.ready; end
      begin @(packet.ready); $display("%0d", packet.value); end
    join_none
  endtask
  initial begin launch(17); #2; end
endmodule
```

This prints `17` even though `launch` returns before either child resumes.

## Preserved event-value boundary

General event rebinding is still outside the approved source contract.
Whole aggregate assignments containing event leaves now receive the same
positioned unsupported diagnostic as direct event-value operations; the
previous static-structure path could bypass that restriction. Event-bearing
output/inout records and record return values are also rejected. Declaration
initialization and input copies remain the supported identity-sharing entries.
This is an implementation boundary, not a claim that these forms are illegal
SystemVerilog.

Automatic arrays of structures and fixed unpacked-array members keep their
existing limitations. Scalar event returns/conditionals, event rebinding and
the broader event model remain work. The prior combination-gap measurement
at `51cda314` remains a historical observation; this change implements its E01
connection and some initialization/input combinations, not the whole list.

## Validation

The new [CLI matrix](../../tests/cli/run_struct_event_tests.py) has 180 checks across eight
execution homes, nested records, four initialization modes, copied/null
defaults, constructor counts, input forwarding, capture, pending triggers,
deferred input, collection and positioned rejection boundaries. A null event
field retains the existing runtime error when waited on.

[Six unit tests](../../tests/unit/struct_event_test.cpp) execute after source
and frontend AST destruction. They also check current-slot identity,
concurrent retained activations, cancelled waits, ordinary value-copy behavior,
zero live scopes/events at completion and bounded event-row reuse under 6,000
fresh record declarations. Existing source-event boundary, SIR output and
PerfCount checks retain their expectations.

Two mutation anchors were adapted to the shared initializer expression without
changing their faults: omitted container snapshots and missing event defaults.
The source-value semantics and existing array-event diagnostics are preserved.
Six compiled controls are detected by the new unit tests and the existing
container-snapshot regression: missing fresh defaults, redundant initialized
constructors, enabled aggregate rebinding, enabled output rebinding, omitted
container snapshots and shared event objects within a record. Restored sources
pass all focused checks.
