# Class identity, initialization and execution

[日本語](class-runtime-proposal.ja.md). Status: **pending review**, 2026-10-24.
This is a proposed contract, not implemented support or approval to implement.

## Why a detailed amendment is needed

The approved P1_SCOPE requires the Icarus-supported class subset. Existing
SIR_SPEC 4.2 names class layouts and dispatch tables but does not define their
instructions or ownership. RUNTIME_ABI 6 defers class roots; SYSTEM_TASKS 13
defers object RNGs. The [46-program investigation](class-execution-investigation.md)
finds native class execution gaps and omitted static-initializer side effects.
Simply accepting `new` or adding scalar fields would leave required lifetime,
initialization, dispatch and scheduling mechanisms unspecified.

Amend SIR_SPEC 2–5, 7–10, RUNTIME_ABI 4/6, VALUE_SEMANTICS 1.4/3.9/3.13/3.14,
SCHEDULER_SPEC 6/9/13/16, SYSTEM_TASKS 3.9/13 and RANDOMIZE_SPEC's object-state
outline. Add an implementation-defined policy entry for the choices explicitly
identified below. Strict slang, four-state defaults, shared-kernel ownership,
existing expectation protection and the approved P1 scope remain authoritative.
No dependency, installation or external C ABI revision is selected.

## 1. Class types and object identity

Front records each canonical class specialization elaborated by slang as an
immutable descriptor: ClassId, declaring scope, base-class chain, implemented
interface classes, fields, constructor stages and method/virtual-slot numbers.
Forward and recursive field declarations use placeholder IDs followed by
validated fixup. Traverse declarations and bound expressions in source order;
assign IDs on first encounter, never by pointer/hash iteration. Discover the
transitive descriptor closure before emitting executable bodies. Identical
canonical specializations share descriptors; distinct specializations do not.
Field hiding retains the declaring FieldId. Access rights, compatible types,
abstract/final restrictions, extern definitions and generic arguments remain
slang's decisions, with all required method bodies bound for diagnostics.

A Context owns an ObjectToken containing a generation-checked index; null is
0. Static handle type is retained separately from dynamic ClassId. Assignment,
upcast and successful downcast preserve identity. No pointer value, runtime
name lookup or native C++ type identification participates in dispatch.
Object handles in structures, fixed arrays and supported containers remain
handles; ordinary value copying aliases the same objects. Existing conditional
merge and equality rules apply, including typed null for differing handles
under an unknown condition. A non-null invalid/foreign token is an internal
fault, not a source null or an unrelated object after index reuse.

## 2. Static properties and initialization

Give every static property one ordinary typed slot per canonical declaring
class specialization and lexical owner. An inherited property uses its original
slot; a hidden redeclaration uses another. Slang's canonical scopes determine
whether declarations in separate elaborated instances denote the same class;
do not merge them merely by source spelling. No object allocation is required
to access static members. Non-parameterized declarations are visited even if
no instance, member access or method call refers to the class: initializer side
effects must run. Generic classes instantiate only specializations actually
elaborated by slang; this does not instantiate every possible parameter tuple.

Default-initialize all static storage, then run explicit initializers in the
existing pre-time-zero static-init phase. At the class declaration's position
in its enclosing scope, visit its own static properties in declaration order.
Parameterized specializations occupy their generic declaration's position in
the deterministic discovery order from section 1 (**project order choice**).
Packages/units and instantiated scopes keep the existing SIR_SPEC 7.1 order;
inheritance does not topologically reorder initializer dependencies. Static
method locals retain the existing lexical static-variable rules and are not
reinitialized per object/call. Apply the existing order-warning analysis to
class static reads too. Initialization effects, reports, forbidden scheduling,
stop checks and errors follow the existing static-init rules.

## 3. Construction, defaults and escaped receivers

Ordinary and explicitly typed `new` use slang's selected concrete class and
constructor/default/named arguments. Evaluate actuals once under the existing
argument order; allocate and root the result before entering initialization.
Give the new object its RNG as section 9 specifies. The constructor chain
executes baseward first. After that class's base constructor returns, initialize
only fields declared by that class in declaration order, then execute the
remaining body of its constructor. Source declaration initializers run once;
fields without them receive their type defaults. An implicit constructor uses
the same stages. `super.new`/extends arguments follow slang's binding and
8.7/8.17, not an extra call inserted after derived-field initialization.

Storage is internally initialized to typed safe defaults before source code
can observe it. A read of a not-yet-initialized derived property by an earlier
base stage is undefined by 8.7; this proposal returns that safe type default
(**project choice**, not a guaranteed IEEE early value). It must not execute
the derived declaration initializer early. Escaped `this` keeps the same
partially initialized object alive; no second constructor or later identity
replacement is created. Constructor functions cannot wait; strict legality
and existing function-scheduling checks apply.

`this` is a nonassignable implicit handle. A const handle does not make the
referenced object immutable. Declaration-initialized const fields can only be
initialized there. Instance const fields may be assigned once in their own
constructor (8.19). Preserve slang's lexical checks and track a per-field
initialization bit where a second executed assignment cannot be excluded
statically; a second assignment is a positioned design runtime error
(**enforcement choice**). No ordinary ref bypasses those checks.

## 4. Shallow copy

`new source` evaluates source once and duplicates the **dynamic referenced
class**, including derived fields through a base-typed source (8.12). It creates
a fresh identity without constructors or declaration initializers. Copy every
property by its value semantics: nested object handles alias; strings, fixed
aggregates and containers keep their existing value/deep-copy rules. Copy
const initialization bits and internal RNG/randomization state, including
future rand/randc/constraint modes. Embedded covergroup handles become null;
this contract does not implement coverage objects or a constraint solver.

Copying null is a positioned design runtime error (**project choice**; no
reference output is adopted as its oracle). A typed constructor form for a
shallow copy remains illegal. Successful copy gets new display/weak IDs.
Allocation failures release unpublished storage/roots, but never undo source
expression effects. Section 9 explicitly selects no parent RNG draw for this
special copy allocation.

## 5. Calls, inheritance and casts

Nonvirtual calls bind to the statically selected declaration. Virtual calls
use the dynamic object's immutable slot table, including overrides reached
through base and interface-class views. `super.method` directly selects the
base implementation; constructors and static functions are not virtual.
An abstract class cannot be directly allocated. Multiple interface-class
contracts do not create duplicate concrete object identities or base storage.
Methods have automatic invocation frames; this includes static methods.

Evaluate an instance receiver once before actual arguments and retain it
through argument evaluation, the whole call, suspension and copyback. This is
a **project evaluation-order choice** where not otherwise constrained. Check
null before executing the body, after argument effects. A static method or
property selected through a null handle is allowed (8.4); preserve effects of
any receiver expression. Instance-property access and nonstatic calls through
null produce a positioned design runtime error (**project choice** for the
otherwise indeterminate illegal access in 8.4), rather than fabricated values.

Class `$cast` follows 6.24.2/8.16: ordinary assignment-compatible source types
succeed, narrowing/interface checks also require a compatible dynamic object,
and literal null succeeds. An unrelated **typed** null is not literal null;
a null base handle does not supply a derived object for a narrowing cast.
Failure leaves destination unchanged and returns 0 in function form; task form
reports a design runtime error. Capture the destination as a ref-style target
before evaluating source, once each in argument order (**project choice**);
retain any target object until the operation ends, even on failure. There is
no failed-cast store or change notification.

## 6. Property targets, tasks and scheduled writes

A typed ObjectRef contains ObjectToken, declaring FieldId and typed selection
metadata. Kernel validation checks generation, dynamic compatibility, bounds
and write permission. Ref arguments pin the originally selected object/cell;
output/inout targets re-evaluate at copyback as existing SIR_SPEC 8 requires.
Delayed blocking assignments and ordinary/NBA stores preserve the existing
RHS/target timing rules; tiers must not re-select through a changed handle.
Object lifetime protection is independent of the lifetime of its source variable.
`ref static` must still meet slang's lifetime restrictions; pinning does not
convert dynamic property storage into a static-lifetime variable.

Do not apply the automatic-variable NBA prohibition to every class property:
6.21 forbids continuous/procedural-continuous drives of nonstatic properties,
but permits eligible property NBAs. Capture and strongly retain the target
object and RHS typed values through delay, delivery or cancellation (8.29).
Suspended receiver calls, refs, outputs and scheduled writes keep their origin,
process group and error/report scope; pinning an object does not keep an ended
issuer process running or rehome its work. Class-handle variables themselves
still follow their own static/automatic target restrictions. Event-counted
NBAs remain subject to the separately pending event-NBA contract.

## 7. Event dependencies and observation

Explicit `@`, edge/iff controls, `wait`, monitors and guarded expressions use
the existing value-change/edge rules. Dependencies on a property retain both
its source binding and current resolved ObjectToken/field/range; rebinding
re-evaluates and replaces the observed path. Unchanged normalized writes do
not wake a wait. The source read must be legal: a property access through null
follows section 5, not a silently disabled dependency. Observer metadata is
weak; actual frames, bindings and values determine strong reachability.
Extend the existing SCHEDULER_SPEC 9.5 private-call-local reachability filter
to typed object graphs too; externally aliased actuals are not private merely
because their handles are stored in automatic formal slots.

For `always_comb`/`always_latch`, preserve 9.2.2.2.1 and SCHEDULER_SPEC 6.3:
object references and object method calls add only their argument expressions;
static `Class::method` calls are analyzed like ordinary functions. Do not add
all object fields to this implicit sensitivity. For `@*`, follow the distinct
syntactic-read rules in 9.4.2.2: include a read property path and its handle
binding, and actual expression reads, without traversing method bodies
(**interpretation submitted for review**). The observed Icarus `@*` source
below never wakes; this is not adopted as a model. The proposed result for
that source is `7`, `9`, `7` after allocation, field change and rebinding.

## 8. Joint object/container reachability and weak references

Extend the existing typed heap graph across container-to-object and
object-to-container edges and object cycles. Root kinds distinguish object,
container, event and other managed tokens; never identify roots by bit patterns.
Root sources include static slots, live frames, receivers, temporary arguments,
return/copyback values, refs, pending NBA targets/payloads and deferred reports.
Detach observer metadata before reclaiming its owner; do not accidentally root
unreachable cycles through callback/watch lists. Collection runs only at
existing safe points and invokes no source destructor or user callback.

The built-in `weak_reference#(T)` has ordinary unique object identity and a
nonmarking referent edge. `new(null)` is permitted; `get()` returns a correctly
typed strong handle or null, rooted before a later safe point. `clear()` removes
only that weak edge. At collection, clear all weak references to each referent
without strong reachability atomically before reclaim/reuse (8.29/8.30).
Event-valued fields need per-object managed event identities and typed roots,
not a shared implicit event. Their construction/alias/copyback rules depend on
the separately pending [automatic-storage/event proposal](automatic-capture-proposal.md);
this amendment does not approve that proposal or enable its unfinished forms.
Shallow-copy event values preserve their handle identity.

No source test assumes collection immediately follows the last assignment;
use controlled kernel collection to test timing-independent guarantees.
`get_id(null)` is 0; other identities use a separate Context-wide nonzero
monotonic 63-bit identifier, invariant across base/interface views and never
reused in that Context (**project identifier policy**). It is not the per-class
`%p` ordinal or a native address. Exhaustion is a resource failure, not wrap.

## 9. Object RNG state and formatting

Use the already approved SplitMix64 state/step representation. For ordinary
object creation, after actuals and successful storage reservation, consume
exactly one full seed from the creating process RNG, or the declaration's
lexical initialization RNG during static initialization. Install that state
before any property initializer/constructor body runs. Method `$urandom`
still draws from the calling thread, not automatically from the receiver.
A shallow copy copies the source object's RNG without drawing a new parent
seed (**explicit project choice** for the special allocation in 8.12).
The additional original seed probe matches this choice in both references.

Object `srandom(seed)` uses the unsigned low 32 bits as state, consistent with
the existing explicit-seed convention (**project choice**). Reserve storage for
future rand/constraint state, but neither silently treat `randomize` as success
nor introduce a solver. Serialization formats for get/set_randstate, process
state methods, constraints and coverage still require their own detailed
contract before activation. This dependency does not defer any required
Icarus-supported mechanisms out of the P1 objective.

Apply the existing SYSTEM_TASKS 3.9 `%p` class-name/per-class-creation-ordinal
policy, with counters owned by Context. Generic aggregate formatting remains
a distinct required mechanism, not automatically enabled by object handles.
Method `%m` and reports use front's declaration/call scope; objects do not
invent hierarchical signal instances. No VPI/DPI raw token exposure or new
external ABI is implied; their typed rooting interfaces need separate review.

## 10. Shared representation, failure and verification

SIR retains typed class descriptors and operations for allocation, staged
construction, copy, property values/refs, calls/casts and runtime guards. Its
verifier checks descriptor closure, base acyclicity, slot/override signatures,
field ownership/layout, static/instance distinction, ref/write permissions,
constructor stages, typed roots, counts/overflow and allowed targets. Text
dumps expose IDs and ownership. Invalid SIR fails before allocation/execution;
ordinary methods cannot issue privileged constructor writes by forging a flag.
Use a kernel-owned constructor-stage capability tied to the object, class
stage and invocation; verification restricts its creation/use to that stage.
Both execution tiers mechanically call one shared kernel implementation. Kernel
has no dependency on slang/SIR/LLVM. New public headers compile as C++20 and
are registered in the existing header test; generated code remains C++20
without exceptions/native runtime type identification.

Preflight sizes and checked IDs, use generation validation and return explicit
allocation/resource failures through existing exit-code rules. An interrupted
or failed constructor unwinds its private roots/frames and stops by the existing
design/runtime rules; escaped objects are not invalidated behind live handles.
Never roll back already performed source side effects. Preserve Context
isolation, deterministic descriptors, immutable design tables and existing
cancellation/final/report scheduling.

## Evidence and acceptance plan

The earlier [46 observations](../measurements/2026-10-24/class-design-observations.json)
remain byte-identical. [Fourteen additional original boundaries](../measurements/2026-10-24/class-contract-boundaries.json)
record native banner `3b2560f` (C++ source unchanged since `52652eb`) and
cast-null categories, const assignment, property NBA ownership, explicit
and implicit events, unused module/package statics, initialization order,
class sharing, shallow-copy parent RNG and weak references. Each reference has
four model matches, six compilation failures, three runtime failures and one
unjudged `@*` observation. Native has twelve unsupported forms and two output
mismatches. The static module source is already rejected; package/unit sources
can omit side effects and exit successfully. Reference cast failures/assertions
are observations, not approved oracles. These 14 are separate from the 46 and
the established 87-probe inventory; full coverage and Linux remain unconfirmed.

Implementation after approval must proceed through the whole contract, not
stop at a basic class smoke test. Add independent valsem identity/copy/default
models, kernel lifetime/dispatch/RNG/weak-root tests, malformed SIR/verifier
cases and original source matrices across module/package/unit/generate/interface/
program/method/task/fork contexts; widths 1/32/65/129/257 and two/four-state,
real/string/aggregate/container fields; base/interface/generic views; blocking,
NBA, ref/copyback, suspension/cancellation, reports, unknowns and null/cast edges.
Verify GC cycles, old-token reuse, pending-target roots, escaped construction,
const writes, allocation failure and seed consumption independently of references.
Compiled negative controls must break these mechanisms while leaving older
unrelated tests intact. Preserve existing suite/expected/performance bodies;
retire obsolete rejection expectations in separate review branches. Run all
four build/test configurations and required lint before any later push.
Expand finite comparison inventories with versioned results and provenance;
passing this contract alone does not establish full P1/Icarus parity.

## Proposal verification

This documentation-only step passes all 136 Tools tests, README counts, source
hash/model-output/totals checks, unchanged prior 46-case evidence, file links,
privacy and whitespace checks. Native production code and existing test
expectations are unchanged. The previous four-configuration and LLVM-22 lint
results remain recorded in the investigation; they were not rerun as new
class implementation evidence.
