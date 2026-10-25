# Lexical activation environments for the VM

[日本語](automatic-vm-environment.ja.md). Status: environment construction
implemented and directly tested, 2026-10-25; Interpreter integration remains work.

## Contract and ownership

This implements environment construction from sections 1–4/6–8 of the approved
[automatic capture contract](automatic-capture-proposal.md), mapped to
SIR_SPEC 7/8 and RUNTIME_ABI 4/6 in the
[approval record](p1-execution-contracts-2026-10-25.md). The language references
are IEEE 1800-2023 6.21, 9.3.2 and 13.5.2. No external implementation or test is
used. No dependency, public C ABI, format version or physical frame ABI changes.

`ActivationPlan` decodes immutable SIR frame/scope metadata into kernel layout
IDs. This is an implementation table, not another IR. The Context owns the
layouts. Plans borrow immutable SIR metadata and must outlive their environments;
moving a plan invalidates the moved-from plan. Local graph, membership, type,
shape and access checks precede use. The global SIR verifier still checks
process/subroutine ownership and CFG consistency. Unsupported owned event/ref
fields have no pretend provider.

`ActivationEnvironment` holds one lease per executing local scope and one per
imported lexical extent. Frame rows map to immutable kernel bindings; no mutable
value or mirror is kept. Scope entry creates fresh storage. Capturing copies the
binding to that actual allocation, including access rights. A later entry of the
parent's same lexical block does not redirect existing children. Captured fields
from one scope do not create extra physical storage or per-field leases.

The child-group factory resolves and retains all requested environments before
returning any group. A failure destroys all provisional roots/imports and leaves
the parent unchanged. The caller must create runnable processes and commit RNG
advances only after this succeeds. This does not yet make the scheduler's full
fork registration transactional. The environment contains no PC, control stack,
thread, RNG or process-completion membership.

Imports include the whole active lexical chain, including unread fields and
empty scopes, and explicitly forward the parent's imported chain. New local
tokens and imported tokens are disjoint by construction. Retention walks these
lists once, without a quadratic token search. Temporary callback pins retain
the same full set. Scope unwind clears local bindings before releasing leases;
finish also drops imported bindings and leases, independently of control-frame
reclamation. The interpreter must detach waits/callbacks before finishing an
environment. Allocation failures are returned and cleanup does not allocate.

Reference captures are immutable borrowed reference bindings, not copies of the
referenced value or pointers into a formal's replaceable table. The resolver must
validate the actual origin and effective rights/lifetime in the interpreter's
private reference encoding. The environment keeps readonly rights, enforces the
declared `ref static` qualification and rejects an automatic actual for it.
A normal ref remains restricted to `join`, even when its actual is static.
Forwarded refs require inherited-environment metadata even when the extent list
is empty. A borrowed ref acquires no automatic lease. A nonallocating visitor
exposes legitimately retained reference bindings to the interpreter's typed root
publisher; scope value roots remain in the shared kernel store.

## Verification and remaining integration

Twenty original tests exercise shared sibling values for all three joins,
separate calls, block reentry, forwarding through returned owners, unread typed
roots and empty extents, readonly rights, 12 ref-qualification/join combinations,
static-ref forwarding across reuse of a formal, malformed/unsupported layouts,
120 typed storage shapes and Context/plan identity. Allocation budgets are
exhausted at each failing step of a three-child group for both kernel and
environment storage. Failed groups release provisional leases and values;
1,000 repeated late failures reuse storage. Callback pin failure and scope-entry
failure preserve the executing state. A 10,000-scope chain is constructed,
pinned and released iteratively. Initial focused dev tests and clang-tidy pass.

Nine compiled-control definitions cover lost inherited extents, wrong binding
identity, omitted outer scopes, delayed executing-owner release, partial group
publication, ref escape, readonly loss, automatic `ref static` actuals and missing
reference-forwarding metadata. Actual control and full-CI outcomes are recorded
after running them, not inferred from these definitions.

The component is linked into `vm` and called directly by these tests.
`Interpreter` does not yet construct or attach these environments, and its
existing rejection of new SIR homes remains in force. Front still emits no new
capture metadata. No source capture, automatic event support, Icarus inventory
gap closure or P1 completion is claimed by this checkpoint.

Next attach environments to logical `VarsId` registrations and resolve
`FrameHome` through the [value-access boundary](automatic-vm-frame-access.md).
Guard/watch helpers borrow the actual waiter's environment. Scoped references
need an immutable actual binding and selection, generation checks and rights;
they must not follow a parent's current row on reentry. Construct the root before
copy-in, preserve output initialization and copyback once, and release executing
owners at return/kill rather than `tryRelease`. Prepare every fork environment
before starting children or advancing RNG. Complete callback/monitor ownership,
typed reference roots, named-control unwinding, program cleanup and resource
error propagation before removing the execution gate or enabling front capture.
