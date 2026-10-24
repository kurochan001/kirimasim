# Shared automatic storage, fork capture and automatic events

[日本語](automatic-capture-proposal.ja.md). Status: **pending review**,
2026-10-24. The English text is the proposed contract. No production
implementation is authorized by this document alone.

## Why the existing contract needs amendment

The [implemented fork-declaration path](fork-local-declarations.md) initializes
declarations in the parent but still rejects automatic variables used by a
child. SIR_SPEC 7.5's planned per-child initialization cannot define shared
variable identity: the original shared-variable probe prints 9, whereas an
original program using independent child variables prints 7. Both references
agree on those two programs. Retaining one call frame is also insufficient:
reentering an automatic block needs a new activation while earlier children
can still refer to its previous activation.

Amend SIR_SPEC 3.2.1/3.3/7.4/7.5/8, SCHEDULER_SPEC 9.4/9.5/10.7/10.8 and
RUNTIME_ABI 4.3/6 under [P1_SCOPE](../spec/P1_SCOPE.md). Include automatic
scalar/fixed-array event construction, because both installed references run
the original automatic-event examples while native execution rejects them.
No dependency or installation is selected. Existing static initialization,
four-state value rules, copy order, strict slang diagnostics and program
completion remain authoritative.

## 1. Variable identity and activation extent

Every entry into a lexical automatic scope creates a distinct activation.
All accesses to one variable of that activation, by its owner and captured
children, refer to the same storage. A child receives a binding to that
storage, not a value snapshot, independent clone or reference to the parent's
current binding-table entry. Consequently:

- Siblings observe each other's writes under the existing scheduling rules.
- A parent write after `join_none` is visible when the child later reads.
- Repeated/recursive calls and repeated block entries have separate storage.
- A loop control variable has its loop-scope lifetime; a body-local or
  fork-local automatic declaration has a fresh activation on each entry.
- Nested children forward the original binding, not a new copy of its value.
- Static variables keep their existing Context-local slots and lifetime.

Initialization follows the current declaration and argument rules. A fresh
allocation's safe internal contents are not an extra source-language
initialization. Front emits each default/initializer once in declaration order;
fork block item initialization completes before children become runnable.
Preserve the reviewed pre-time-zero static initialization policy. A class
method, when implemented, uses these same automatic activation rules.

Following 6.21, lifetime extension applies to the automatic scope and its
fields, not only to the particular variable a child reads. Keep the fork's
automatic scope and its lexical enclosing automatic activations through the
relevant spawned activity. Forward extent ownership to nested descendants so
ending an intermediate child cannot release an enclosing activation early.
Dynamic calling alone does not alias a caller's locals into a callee: the
front records lexical environments separately from process/control ancestry.

The executing owner can leave its block or return from a task/function while
that storage remains. Retaining storage does not retain execution: a returned
call does not resume again, a finished process stays finished, and the kernel's
existing `wait fork`, join membership, disable ancestry and program-completion
counts do not include storage records as processes (6.21, 9.3.2, 24.3.1).

## 2. Context-owned storage and leases

Add a shared-kernel automatic-storage store. Immutable layouts contain field
types, widths, normalization, fixed dimensions and typed root descriptors;
they contain neither slang nor SIR types. A Context-local generational scope
token identifies an allocation, and a binding identifies that token, field,
selection and access rights. No SystemVerilog value stores a native address.

Ownership is explicit:

1. Entering a promoted scope gives the executing activation an owner lease.
2. Fork registration resolves each child's capture bindings and acquires
   leases for distinct storage/extent tokens before any child starts.
   Several fields from one scope do not create separate physical scopes.
3. Normal scope exit/return releases the executing owner lease. Child and
   descendant extent leases keep the allocation until their activity ends.
4. A call, pending callback or registration that legitimately keeps an owned
   binding retains the corresponding lease. A borrowed ref is not silently
   converted into an indefinitely owning source-language reference.
5. Detach waits/callbacks and release their bindings before their interpreter
   frames expire. Reclaim a storage record after its last owner is gone.

Do not mirror mutable values in both a parent frame and the store. Ordinary
frame rows remain ordinary storage; promoted rows and imported capture views
resolve to the shared store. A logical variable has one current value. Layouts
must retain all typed fields for an extended scope even when only one field
appears in a child's expression.

Validate generations, bounds, layouts and access rights on binding use. Reuse
records only with a new generation; retire them on generation exhaustion.
Context reset invalidates earlier handles. Tokens are Context-local, not
portable values. Typed C++ leases retain their owning store identity; no
source operation transfers a binding between Contexts. Do not infer validity
or roots from an arbitrary integral bit pattern.

This changes ownership of automatic data, not SCHEDULER_SPEC 10.8's logical
process reclamation conditions. Task-call temporaries/control records may be
released after return once their existing obligations finish; retained scope
fields do not need an intact VM call stack, PC, RNG or user thread.

## 3. Parameter, ref and copyback rules

Evaluate actuals in the existing order, create the callee's root storage before
parameter copy-in/body execution, and preserve input/output/inout semantics.
At return, perform the existing copyback **once**, then release the call's
executing scope ownership. A child that later changes a retained output
formal does not perform another copyback or change the caller's returned value.
The original `copyback_once` model therefore keeps caller value 7 while the
child later observes its formal as 9. Event output/inout copyback into an
already initialized event variable would rebind that variable; it remains
unsupported pending the separate event-rebinding contract described in section 5.
The generic copyback rule here does not authorize that event form.

Keep the existing borrowed-ref validity and const restrictions. Under
9.3.2/13.5.2, a normal ref formal cannot be used inside `join_any`/`join_none`
children except in fork declaration initializers. Capturing a local value
initialized from that ref is permitted and snapshots it using the usual value
copy rules. `join` can forward a permitted borrowed ref while its receiving
call remains active. Do not bypass the restriction merely because a particular
runtime actual happens to occupy a static slot.

Preserve `ref static` qualifiers in SIR parameter/binding metadata. Strict slang
already accepts the original static-actual example and rejects the automatic
actual. A permitted `ref static` binding can be forwarded to a later child;
it names the underlying static actual, with the original readonly rights.
Do not keep a pointer into a dead formal-argument frame. Invalid-index behavior,
typed container receiver retention, detached cells and copyback-time selector
evaluation retain their existing contracts.

## 4. Captured changes and synchronization

The kernel store provides typed read/change observation keyed by storage
token, field, element and bit range. Notify after committed normalized changes,
not failed writes, unchanged values or invalid selections. Reads/writes and
edge detection reuse existing kernel/value semantics, including wide X/Z
values and bounded packed parents.

Extend the scheduler's existing slot/heap dependency path with these storage
dependencies. A guard/watch reattaches after its selected binding or index
changes. `wait(value == 7)` must wake after another child changes the captured
value even though no static slot changed. A callback uses the waiter's owned
bindings, not a borrowed pointer to the returned parent's frame. Use the same
ordered late-change queue and existing guard/`iff` evaluation rules.

For supported implicit sensitivity on scalar/fixed storage, front carries the
static read ranges, including unexecuted branches and the existing comb/latch
write exclusions. Resolve those ranges against the activation bindings;
runtime reads alone must not replace the static implicit sensitivity set.
The independent implicit-container-sensitivity contract remains unfinished:
this amendment does not silently enable a container form currently diagnosed
as unsupported.

Container variables in captured fields participate in both dependency levels:
the storage binding observes replacement, and existing heap hooks observe
structure/cells. Capture the variable binding rather than the heap handle that
happens to occupy it at fork time. Replacing a queue must redirect subsequent
reads/method receivers to its then-current value. Existing array/queue value,
reference and deep-copy rules remain unchanged.

## 5. Automatic event construction and ownership

An automatic scalar event without an initializer gets a fresh event object
when its declaration executes (6.8). Each fixed-array element gets its own
object per activation, with the existing dimension order. Capturing the event
variable shares that variable and its event object; it does not construct a
new object per child. Different call/block activations do not share a queue.
Static event defaults remain one object per declared static element.

Add a Context-owned managed-event registry mapping an opaque, generation-checked
event value to the scheduler event object. Null is zero. Track event values
held in typed storage, supported input arguments, owned temporaries and
pending operations/registrations. One captured field is one physical holder,
regardless of how many children view it. Copying a supported event input or
initialization value shares the event object; it does not copy its queue.

Release objects when no typed holder or runtime pin remains; pins protect
in-flight callbacks and pending triggers. Event waits/registrations can hold
runtime pins without creating source-language variables. Cancel/detach them
before releasing the corresponding event record. Reused objects clear all
triggered state and queues and receive a new generation. No unbounded
per-declaration event allocation may accumulate after the activity ends.

Extend trigger/wait operands to accept a loaded event value, including captured
fields and runtime fixed-array selections, while retaining legacy slot forms.
Evaluate the event expression once at registration. A nonblocking trigger
owns the concrete event value until delivery/cancellation under the existing
pending-update rules; returning from the issuer does not dangle it or create
a hidden child. Preserve Active/Reactive origin and NBA/Re-NBA behavior.
For new dynamic trigger operands, propose target evaluation before the optional
delay, matching the existing instruction construction, with each evaluated once.
This evaluation-order choice is explicitly part of the requested review.

`.triggered` observes the selected event object's current-slot state. Its
dependencies include that event object, not only the variable's unchanged
handle. Resetting the triggered state when time advances does not wake waits,
as in existing SCHEDULER_SPEC 9.4. Changing an index or a variable's association
alone does not constitute a named-event trigger.

For the new dynamic/null operand cases, specify `-> null` and `->> null` as
no-ops and `.triggered` on null as false (15.5.3/15.5.5.2). Evaluate supplied
trigger/delay expressions once even for a null target; do not schedule an event
for it. **Project choice:** waiting on null reports a positioned design runtime
error with exit 2; the standard leaves that waiting behavior undefined. An
invalid/stale nonnull registry token is an internal validity failure, not null.

This does not enable rebinding/merging an already initialized event variable,
event comparisons, event-containing dynamic containers or VPI event handles
without their separate value/interface contracts. Both references reject the
original event-assignment boundary program. Required type/interface coverage
remains part of the overall goal. Event input/initialization copies in this
section occur before the new variable's waits; arbitrary existing-wait rebinding
is not assigned a new policy by this amendment.

## 6. Typed roots, lifetime and cleanup

Register live retained storage with the shared typed root publisher, including
container values, detached cell refs and legitimately owned ref bindings.
Capture views are not independent deep copies or duplicate physical fields.
Normal integers must not become roots. Class/object and other future field
kinds require their own real root provider before source execution is enabled;
opaque unknown fields must not be treated as safely untraced.

Root-only lease changes make collection eligible at the existing safe points,
even without a heap mutation. Never collect while source/guard code, argument
marshalling or a store transaction still has unpublished temporaries. Enumerate
roots without allocating or executing source callbacks. Freeing a scope drops
its value roots and event associations; it does not run a SystemVerilog task.

On kill/disable/return, unwind executing scope ownership exactly once.
Cancel waits and detach callbacks before dropping their binding leases.
Other live descendants/registrations retain their own leases. Preserve settled
report obligations and already-owned pending update behavior. Do not retain
the issuer's thread merely because storage or an event operation remains.
Program completion still ends descendants according to the approved program
contract; leases cannot keep an otherwise ended program running.

## 7. SIR and tier/kernel division

Add immutable automatic-scope layouts and frame-row metadata distinguishing
ordinary rows, scoped fields and imported capture views. Each body has its
root-scope layout, and lexical block scopes have local IDs. Root-call storage
is created before parameter copy-in; block storage is created on each entry.

Add `activation.enter #scope` and `activation.leave #scope` for block ownership.
The current binding lives in frame state, not an SSA temporary across a wait.
Reentering a scope replaces its current owner binding with a fresh token;
existing child captures retain the previous token directly. Reads/writes use
the same logical frame-variable operands, dispatched by their declared home.

A child process row records typed captures from the fork parent's logical
bindings and the lexical extent scopes it inherits. Resolve captures once and
copy bindings into the new child's environment before starting it. Forward an
inherited binding directly. The verifier rejects capturing an unpromoted or
unavailable source row, incompatible types/shapes, writable views of readonly
actuals, forbidden ref lifetimes and unrelated/foreign layouts.

Reference instructions gain a scoped-storage home. Store ownership/lifetime
metadata rather than a pointer into VM vectors. Add `event.new` returning a
typed event value, and typed value forms of the existing named wait/trigger
operations. A deferred operation copies values/bindings; it never borrows an
SSA temporary after registration. No extra source-language process is created.

Verify balanced scope ownership on CFG edges and unwind scopes on return,
break, continue, disable and termination. Existing control-scope records carry
the automatic-scope depth needed to unwind; scope extents retained by children
are distinct from the executing owner's stack. Text form deterministically
prints layouts, homes, capture sources, readonly/static-ref qualifiers and
scope instructions, with malformed-SIR and roundtrip checks.

The shared kernel owns storage identity, leases, publication, managed event
objects and typed root access. Tier 1 mechanically transports SIR operands;
future generated C++ uses the same C++20 kernel API without exceptions/RTTI.
The kernel imports no slang, SIR or LLVM. Add all new public headers to the
existing C++20 header test. This selects no new external C ABI version.

## 8. Exactness, resources and acceptance

Validate field dimensions/offsets before allocation. Fork registration acquires
all capture/extent ownership transactionally before making children runnable;
failure returns the existing resource result and releases partial records.
Do not rollback already executed initializer side effects. Propagate stop and
resource failures through the kernel boundary without exceptions.

Memory for released scopes/events is reused and proportional to retained
activations/values/operations, with generation exhaustion handled explicitly.
No global registry, runtime signal-name lookup, hidden fork child or extra RNG
draw is permitted. Keep proven ordinary fast paths for nonescaping scopes;
do not optimize by reusing storage still referenced by an earlier activation.
Existing performance expectations cannot be changed to conceal differences.

Acceptance covers shared siblings and forwarding, every join kind, parent
writes before suspension, independent calls/recursion/block iterations, loop
index identity, wide/real/string/fixed-array values, typed container collection
and replacement, explicit watches and supported implicit ranges, copyback once,
permitted/forbidden refs and `ref static`, automatic event identity and triggered
state, deferred triggers, cleanup/disable/program completion, stale generations,
Context isolation, allocation failure and interrupt paths.

Use independent kernel/reference models, SIR and original source matrices.
Compiled controls must expose snapshots instead of bindings, reused iteration
storage, premature release, missing roots/notices, repeated copyback and stale
event reuse through actual new outputs or real validity failures. Keep judging
self-check failures separate. Existing expectation changes require their own
reasoned review/PR; approval here is not blanket permission to edit tests.
Run full local CI before any implementation push.

## Evidence and remaining limits

[Original sources and observations](../measurements/2026-10-24/automatic-capture-design-observations.json)
record 32 programs, both installed reference versions and current native
parse/execution results. Two deliberately illegal ref programs are rejected
by strict slang; the other programs parse. The event-rebinding example is an
explicit boundary outside this proposal. Model stdout is a proposed contract
check, not an approved test oracle. Reference runtime failures do not override
6.21, 9.3.2 or the existing copy/value rules, and no simulator source/tests
were read. Of the 31 non-boundary programs, reference 13.0 matches 11 proposed
outputs, fails at runtime on 13 and rejects seven at compilation; development
matches 12, fails at runtime on 12 and rejects seven. These totals include the
two deliberately illegal ref cases. A plain nonblocking automatic-event trigger
separates unsupported `->>` from the `.triggered` member boundary.
The expanded 82-probe comparison is recorded separately in the progress note.
Full P1 coverage remains unproven.
