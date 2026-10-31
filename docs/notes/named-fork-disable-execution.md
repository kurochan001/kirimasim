# Named fork disable execution

[日本語](named-fork-disable-execution.ja.md)

IEEE 1800-2023 9.3.2/9.6.2, SCHEDULER_SPEC 10.4/10.7/10.8 and SIR_SPEC
7.2/7.5 require ending every activity started within a disabled block. A fork
can retain children after join_any/join_none, and a completed child can retain
running descendants. The scope of the parent's current PC is insufficient.

`Lowerer::lowerFork` gives a targeted named fork the ordinary numeric
scope.enter/scope.leave pair and a continuation block. Declarations initialize
in the parent before children start. Empty forks use the same scope exit.
`lowerDisable` accepts any named statement block and uses the existing
scope number; sequential blocks and parallel blocks share the instruction.

At each SIR fork, the interpreter records the parent's active numeric scope
memberships on each new child. SCHEDULER_SPEC 10.8's retained parent links
carry those memberships to further descendants. The memberships outlive the
parent's scope.leave or call return and are cleared when records are freed.
Disable cancels that subtree, including waits, through the existing scheduler;
children started before or after the scope are independent. Active parent
activations still unwind to their existing continuation without output copyback.
Scope IDs distinguish separate declarations, while all dynamic activations of
one declaration are disabled together.

An executing function may finish activity of a block declared in that function
after the fork has returned. Ownership follows numeric scope.enter instructions
in its SIR body. Disabling a caller's/outside block stays a design runtime error.
Unconfirmed deferred assertion calls belonging to canceled activity are flushed
through the existing kernel API. Confirmed reports and scheduled NBAs retain
their existing behavior. Source contexts use this same mechanism, including
class methods and reactive program threads.

The generator adds 297 original sources: nine placements, three joins,
self/external/retained-parent cancellation, time/event/task waits, cancellation
before children start, automatic fork captures, reentrant/repeated activations,
independent scope IDs, function ownership, reports, queued NBAs and record reuse.
The object/event combination remains behind its existing explicit boundary.
A separate checker compares process creation and peak retention at 4/80 rounds.
Nine new compiled mutation controls cover these mechanisms. The pre-existing
empty-fork control keeps its original fault at the common finish path.

The maintainer approved accepting the unchanged historical disable_fork_block
input with exit 0 and no stdout/error diagnostics, replacing six unsupported
messages. Expectation-only commit `e543394e` retains its original source and
all other rejection inputs. Implementation, measurement and count updates are
separate purposes. P1 completion, always_ff expression edges, specify and VPI
are not established by this checkpoint. The separate property-target inventory
is [recorded here](class-property-target-inventory.md).

## Validation checkpoint (2026-10-31)

Implementation `e7398246` passes all 297 sources, the 4/80-round retention
check (created/peak 14/3 and 242/3), and the full 9,164-test dev run. Nine new
compiled controls and the existing empty-fork control are built and caught;
each preserves 394 related legacy checks. The mutation harness was corrected
to recognize the retention checker's intentional RuntimeError output as a
failure, then the affected control was rebuilt and detected again.

The [131-probe run](../measurements/2026-10-31/icarus13-named-fork-disable.json)
uses the actual source revision and Icarus 13.0 executables. All seven named-fork
probes pass on both engines. The earlier 116 hashes and oracle bytes are
unchanged; those gaps fall from 12 to 5, with four separate reference mismatches.
The fifteen added class-target probes add no reference-passing native gap.
The historical 95 names retain three gaps. No parity claim follows; 14.0,
population, sv-tests and the seven-night fuzz evidence were not remeasured here.
