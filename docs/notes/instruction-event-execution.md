# Automatic event construction and published instruction collection

[日本語](instruction-event-execution.ja.md). Status: implemented and locally checked at `e909f022`,
2026-10-26; one historical rejection-oracle migration awaits approval.

## Contract and mechanism

The grounds are the approved [automatic-capture 1/2/5/6/8](automatic-capture-proposal.md)
and [managed-event safepoint amendment](managed-event-safepoints-proposal.md),
RUNTIME_ABI 6, SIR_SPEC 3.2.1/5.1/7/8 and IEEE 1800-2023
6.8/6.21/9.3.2/13.5/15.5. Implementation, generators and controls are original.
No external simulator source or tests were read; standard wording and examples
were not copied. No dependency, option, global state, SIR version or C ABI
version is added.

The front's declaration-default path now emits `event.new` for an automatic
scalar event without an initializer, and separately for every fixed-array
element in declaration order. Reexecuting the declaration makes fresh objects.
Existing lexical environments retain the complete extent for fork descendants,
including unread event elements. The event target/wait/triggered paths already
use the common typed-place rules; no special VM case recognizes a source
container. Automatic event value formals, initializer aliases and automatic
unpacked-structure event fields remain unsupported in the front. Rebinding,
comparisons and event-containing dynamic containers are not enabled.

The VM publishes native activations in a nonallocating linked stack of stable
variable-set IDs. It covers process/task bodies, nested synchronous calls,
guard/watch callbacks, binding and output copyback, including settled deferred
calls whose issuer has already returned. It never stores pointers into a
relocatable word or registration vector. A callback's frame record cannot be
released while native execution still borrows it; RAII removes descriptors
before retiring their activations.

Complete typed input/output snapshots receive the existing transfer pins
before any copy can call source code. Input pins end only after complete
binding publishes the callee's fields. Return pins precede SSA-region retirement
and survive copyback callbacks until the caller stores the result. A partially
bound destination therefore still has a completely pinned transfer payload;
the collector never receives an incomplete root set. The typed native stack,
live frames, retained lexical extents, references, deferred arguments, typed
slots, all active SSA regions and kernel operation pins supply current roots.
Numeric and opaque encodings are never guessed to be event values. Unsupported
class/DPI/object element kinds are not silently treated as traced fields.

Before `event.new` needs another physical row, the VM collects these roots if
there is no reusable row and normal execution permits collection. A kernel
query owns the termination/final policy: `ending()` alone is insufficient,
because `runFinal` temporarily clears it while running final procedures.
No collection is added during final cleanup. Registry generation retirement
and the existing resource-limit behavior are retained. Collection executes no
source callbacks, retries no instruction and advances neither time nor region.
At this instruction point it does **not** collect the container heap.

Typed SSA words are conservatively retained until their body exits. This is
not minimal last-use liveness: one event-producing loop temporary can retain
the previous iteration's object until overwritten. Bounds depend on statically
allocated typed temporary words, live typed fields/retained extents, runtime
pins and generation-retired rows, rather than the number of loop iterations.
A one-event loop uses at most two physical rows in the checked cases.

Copy-in and copyback share a cancellation check after each committed write.
If a callback newly terminates execution or stops the originating program,
remaining copies and callee/continuation side effects stop. Calls already
eligible after their owner ended, such as confirmed reports and final calls,
are distinguished from newly canceled calls. Synchronous call ownership is
registered before copy-in can call back. This implements the transfer/lifetime
rule at the common call boundary (SIR_SPEC 8; automatic-capture 3/6), for scalar
and array copies and every represented storage home.

## Tests and review boundary

Twelve new VM tests construct 32 independently verified SIR designs: nonwaiting
process/function/task loops, lexical reentry and unread arrays, recursion to
depth 64, nested/reentrant guards, settled deferred input binding, ordinary
native fields, return pins through output callbacks, delayed named/mixed waits
in both scheduling groups, callback cancellation in synchronous/task copy-in
and output paths, retained descendants, container-heap exclusion, final policy
and allocator failure/reuse. Physical-row assertions accompany semantic checks.
One registry test checks that the reusable-row query changes only when a row
is actually collected or consumed.

The original generated source matrix has 176 designs. Default construction
uses eight containers and four shapes (scalar, both array directions, rank 2),
with 64 nonwaiting declaration entries each. Retained-task checks cover
module/program origins, all joins, the four shapes, named/mixed/triggered waits
and immediate/delayed-NBA triggers. An original full-extent rejection input now
executes successfully. Its [exact oracle migration](automatic-event-expectation-migration.md)
requires separate human approval; its input and current expectation have not
been changed to hide the failure.

[Full local validation](../measurements/2026-10-26/instruction-events-validation.json)
has 17/21 passing steps. All four full preset builds succeed; dev/ASan/UBSan each
have 5,740 passes out of 5,743, release 5,741/5,744, with two known skips and one
failure each. Every failure is the unchanged historical metadata rejection
check awaiting approval. All policy, full format/tidy, generated counts and
existing-expectation checks pass. All 791 mutation anchors apply. Existing
fixture bodies, dump expectations and PerfCount oracles are unchanged.

[All 12 new compiled faults](../measurements/2026-10-26/instruction-events-controls.json)
are detected; compilation failures do not count as detection. Restoring exact
source bytes passes 288 related checks. Faults omit native roots/ancestors,
SSA, collection, return/input pins, or allow container/final collection,
continued canceled transfers, null construction and shared array defaults.

[Both 87-probe rechecks](../measurements/2026-10-26/instruction-events-comparison.json)
reduce Icarus-passing gaps from eight to seven. All previous passing judged
bytes and reference statuses remain; reference mismatches stay four/seven.
The [fixed population](../measurements/2026-10-26/instruction-events-population.txt)
remains 1,456/1,466 with zero failures/ten unsupported, and native-only
[sv-tests](../measurements/2026-10-26/instruction-events-svtests.json) remains
244/344. Reference sv-tests engines and Linux were not rerun.

The [five alternating ordinary-path pairs](../measurements/2026-10-26/instruction-events-speed.json)
observe +1.2% for frame/calls and +0.1% for static named events, with every
counter equal. Median peak RSS is 9.19 → 9.27 MiB and 9.19 → 9.19 MiB.
[Five runs per new-path size](../measurements/2026-10-26/instruction-events-scaling.json)
cover 1, 2,000, 20,000 and 200,000 declaration entries: scalar/three-element
array large-run medians are 0.0700/0.2690 s; median peak RSS is 9.1875 MiB at
every size. These are one-machine observations under PERFORMANCE 5, not a wall
clock pass/fail gate, confidence bounds or seven nightly baselines. Registry-row
bounds come from the VM tests; process RSS alone is not a row-count proof.

Root-publication scratch allocation uses existing
standard allocation and is caught as a resource limit before sweeping; a real
deterministic failure at every such scratch-allocation site is **not confirmed**.
Confirm those sites with an allocator-controlled publisher, preserving the
existing no-global-state/no-hidden-option rules. Registry and transfer failures,
failed-provider no-sweep behavior and Context allocation failure/reuse have
separate deterministic tests; they are not a substitute for that audit.

P1 remains incomplete. Event value/formal/structure coverage and other required
mechanisms in [P1_SUPPORT](../P1_SUPPORT.md) remain work; finite source probes
cannot establish full Icarus parity. These changes are local and have not been
merged into main or pushed.
