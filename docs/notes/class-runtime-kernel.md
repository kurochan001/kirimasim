# Class runtime: kernel checkpoint

[日本語](class-runtime-kernel.ja.md)

This implements the kernel portion of the [approved class contract](class-runtime-proposal.md),
sections 1, 3–6 and 8–10. It does **not** enable source class execution or
establish Icarus parity. The frontend still rejects object construction.

The later [class SIR checkpoint](class-sir-runtime.md) adds descriptors,
instructions and verification. Its execution adapter and frontend remain pending.

## Representation and ownership

`Heap::addClasses` atomically installs immutable numeric class descriptors,
including recursive field types, base/interface compatibility, declaring-field
identities and design-wide virtual slots. IDs need not follow inheritance
order. Interface diamonds and long base chains use iterative traversal;
cycles, incompatible declarations and invalid layouts fail before publication.

Objects share the existing typed heap with containers and stable property
cells. Typed leaves distinguish strong object edges from ordinary integers.
Handles preserve identity through assignment and nested-container copies;
shallow copies duplicate value-owned containers, preserve object aliases,
copy const/RNG state and allocate a fresh monotonic logical ID and per-class
ordinal without executing initialization or drawing another parent seed.

An object token includes a generation and its live owning heap. That owner
validates C++ boundary values; it is not a language identity or dispatch key.
Heap object words carry the owner validation tag. Automatic activation fields
carry only Context-local token bits, with a separately registered class view;
the activation store validates writes and enumerates typed object roots.
No native address is exposed as a source value.

Move-only pins protect unpublished values and retained operations. A selected
property cell roots its entire owner. Marking traverses object/container cycles
and string leaves; observations are weak. A failed mark reclaims nothing.
Weak promotion pins before a later safe point; all dead weak edges clear
before any pool entry can be reused. IDs are never pool indices or reused IDs.

## Construction and property operations

All storage receives safe typed defaults before publication. A move-only
kernel capability identifies one invocation of one base-first constructor
stage. It distinguishes declaration initialization from the constructor body,
checks required initializers and enforces each const field's one allowed
write. Abandoning construction preserves escaped objects and completed source
effects, while preventing a later invocation from resuming that stage.

Property refs identify the original object and declaring field. They are
borrowed views: an adapter must hold a pin or publish a typed root before a
safe point; copying an `ObjectRef` alone does not acquire lifetime ownership.
Word and bit
selections validate bounds, preserve unselected values and reject selections
through typed handles. Writes normalize before change notification. Casting
distinguishes assignment-compatible static views, narrowing/interface checks,
typed null and literal null; a failed cast performs no store.

## Verification and remaining integration

Twenty new kernel tests cover widths 1/32/65/129/257, two/four-state values,
copy/alias graphs against an independent `valsem` model, defaults, dispatch,
casts, constructor capabilities, weak roots, Context isolation, stale tokens,
allocation failure, seed consumption, repeated collection and retained
automatic frames. Seventeen compiled negative controls target these rules.
The object test executable links only kernel, valsem and GoogleTest; the public
headers also compile as C++20.

SIR class descriptors/instructions and verification, canonical slang discovery,
constructor/method lowering and VM root/operation adapters remain to be
implemented before any source support claim. Managed event fields, source
property observers/ref/NBA lifetime integration, source casts, formatting and
the full source-context matrices remain in that work. Existing rejection tests
and expected values are unchanged. This checkpoint does not complete the
approved contract or the P1 class requirement.
