# Ordinary observers alongside class objects

[日本語](class-independent-observers.ja.md)

Status: implemented under the [approved class contract, section 7](class-runtime-proposal.md#7-event-dependencies-and-observation).
This connects ordinary observation in designs that also allocate objects. It
does not implement object-property dependency paths or complete P1.

## Boundary and implementation

IEEE 1800-2023 8.4, 9.2, 9.4, 10.4 and 21.2, SCHEDULER_SPEC 6/9/13,
and the class contract preserve the existing meaning of ordinary signal/event
observation. Previously, either frontend or loader rejected the whole design
when objects coexisted with any sensitivity process, dependency wait, monitor
or event-controlled NBA, even if the observer never used an object.

`sir::classExecutionShapeSupported` now provides one implementation-boundary
check shared by `Lowerer::checkClassExecutionSupport` and the VM loader. It
classifies observer roots and the transitive ordinary-call graph, propagating
object use through recursive call groups without recursive C++ traversal.
Front uses the result for a positioned unsupported diagnostic; the VM checks
verified native SIR before allocating runtime storage. There is no new
instruction, semantic option, scheduler behavior, dependency or ABI version.

Supported combinations include object-free `always_comb`, `always_latch`,
`always @*`, continuous assignments, `always_ff`, explicit/conditional/named
waits, monitors and eligible event-NBA layouts. Static class methods and
properties use ordinary typed slots. Waiting object methods can observe
ordinary signals/events while retaining their existing receiver roots.
Observers keep their existing scheduling and value-change behavior; in
particular, `always @*` gains no automatic time-zero execution.

The boundary remains conservative. Property/handle reads, object calls and
object-containing values in an observer's evaluation closure remain
unsupported. This includes indirect calls, iff guards, monitor callbacks and
event-NBA watch/guard callbacks. Generic refs are rejected in observer
closures and event-NBA payloads because their object-property provenance is
not connected. Reactive process bodies containing object operations remain
unsupported even when a narrower explicit sensitivity might be sufficient.
Procedural overrides, pattern/class formatting, managed event fields and
nonstandard initialization overrides keep their previous boundaries.

## Verification

The original [generator](../../tools/gen_class_observer_matrix.py) provides
280 source programs: declaration homes in compilation units, modules,
packages, interfaces, programs and generate scopes; static/automatic handles;
direct/two-level function reads; 1/129-bit two/four-state values; ordinary
observation, NBA delivery and receiver retention across allocation pressure.
Reactive module items are not generated inside programs. Monitor expectations
check that changing an unrelated property does not add output.

Three additional frontend unit tests cover static-method sensitivity,
positioned boundaries through recursive calls, and well-formed native SIR
with an observer call redirected to an object-reading function. The latter
checks wait, monitor and both event-NBA callback forms, requiring loader
rejection before slots, heap cells, objects or activations are allocated.
Existing test sources, expectations and performance budgets are unchanged.

Five original finite Icarus probes accompany the matrix. They measure only
these coexistence combinations; they do not establish complete class parity.
Compiled fault-injection and full regression results are recorded after
validation. The separate partial-inout expectation migration is unaffected;
slang posting remains on hold.
