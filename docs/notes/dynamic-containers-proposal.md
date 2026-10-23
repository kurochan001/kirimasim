# Dynamic array and queue execution amendment

[日本語](dynamic-containers-proposal.ja.md). Status: proposed, awaiting approval
(2026-10-23). No implementation or specification change is authorized by this
proposal alone.

P1_SCOPE advances the Icarus-required dynamic containers into P1. SIR_SPEC
currently describes their default empty handles but not their instructions;
RUNTIME_ABI 6 leaves heap and collection details unwritten. This amendment
provides that missing execution contract. The target remains the full P1
coverage objective; this is the next mechanism, not a replacement target.
The referenced clauses were checked locally. All descriptions are paraphrases;
no standard text, simulator code or external test was copied.

## Values and operations

- Dynamic arrays and queues use runtime length and zero-based indices
  (7.5, 7.10). The default value is empty. Integral elements retain their
  existing width, signedness and two-/four-state meaning; real, string,
  fixed aggregate and nested-container values retain their own approved
  value semantics as those element types are implemented. Class elements
  depend on the later class-object mechanism and are not silently enabled.
- `new[size]` evaluates its size before its optional initializer. Convert
  the size to the required signed 64-bit integral domain. Zero produces
  an empty array; negative size is a design runtime error, code 2 (7.5.1).
  **Project choice requiring approval:** an X/Z-containing size also gives
  a design runtime error, rather than guessing zero or allocating an
  unknown amount. An initializer contributes the common prefix; additional
  elements receive their type's default. Growing or shrinking always makes
  new element identities, including a same-size resize.
- Whole-container assignment and input/output value arguments copy element
  values. They do not make two container variables share mutable elements.
  Nested container values are copied recursively; object handles, when
  implemented, retain class-handle assignment semantics. A queue assignment
  obeys its destination bound. Self-assignment snapshots the source before
  replacing destination elements (7.5, 7.6, 7.10.4, 13.5).
- Implement size/delete and queue insertion, deletion, push and pop methods
  under 7.5.2, 7.5.3 and 7.10.2. Queue indexing, slices, concatenation,
  foreach, array queries and assignment patterns use the same element
  representation; they must not get independent value rules. Forms not yet
  implemented remain explicit diagnostics during incremental delivery.
- Invalid indices return the element's default on reads and leave storage
  unchanged on writes; queue append at `[$+1]` follows 7.4.5. Empty pops
  return the element's default. Methods apply their specified invalid-index
  behavior, rather than treating every method index as an ordinary write.
- A bounded queue retains only indices within its declared bound and emits
  the required warning when an operation loses excess elements (7.10.5).
  **Project diagnostic choice:** use the existing source-position warning
  policy for repeated executions; include empty-pop and invalid-method-index
  warnings where the standard requires or permits them. Do not turn those
  recoverable cases into a fatal error.
- **Project evaluation choice:** retain the receiver's variable binding
  before evaluating arguments, evaluate arguments once from left to right,
  and perform the mutation on that variable's then-current value. Argument
  side effects cannot cause an index expression to run twice. This is
  identified as a project rule where the standard leaves ordering open.

## References and element identity

A reference to an element names a stable element cell, not its current
numeric index or an address inside a reallocating vector (13.5.2, 7.10.3).

- Queue insertion and movement preserve the cells of existing retained
  elements. A ref still names that element after its index changes.
- Removing an element, replacing a whole container, or resizing/deleting
  a dynamic array detaches old cells. A ref held by an active subroutine
  keeps its detached cell alive and can still read or modify it. Those
  modifications do not affect the replacement container. An outdated ref
  is not an invalid-index read and must not be converted to X/default.
- A ref to the container variable itself keeps referring to that variable,
  including its replacements. Const-ref restrictions remain enforced.
  Fork capture, automatic frames and task calls use the existing lifetime
  rules rather than special handling by source container.

## Shared storage, SIR and collection

Use a Context-owned typed heap with container objects and stable element
cells. Kernel descriptors express value layout without depending on slang
or SIR. Empty uses handle zero as already specified. No new dependency,
global state, environment lookup or runtime lookup by signal name is added.

SIR's existing planned `dynarray<T>` and `queue<T>` forms get explicit typed
operations for construction, value copying, length, element access/reference,
methods and traversal. front resolves types, receivers and destinations;
the kernel owns allocation, mutation, copying and lifetime. VM/codegen
adapters supply operands and root metadata and execute those operations.
The verifier and text writer must represent/check these new forms. Public
kernel interfaces remain C++20.

Fill in RUNTIME_ABI 6 with precise tracing at region/time-slot boundaries.
Roots include live typed slot/frame/temporary values, active reference
cells, pending writes/events and report/callback records. Follow nested
container values and reference cells; keep detached cells reached by refs.
Collect only after the tier has exposed all roots and no DPI call is active.
Do not collect through unregistered native temporaries. Free unreachable
container objects and cells and reuse records without allowing stale tokens
to name a different value. Later class/mailbox/weak-reference support extends
these root descriptors under its own semantics.

**Project allocation choice:** checked size/byte arithmetic and allocation
failure return `resource_limit` / code 6 through the kernel boundary, never
an escaping allocation exception or a partial destination replacement.
Do not introduce an undocumented capacity cap or a new CLI option. The
initial implementation may defer collection until a permitted boundary;
it must not claim unbounded memory scalability or full parity from that.

## Verification and scope

Write original matrices across element type/width, empty/nonempty/boundary
sizes, constructor and copy forms, methods, source spelling, slot/frame/ref
storage, direct/called/fork execution and module/program/interface scopes.
Include same-size resize, nested values, alias-free copying, queue index
movement, retained outdated refs across waiting calls, dropped bounded-queue
elements, failed allocations, collection root coverage and Context isolation.
Use independent value/cell models for kernel tests and preserve existing
suite bodies and expectations. Separate any existing rejection expectation
changes from implementation commits, with this approved rationale.

Classes, associative arrays, VPI and the other P1 gaps remain required work
where Icarus supports them. A passing basic array/queue probe is not proof
that all the mechanisms above or the full P1 objective have been completed.
