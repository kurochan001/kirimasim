# Class runtime: SIR checkpoint

[日本語](class-sir-runtime.ja.md)

This continues the [kernel checkpoint](class-runtime-kernel.md) under the
[approved class contract](class-runtime-proposal.md), sections 1, 3–8 and 10.
It implements SIR's class representation and verification. **It does not
enable source class execution or implement the VM execution adapter.**
The approved class contract and the P1 class requirement remain unfinished.

## Types and descriptors

`class<!cN>` records a static class view independently of the dynamic object.
`ClassId` is separate from the existing container `TypeId`. Immutable class
rows retain declaring scopes, bases, interfaces, final/abstract restrictions,
own fields, fixed dimensions, const/initializer flags, static slots, constructor
wrappers, weak target types and design-wide virtual tables. Recursive field
types are valid; inheritance cycles are not. Fields hidden by a derived class
keep different declaring IDs.

Virtual slots carry signatures independently of executable implementations,
so an interface needs no fabricated method body. Tables bind numeric slots to
method IDs. Verification checks owners, lowered parameter types/dimensions,
directions, suspension form, return compatibility and inherited overrides.
Task suspension flags are normalized consistently within a slot's implementations;
`mayWait` permits suspension rather than promising that a particular call waits.
Static methods have ordinary automatic calls and no implicit receiver.

Class constants are typed null only. Class metadata cannot be an ordinary
value operand, and arithmetic, untyped refs, arbitrary builtins and system
calls cannot manufacture class handles. Container values can contain class
handles, with their element types retained. Old designs emit no new text;
new dumps expose class/field/slot numbers and constructor ownership.

## Construction and calls

An instance method's first input is its nonassignable receiver. A constructor
wrapper is an automatic nonwaiting function registered by its declaring class.
It can be entered only by allocation or direct-base initialization, rather
than an ordinary call or deferred report. The new instructions express
allocation, copy, compatible views, cast checking/values, IDs, seeds, property
reads/writes, method calls and weak operations.

Constructor phases are implicit invocation state, never a source value or
forgeable SSA capability. A control-flow analysis requires direct-base
construction, own-stage entry, declaration initialization, body entry and stage
completion in that order. Required field initializers must execute in declaration
order and dominate body entry on every incoming path; fields without source
initializers cannot receive declaration writes. A normal return must complete the stage.
Initializer writes address only that invocation's own declaring fields and
provide all fixed-array elements in declaration order. Const fields have
distinct declaration/body permissions. The kernel's existing capability and
per-field bit will enforce executed-once writes when the adapter is connected.

Virtual and direct method calls remain distinct. A waiting call needs its task
form and continuation, with the existing prohibition on SSA use across waits.
All argument-expression effects belong before the instruction. The frontend
must retain a receiver evaluated before actuals and preserve the approved null
check/copyback timing when it emits these instructions.

`object.cast.check` distinguishes literal null from a typed null. The internal
value instruction returns typed null on failure; the language producer must
use the check to guard destination writes, preserving `$cast` failure semantics.

## References and receiver ownership

`ref<class<!cN>>` and `ref.readonly<class<!cN>>` preserve the class view and
write permission. A readonly reference cannot become a mutable formal or
perform ordinary/NBA stores. Scalar, array and selected-element references
retain their shapes through their producing instructions, bindings and every
incoming block argument. This prevents scalar/array reinterpretation without
guessing from token bits.

Receiver rights propagate through guard frame aliases and fork captures via
the existing lexical ownership graph. Validation uses a fixed point with
numeric field IDs, avoiding native recursion and repeated per-instruction
searches through subroutine tables. Stores, mutable refs and copyback cannot
rebind an implicit receiver even after it is captured.

## Execution boundary and remaining work

The VM detects class metadata before interning constants, creating slots,
registering activations or allocating objects. Malformed inputs receive
`invalid class SIR`; valid class SIR receives an explicit unimplemented-adapter
load error. Both use the VM API's existing internal-load-error result. The
frontend's existing unsupported-source diagnostics remain unchanged.
This boundary prevents a new opcode or unrooted class value from silently
running through an older execution path.

Canonical slang discovery, construction/method lowering and the shared-kernel
execution adapters remain to be connected. That work must cover every typed
holder, temporary, pending payload, receiver and cancellation path before
removing the boundary. Class-property refs, class container-element refs,
property bit/NBA target instructions, unpacked-record field representation,
managed-event field roots, observers, formatting and the full original source
matrices also remain. Class container-element refs are explicitly rejected
rather than emitted as untyped references.

## Verification

Twenty-eight producer-independent SIR tests and two VM boundary tests cover
widths 1/32/65/129/257, two/four-state fields, recursive field types, interface
diamonds, a 512-class reverse-ID chain, malformed metadata, virtual signatures,
constructor control flow, hidden owners, casts, direct/waiting calls, typed
references, readonly/receiver rights and allocation-free rejection. The SIR
test executable links only SIR and GoogleTest. Twenty-three new negative controls
target the mechanisms; the older event-null text control retains its original
fault after its anchor is adjusted for class null text. Existing suite bodies,
expected outputs and performance budgets are unchanged.
