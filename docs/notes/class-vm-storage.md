# Class runtime: VM storage adapters

[日本語](class-vm-storage.ja.md)

This continues the [class SIR checkpoint](class-sir-runtime.md) under the
[approved class contract](class-runtime-proposal.md), sections 1, 3–6, 8 and 10.
It connects immutable SIR storage descriptions and retained values to the shared
kernel. **Class instruction execution and the frontend remain unconnected.**
The VM's existing pre-allocation rejection of class SIR remains in place. This
is not completion of source class execution or the P1 class requirement.

## One heap schema

`vm::detail::HeapPlan` translates SIR's container and class descriptions. The
interpreter's existing `prepareHeap` now uses this same plan for ordinary
containers. Container IDs are recorded separately from SIR type-row numbers;
class IDs preserve descriptor order, including recursive field types and bases
whose IDs follow their derived classes. Numeric virtual method slots, declaring
fields, const/initializer flags, interfaces and weak target classes pass to the
kernel without runtime source-name lookup.

Fields repeat their typed element layout in fixed-array declaration order.
Integral layouts retain two/four-state defaults and exact final-word masks;
class and string leaves carry their distinct tracing markers. Nested containers
retain their registered types. Managed event fields are rejected before schema
registration because their independent event identities and roots are not yet
connected; they never become opaque integer fields.

The plan preflights all relevant schema rows before registering layouts. A
resource failure can leave unused immutable layout/type rows; it publishes no
plan, class table, source object, cell, container, pin or random draw. The same
complete schema can be retried after that failure. Kernel class registration
is atomic, and no fallible plan operation follows it. The design and Context
must outlive the plan.

## Typed values and retained targets

`localObject` and `objectValue` translate between typed Context-local VM words
(`a=token`, `b=0`) and kernel tokens. Export validates the token's original
Context owner, generation and compatible static class view before discarding
the internal owner tag. A raw local word belongs to its supplying Context;
its bits alone do not establish cross-Context ownership. Object-valued heap
words acquire their validation tag only at the typed boundary. No tag is
exposed as a source value, identifier or dispatch key.

`ObjectProperty` holds the originally selected declaring field and an object
pin. Rebinding a source handle does not reselect it; moves transfer ownership
and invalidate the old access. Checked linear element selection preserves the
field's type and word interval. Reads and writes translate all class elements
before publishing the destination. Wrong-class/stale/noncanonical values or
allocation failures commit no prefix. The kernel remains the authority for
normalization, container value copying, readonly access, constructor capability
and const writes. These accesses require the owning Context to remain alive;
they retain storage, not a process, execution location or callback.

`ObjectTransfers` pins class-valued scalar/array argument, return or copyback
buffers. Failure unwinds the new group and preserves earlier groups. Rollback
only pops pins and cannot allocate. Opaque words and ref/const-ref tokens are
not interpreted as object values; a caller must separately preserve the
resolved reference's typed storage.

`ActivationPlan` now maps class fields to the kernel's Object kind with the
static class view, after the heap schema is registered. Kernel validation and
root enumeration apply to scoped fields and fork captures. A child keeps the
original binding after its parent's execution ends, for all join modes, and
writes cannot change its class view.

## Verification and remaining integration

Eighteen new adapter tests cover widths 1/32/65/129/257 in two/four-state modes,
fixed arrays, forward classes, independent container IDs, dispatch IDs,
recursive object graphs, nested-container copies, string edges, opaque handles,
foreign/stale tokens, moved/retained targets, const stages, failed transfers,
scoped/captured bindings and injected failures at every schema allocation.
The tests execute the real shared heap and activation store without the
frontend. Seventeen new compiled negative controls target these adapter
mechanisms. The existing container-default control moves to the shared plan
and still corrupts only container defaults.

The complete class SIR still stops before runtime allocation. Class opcode
execution, constructor/method invocation, and all interpreter root/payload
publication paths must be connected before that boundary is removed. Property
and container-element ref/bit/NBA lowering, managed events, dependency
observation, source casts, formatting and canonical slang discovery remain as
listed in the SIR checkpoint. Existing rejection tests, suite bodies, expected
outputs and performance budgets are unchanged; feature probes and the P1
population have not been remeasured for this storage-only change.
