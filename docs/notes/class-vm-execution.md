# Class runtime: VM execution

[日本語](class-vm-execution.ja.md)

This continues the [storage checkpoint](class-vm-storage.md) under sections
1, 3–6 and 8–10 of the [approved class contract](class-runtime-proposal.md).
The ordinary interpreter now executes core class SIR through the shared kernel.
**The frontend still rejects source object construction. This does not complete
source class support or the remaining P1 Icarus-parity criterion.**

## Execution and ownership

The loader verifies class SIR and registers its heap schema before activation
layouts or mutable design values. Constructor wrappers receive the already
published, pinned receiver. Base construction, declaring-field initializers,
constructor bodies and once-only instance const initialization use the kernel's
stage capabilities. Escaped receivers retain their original identity; a virtual
call during base construction sees safe defaults before derived initializers.

Field reads/writes preserve declaring identity, static view, fixed-array order,
wide masks and two/four-state defaults. Object/weak allocation, dynamic shallow
copy, upcasts, checked casts, identity and seed operations use kernel APIs.
Equality, truth and unknown conditional selection also use checked kernel APIs,
with independent logical-ID reference semantics in valsem.

Instance functions and waiting tasks enter the ordinary argument binding,
return, reference and copyback machinery. Virtual dispatch resolves numeric
slots; direct calls preserve their selected declaration. A waiting invocation
owns its resolved call instruction so nested task-stack growth and suspension
cannot invalidate the copyback site. Each constructor activation owns its stage;
unwind abandons it and releases pins without undoing escaped effects.

Class roots are typed, never guessed from integer bits: static scalars/arrays,
ordinary frames, scoped/captured activation storage, active native SSA regions,
arguments, returns, copyback buffers, deferred arguments and pending scheduler
payloads. Object-valued container elements translate owner tags at the heap
boundary. Reference tokens keep their existing storage lifetime rules. Object
pins retain storage only; they do not keep a cancelled process executing.

## Verification and limits

Twenty-two execution tests cover constructor order and early virtual calls, hidden
fields, widths 1/32/65/129/257 in two/four-state modes, arrays, virtual/direct
calls, nested waiting tasks, returns/output/ref transport, container push/pop,
copy RNG/identity, casts, weak expiry, positioned null/const errors and interrupted
construction, deferred calls and schema allocation failure. Independent roots are tested under repeated allocation after the
producer returns, including static slots/arrays, container/object edges, delayed
NBA payloads, suspended ordinary/scoped frames and detached fork captures.
Kernel identity operations are compared with valsem and reject foreign, stale
and noncanonical tokens.

Before creating runtime values, the loader still rejects class designs combined
with dependency waits, monitors, procedural overrides, event-counted NBA layouts,
pattern formatting, class-valued formatting arguments, managed event fields,
or nonstandard `--init-values` overrides.
This conservative boundary remains until object dependency graphs, private-field
observation, event ownership and formatting are connected. Ordinary numeric
formatting and delay-based waiting remain available.

Class property/container-element ref, bit and NBA instructions, canonical slang
class discovery, source lowering and its full placement matrix remain pending.
Existing suite sources, expected output and performance budgets are unchanged.
The old VM guard test now uses still-unsupported managed event storage, retaining
all original assertions. Feature probes and the P1 population are not remeasured
for this SIR execution checkpoint. Eighteen new compiled negative controls
target the execution mechanisms; the affected class preflight and three event
transfer controls retain their original faults.
