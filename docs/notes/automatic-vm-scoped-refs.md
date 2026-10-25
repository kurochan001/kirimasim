# Immutable scoped references for the VM

[日本語](automatic-vm-scoped-refs.ja.md). Status: reference component implemented
and directly tested, 2026-10-25; complete Interpreter attachment remains work.


Current integration: the [Interpreter execution adapter](automatic-vm-execution.md)
is now connected. This note records the earlier component checkpoint; its load
gate and remaining-work statements describe that checkpoint. Valid represented
SIR homes now execute; [source capture](automatic-front-capture.md) is also
implemented in `ebb0e14d`, with historical expectation migration pending review.

## Contract and representation

This implements the reference-address component needed by sections 1–4/6–8 of
the approved [automatic capture contract](automatic-capture-proposal.md), mapped
to SIR_SPEC 7/8 and RUNTIME_ABI 4/6 by the
[approval record](p1-execution-contracts-2026-10-25.md). The language references
are IEEE 1800-2023 6.21, 9.3.2 and 13.5.2. The implementation and tests are
original; no external source or test is used. This adds no dependency, public
C ABI, SIR format version or physical frame ABI.

`ActivationOrigin` carries the immutable actual kernel binding and its declaring
plan/frame row. Captures and descendants preserve that origin, including narrowed
readonly rights, rather than substituting an intermediate capture row. A parent
can return or enter the same block again without redirecting a descendant's
reference. Plans borrow immutable SIR metadata and must remain at a stable address
until their environments and reference registry are destroyed.

`FrameLayout` is the one implementation of ordinary frame word positions, shared
by the Interpreter and reference decoder. It also describes private logical word
addresses for reference selection. Scoped and capture rows occupy no ordinary
value words or ordinary typed-root entries. Existing ordinary array ordering,
wide chunks and reference/container root positions remain the same. These
addresses are implementation details, not the P2 frame ABI.

`ScopedReferences` decodes each plan into a table of owned rows sorted by logical
address and their actual scope-field ordinals. A descriptor stores the binding,
declaring shape/row, selection and creation depth. Stepping resolves the logical
address back to a real field/array element, checks that it remains in the same
lexical extent and preserves readonly rights. It does not assume that consecutive
SIR rows have consecutive kernel field ordinals. Ordinary rows and array
placeholder words are holes, and stepping into a different lexical scope fails.

The descriptor table has a 32-bit index and 32-bit generation. Use validates the
descriptor generation and the actual kernel binding. A body owns tickets, not
the referred-to value. Its scope unwind/return releases tickets independently
of control-frame reclamation. Equal addresses with equal rights and depth are
interned within one body; released entries are reused with a new generation,
and maximum-generation entries are retired. Different bodies' tickets remain
independent. The registry acquires no lasting scope lease and cannot extend an
otherwise invalid borrowed reference. A short-lived lease used to select a
sibling field preserves the binding's effective rights.

Shape and ticket allocations use an injectable PMR resource. Failed registration
does not publish a ticket, consume a reusable descriptor or retain an empty owner
row; a later retry can succeed. Cleanup only compacts and erases existing entries.
No exception crosses its noexcept methods. The resource and store must outlive
the registry. Numeric tokens alone are not portable between Contexts; shapes and
typed origins reject a foreign store. This does not claim that arbitrary numeric
bit patterns encode their source Context.

`FrameValue` now supports a selected physical origin within a bound field, with
an explicit width. A tail word or shortreal view therefore does not request the
entire underlying field accidentally. Reads/writes still go through the one
kernel storage/notification boundary. Its immutable borrowed-ref view copies
only the reference payload, including both W4 halves; it rejects rebinding and
does not copy the referenced SystemVerilog value.

## Evidence and limits

Eleven original tests cover 57 array-word selections over widths 1–257, selected
physical writes, shortreal bits, deliberately reordered field ordinals, logical
holes, returned/reentered owners, multilevel forwarding, readonly/writable
coexistence, invalid/stale/foreign identities, per-owner cleanup, 10,000 block
reentries with two descriptor entries, every injected shape/ticket allocation
budget and immutable reference payloads. The preceding environment/value suites
also pass, 39 focused tests in total.

[Compiled controls](../measurements/2026-10-25/scoped-reference-negative-controls.json)
inject nine new faults: stale generation acceptance, wrong
array element, wrong physical chunk, wrong narrow width, crossing an extent,
merging different rights, lost interning, missed scope-ticket release and an
origin redirected to the child. The relocated array-layout, environment-origin
and two notification/selection controls are also rerun. A compile failure is not
counted as a detected behavior fault.

The [full local-CI run](../measurements/2026-10-25/scoped-reference-final-validation.json)
at `589bd26b` passes all 21 steps. Dev/ASan/UBSan pass 4,661/4,663 and release
4,662/4,664, zero failures and the same two known skips. All 29 SIR dumps and
existing PerfCount budgets pass unchanged. A
[focused ordinary-frame cost observation](../measurements/2026-10-25/scoped-reference-speed.json)
uses five alternating pairs: adjusted wall time is 0.2359 s before (`652159b9`)
and 0.2388 s after (`589bd26b`), about +1.3%, with identical counters and median
peak RSS of 9.1875 MiB. This small sample is not a confidence interval or a
standard simulator-speed comparison. Scoped-reference execution, the standard
four benchmarks, other simulators and Linux are not timed, and no budget changes.

Full 32-bit exhaustion of this **descriptor** registry is **not confirmed**.
The earlier kernel storage exhaustion measurement is not evidence for this
separate table. Verification would repeatedly acquire/release one descriptor
through its real generation limit, then check retirement and stale rejection;
no reduced-generation production knob is introduced.

The fresh [87-probe comparison](../measurements/2026-10-25/icarus14-scoped-reference-probes.json)
keeps every native/14.0 status and passing judged byte unchanged; against the
historical 13.0 record, 22 passing gaps remain. The exact archived
[P1 population](../measurements/2026-10-25/scoped-reference-population.json) stays
1,451/1,466, zero failures and 15 unsupported. Pinned
[sv-tests](../measurements/2026-10-25/scoped-reference-svtests.json) stays 244/344,
with every native status unchanged. The initial sv-tests archive missed its
configuration; that setup failed before any test ran, and the complete pinned
archive was then used. Linux and reference engines for these two datasets are
not remeasured. A finite probe run is not a complete Icarus coverage proof.
Existing expectations, dump text and PerfCount budgets are preserved.

The Interpreter currently consumes the common ordinary layout only. Its load
gate still rejects new activation metadata; it does not construct environments
or scoped-reference descriptors yet. Source fork capture and automatic events
remain unsupported, and no Icarus inventory gap is claimed closed. Next connect
plans/environments to `VarsId`, scope instructions, transactional fork setup,
call copy-in/copyback, return/kill/program cleanup, guard callback ownership,
static/dynamic wait dependencies and typed reference roots before removing that
gate. Then enable source lowering and verify the full approved source matrix.
