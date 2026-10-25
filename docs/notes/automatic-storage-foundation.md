# Shared automatic-storage kernel foundation

[日本語](automatic-storage-foundation.ja.md). Status: kernel foundation
implemented, 2026-10-25; source capture integration remains work.

## Approved contract and scope

The user approved [automatic-capture-proposal 1–8](automatic-capture-proposal.md)
on 2026-10-25, with the [normative mapping](p1-execution-contracts-2026-10-25.md)
to SIR_SPEC 3/7/8, SCHEDULER_SPEC 9/10 and RUNTIME_ABI 4/6.
IEEE 1800-2023 6.21/9.3.2 motivate shared identity and retained lexical
extent; the detailed ownership and token rules are explicit approved project
choices. No other simulator implementation or tests were read. This change
adds no dependency, installation, global state or external ABI entry.

This implements reusable Context-owned storage and typed root publication.
It does **not** enable source fork capture, activation instructions, managed
automatic events, ref bindings or class/object storage. Existing source
rejections and expected results remain unchanged. Completing the approved
contract still needs those layers and its source mechanism matrices.

## One physical value per retained activation

`Context::activations()` owns `ActivationStore`. Immutable layouts retain
field kinds, widths, fixed dimensions and checked physical offsets. All
kinds/dimensions/offsets are validated before layout allocation. Two/four-state
integral, real/shortreal, immutable string/chandle handles and explicitly typed
container/cell roots have defined storage. Unknown field kinds are rejected;
objects/events/refs are not silently treated as untraced integral handles.
Safe internal zero words are not source initialization: future front lowering
must still emit each declaration's default/initializer once.

A scope allocation has a numeric 32-bit row plus 32-bit generation token.
Owner and descendant leases retain the same complete record, including fields
not explicitly read by the child. Reentry while an older child remains live
allocates separate storage. Releasing the last lease frees values and reuses
the metadata entry with a different generation. At generation exhaustion the
entry retires, without wrapping. Clear invalidates existing owners/bindings
without resetting generations. Empty lexical extents have the same ownership
rules without value words.

Bindings hold immutable field identity, access rights and C++ store identity.
They are runtime metadata, not native addresses in SystemVerilog values.
Borrowing/copying a binding does not acquire ownership. Explicit retain and
move-only leases govern lifetime; C++ leases must not outlive their Context.
Readonly rights survive retain/move/rebinding, and foreign-Context bindings
are rejected even when their local numeric tokens coincide.

Typed reads/writes validate generation, field, element, bit range and buffer
size. Writes normalize every chunk before observation; two-state conversion
uses existing kernel/valsem semantics. Failed, invalid, readonly and unchanged
writes publish no change. Read/change hooks describe numeric field/element/bit
ranges and cannot allocate, throw, mutate or reset the store. The scheduler
subscription integration is still pending; these are its storage hook boundary.
Allocation failures return status without publishing partial records or altering
existing bindings. Lease release does not allocate or execute source callbacks.

## Typed roots independent of interpreter frames

Root enumeration visits only declared container and detached-cell fields.
Equal integral/string/chandle patterns do not become heap roots. Physical
fields are traced once, regardless of how many leases retain their scope.
Root-only replacement and last-lease release mark the root set changed.
Strings use the existing immutable Context string representation.

VM's existing safe-point publisher also enumerates this kernel store,
independently of live interpreter frames. It counts roots and sizes the
caller-owned destination before a nonallocating visitor fills it. The existing
shared heap collector handles collection, failures and pending roots. Tests
prove both retention after an issuer frame ends and reclamation after a lease
is released without a heap mutation. No kernel operation depends on slang,
SIR or LLVM. Public headers and the standalone stress probe compile as C++20;
C ABI stays 0.3 and tier 1 continues to call C++ directly.

## Verification

[Fourteen original unit tests](../../tests/unit/activation_test.cpp) cover
80 typed layouts across two/four-state values, ten widths from 1 to 257 bits,
scalar/ascending/descending/rank-two shapes, independent elements, preserved
packed neighbors, nested leases, reentry, reset, Context separation, readonly
rights, normalization, observation, typed roots, detached cells, allocation
failures and two real VM safe-point paths. Repeating 100,000 completed scopes
keeps one metadata entry. Eleven compiled controls are detected by actual
failures (4, 2, 1, 1, 2, 1, 1, 1, 2, 2 and 2); restored tests pass.
The final pre-sized root visitor is checked again by its compiled omission
control. All 547 mutation anchors apply. The
[focused record](../measurements/2026-10-25/activation-foundation-focused-validation.json)
retains the evidence. Dev has 4,555/4,557 passes, no failures and the same two
known skips; changed-file clang-tidy also passes.

[The explicit stress tool](../../tools/check_activation_generations.py)
executes **all 4,294,967,295 production generations**, checks the retired row,
new row and stale binding, and retains the normal generation limit. The
[measured run](../measurements/2026-10-25/activation-generation-exhaustion.json)
passes in 35.684 seconds, compiling the actual kernel source as C++20.
It is outside regular CTest and is reproducible with:

```sh
python3 tools/check_activation_generations.py --output build/activation-generations.json
```

This is a storage stress result, not a simulation-speed comparison. Existing
PerfCount expectations are not changed. Wall-time simulation impact has not
been separately remeasured; use PERFORMANCE's existing measurement procedure
when evaluating source integration. Full P1 parity is not established by this
kernel foundation or its finite feature inventory.
