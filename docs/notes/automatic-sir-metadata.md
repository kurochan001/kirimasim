# Automatic scopes and capture metadata in SIR

[日本語](automatic-sir-metadata.ja.md). Status: representation/verifier
checkpoint, 2026-10-25. VM and source integration remain work.

Subsequent checkpoint: the [VM value-access migration](automatic-vm-frame-access.md)
is implemented and validated. Resolving the new homes and constructing their
owned environments and immutable references remain work before execution is enabled.


Current integration: the [Interpreter execution adapter](automatic-vm-execution.md)
is now connected. This note records the earlier component checkpoint; its load
gate and remaining-work statements describe that checkpoint. Valid represented
SIR homes now execute, while front source capture remains disabled.

## Approved contract and boundaries

This implements the representation/verification part of sections 1–3/7/8 of
the approved [automatic-capture contract](automatic-capture-proposal.md),
mapped to SIR_SPEC 3/7/8 by the
[approval record](p1-execution-contracts-2026-10-25.md). The source-language
grounds are IEEE 1800-2023 6.21, 9.3.2 and 13.5.2; the representation is a
project implementation choice under that contract. No simulator implementation
or external tests were read or copied. No dependency or external ABI entry
is added. This does not finalize P2/SIR v1: formatVersion remains 0, no parser
is introduced and physical frame sizes/alignment/offsets remain undecided.

The [kernel storage](automatic-storage-foundation.md) and
[wait observation](automatic-storage-observation.md) already exist. New SIR
homes must not silently use independent ordinary-frame words. Until every
typed access, ref, initialization/copyback and unwind path is connected, the
VM explicitly rejects designs using the new metadata before execution.
Front emits none of it yet. This checkpoint enables no source fork capture,
automatic events or new source type, and closes no Icarus-passing gap.

## Representation

`FrameHome` distinguishes ordinary rows, scoped fields and imported captures.
A scoped row names a body-local ActivationScopeId. Each immutable scope row
lists its fields in layout order and its lexical parent. `ActivationInfo::root`
names storage to create before parameter copy-in; block ownership uses
`activation.enter #N` and `activation.leave #N`. The owning environment is
frame state, not a temporary surviving a wait. These IDs are distinct from
design-wide named-disable identities and select no physical offsets.

A captured row names a frame row of the process/task in `Process::parent`.
It describes a binding, not a value clone or an alias to the parent's current
table entry. `ActivationInfo::extents` lists the parent's full active lexical
chain, including unread/empty scopes; `inheritCaptures` forwards its inherited
extents to nested children. Lexical environments are distinct from dynamic
call/process ancestry. Runtime acquisition and transactional child startup are
still to be implemented.

Frame/parameter metadata preserves readonly binding rights and `ref static`.
Captures must match source types, descriptor IDs, dimensions, ref targets and
static qualifications; they can restrict rights but cannot regain writes.
Normal ref captures are permitted only for `join`; static-qualified aliases
can use every join form. This checks declared metadata: static-actual
provenance at call sites and the complete reference execution/root adapter
remain work. Borrowed refs are not promoted into indefinite ownership.

Owned fields currently accept the types with a real ActivationStore provider:
two/four-state integral, real/shortreal, string/chandle and typed container
handles with checked fixed dimensions. Owned event/ref fields remain rejected
until their actual providers exist; they are not untraced integer handles.
Normal frame refs and imported ref bindings are distinct from owned fields.

## Verification and text

The shared `verifyActivationScopes` pass checks field membership, root/parent
graphs, dimension overflow, capture shapes/rights, complete lexical extents,
fork-parent cycles and root placement of formal arguments. CFG edges require
the same executing ownership stack; waits preserve it, returns release the
executing environment, and named control exits restore their saved automatic
depth. Entry/leave use the innermost lexical scope. Guard/watch frame reads
require the scope to exist at the wait/trigger site; helper code cannot own
new scopes or write readonly capture views. The main verifier retains its
existing type, SSA, reachability and scheduling checks.

Text prints scope fields/parents/root, capture extents, homes, readonly and
static-ref qualifiers, and the two instructions deterministically. Ordinary
designs print none of the new tables/attributes. This is printing only;
print/parse/print coverage and a stable grammar are not claimed.

## Evidence and next work

Sixteen original ActivationSir tests cover 66 typed shapes, 25 malformed
tables, eight ownership failures, twelve ref qualification/join combinations,
waits, repeated block entry, named disable, readonly helpers, root parameter
copy-in and a 10,000-scope acyclic/cyclic graph. A malformed execution input
is rejected before slot/storage allocation. All nine
[compiled controls](../measurements/2026-10-25/activation-sir-negative-controls.json)
at `56a10e05` build and fail actual new verifier tests; the restored build
passes. No judging self-check failure is counted as evidence.
The [final validation](../measurements/2026-10-25/activation-sir-final-validation.json)
passes all 21 local-CI checks: dev/ASan/UBSan pass 4,585/4,587 and release
4,586/4,588, zero failures and the same two known skips. All 29 existing SIR
dumps and PerfCount budgets remain unchanged, as do existing suite oracles.

Three [original initialization probes](../measurements/2026-10-25/automatic-entry-initialization-observations.json)
check existing ordinary-frame/static-slot behavior separately. Native resets
automatic scalar/array defaults on all three block entries and retains static
array writes; both references retain the automatic writes too. These are
differences against the existing approved per-entry contract, not adopted
reference oracles or proof of new capture support. The frozen 87-probe cohort,
population and sv-tests were not rerun for this representation-only checkpoint.

Next connect VM logical frame/ref access to typed kernel bindings and attach
call/block ownership and fork environments. Every wide/packed/array, file/scan,
container, random-seed and copyback path needs that same access layer. Cancel
waits before dropping leases, keep retained storage independent of returned
call control, and preserve one-time copyback. Then front can promote complete
lexical scopes and enable source matrices. The VM gate can be removed only
after those execution paths have semantic evidence. Full P1 remains incomplete.

Fork capture must resolve `act_.varsId` (the active SIR body), which can be a
synchronous function or a waiting task, rather than the process root's frame.
Storage-owner release cannot wait for `tryRelease` to reclaim a finished
control frame with descendants/reports. Scoped refs need immutable bindings
to the actual scope token, including permissions, instead of following a
parent's replaceable binding-table entry. Acquire the complete fork environment
before making children runnable or changing its random-seed progression.
