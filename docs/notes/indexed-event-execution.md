# Runtime fixed-array event selections from source

[日本語](indexed-event-execution.ja.md). Status: implemented and checked,
2026-10-26. The historical CLI expectation migration was separately approved
in `c53ccfe7` and applied in `28f5c972`. The
[post-migration validation](../measurements/2026-10-26/indexed-event-migration-validation.json)
passes all 21 local CI checks: dev/ASan/UBSan 5,139/5,141, release 5,140/5,142,
zero failures and two known skips each. The earlier failed runs below remain
historical evidence; the instruction-safepoint proposal remains unapproved.

## Contract and connection

The grounds are the approved automatic-capture contract 5/7, SIR_SPEC 3.2.1/7/8,
VALUE_SEMANTICS 3.11, SCHEDULER_SPEC 9.4/9.5 and IEEE 1800-2023
7.4.5/9.4.2/15.5. Sources, generators, boundaries and controls are original;
no other simulator's implementation or tests were read. No dependency, C ABI,
IR version, performance budget or collection timing changes.

`2108dd72` connects `Lowerer::eventOperand` to `placeOf`/`loadPlace` for runtime
fixed-array selections. The same storage decoder handles declared bounds,
dimension order, wide/X/Z indices and source evaluation order. Valid constant
static selections retain their compact legacy slot form, preserving all existing
SIR dumps. `.triggered` loads a slot target but directly uses an already loaded
event value. No independent array meaning is implemented in the VM.

Named waits select once on registration; later index changes do not change the
selected object or cause a named trigger. Mixed `iff`/signal waits use the same
kernel registration. Nonblocking targets are evaluated once before the optional
delay; delay-side mutation does not redirect a pending trigger. Explicit
`.triggered` monitors capture the object actually read and reattach when indices
change. Invalid runtime selections yield null: immediate/nonblocking triggers
are no-ops, including a supplied overflowing delay; queries are false and waits
give the approved positioned runtime error, exit 2.

Slang's strict diagnostics remain authoritative. In particular, a literal
out-of-bounds index is rejected with its existing promoted `-Windex-oob`
diagnostic before lowering. This work does not weaken it to exercise the runtime
null path. Implicit named-event sensitivity remains unsupported with its existing
positioned diagnostic, including reads through nested/recursive functions.

This is a front connection needed by the wider loaded/captured-event work.
It constructs no new managed event object. Source automatic default construction,
event-value assignment/comparison/printing and instruction-level collection are
not enabled. The separately proposed event safepoint amendment remains unapproved.

## Original mechanism matrix

`gen_indexed_event_matrix.py` generates 160 designs. Sixty-four normal designs
cover eight execution contexts, local and qualified names, ascending/descending
and rank-two layouts, named waits, mixed `iff`, delayed-target snapshots and
executed `.triggered` reads. Automatic task/fork variants use captured automatic
index variables. Selector and delay functions count calls/order; their arguments
and ref writes pass through the existing shared scalar/reference path.

The other 96 designs cross four contexts, all three layouts, four invalid
runtime indices (below bounds, X, Z, and a 129-bit value), and trigger/query versus
null-wait behavior. The new policy checker covers 12 implicit-sensitivity call
chains and four literal-bound diagnostics. All 160 designs and all 16 boundaries
pass. Four independently compiled controls detect runtime-selection rejection,
wrong getter targets, duplicate selector evaluation and target reevaluation after
delay-side mutation. Restored production passes 192 focused CTest cases,
including all 29 existing dumps and the existing function-sensitivity checker.

The [compiled-control evidence](../measurements/2026-10-26/indexed-event-controls.json)
records four successful builds followed by real test failures, and a passing
192-case restored run. The [full local-CI record](../measurements/2026-10-26/indexed-event-validation.json)
has 17 successful checks and four failed CTest steps. The first dev run also
found stale generated suite/generator counts. After regeneration, a complete dev
rerun passes 5,128/5,131 with only `cli/options` failing and two known skips;
ASan/UBSan have the same counts, and release passes 5,129/5,132 with that same
single failure and two skips. All 160 new designs, existing dumps and PerfCount
pass. No existing expectation changed. Generated README counts show the actual
one-failure dev result rather than retaining the earlier green checkpoint.

[Reference observations](../measurements/2026-10-26/indexed-event-reference-observations.json)
retain exact source hashes, actual compiler/runtime versions and passing native
outputs. The [frozen 87-probe rerun](../measurements/2026-10-26/icarus14-indexed-event-probes.json)
preserves its previous results. Population, sv-tests, Linux and speed comparisons
were not remeasured.

## Comparison and remaining approval

The installed Icarus 14 development build cannot compile any of the 160 designs.
Two separately recompiled representatives explicitly report event arrays
unsupported. Other raw diagnostics were not retained. These failed reference
runs do **not** validate the native expectations; those are original
specification-derived cases marked `spec_check: ai`. No fresh Icarus-13 run was
made. The existing 87 native/Icarus-14 statuses and passing output bytes are
unchanged, leaving 21 historical Icarus-13-passing gaps. No whole-language parity
or P1 completion follows from these finite observations.

The unchanged `event_var_index.sv` historical CLI input succeeds silently
and quiesces at time zero. Its six old unsupported diagnostics caused the
pre-migration `cli/options` failure above. The separately
[approved migration](indexed-event-expectation-migration.md), applied in
`28f5c972`, adds one acceptance entry and removes exactly those six diagnostics,
preserving the historical source and every other diagnostic. The complete CLI
suite and full local CI now pass. Automatic construction and new GC timing
remain separate work; this approval does not authorize them.
