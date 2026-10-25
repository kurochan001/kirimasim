# Typed event fields and retained activation roots

[日本語](event-activation-roots.ja.md). Status: component implemented and checked,
2026-10-25. Interpreter root publication and source construction remain work.

## Contract and implementation

The grounds are the approved [automatic-capture contract](automatic-capture-proposal.md)
5/6/7, SIR_SPEC 3.2.1/7/8, RUNTIME_ABI 4/6 and IEEE 1800-2023 6.8, 6.21 and
15.5. Implementation and tests are original; no external implementation/test
code was read and no dependency or performance budget changed.

`1dc06096` accepts exactly the canonical `event` type in activation fields.
`ActivationPlan` maps it to the kernel's existing 64-bit typed event provider;
malformed widths are rejected before layouts are published. The complete
retained lexical extent is visited, including unread scalar and fixed-array
fields retained by children that capture only a different field. Event bits in
ordinary integer fields do not keep objects alive. A concrete named wait keeps
its selected object alive after its source scopes end, until cancellation.

`FrameLayout.eventRoots` records ordinary Event **row IDs**, separately from
container/ref roots. Final array offsets are assigned before consumers enumerate
elements. Scoped/captured rows use the retained kernel activation provider.
The Interpreter does not yet publish this ordinary row list to the registry.
The new representation alone does not enable `event.new` execution or source
automatic-event declarations.

The separately approved [historical input migration](event-field-expectation-migration.md)
items 1/2 was applied in `ecbe4abb`: only canonical-width inputs changed to
malformed-width inputs; rejection assertions and other cases remain intact.
Item 3, the source-level metadata fixture, remains unchanged and pending.

## Evidence and limits

Six original `EventActivation` tests exercise canonical scalars, ascending and
descending fixed arrays, rank-two arrays, all three joins, whole unread extents,
multiple children, shared identity, wait pins, typed integer exclusion and final
ordinary array offsets. All 51 focused tests pass after the approved migration.
The reentry test performs 512 cycles at time zero with one physical event row,
but calls collection **explicitly**. It does not establish bounded allocation
inside Interpreter no-wait loops.

All five [compiled controls](../measurements/2026-10-25/event-activation-controls.json)
build successfully and fail the intended real tests; restored production passes
all 51 focused tests. The [full local CI](../measurements/2026-10-25/event-activation-validation.json)
passes all 21 checks: dev/ASan/UBSan 4,954/4,956 and release 4,955/4,957,
zero failures and two known skips each. All 29 existing dumps and PerfCount pass.
README counts are generated from dev JUnit.

The fresh [87-probe run](../measurements/2026-10-25/icarus14-event-activation-probes.json)
preserves every native/Icarus-14 status and passing output byte. The binary's
actual configure-time version says `2866a0b62a05`; provider sources were dirty
at configuration and later committed as `1dc06096`. The evidence records exact
source hashes rather than treating that version as a clean source identity.
Historical Icarus-13-passing gaps remain 21; this is not a complete inventory.
There was no fresh Icarus-13, population, sv-tests, Linux or speed measurement.

## Next connection and decision

Publish typed slot, ordinary/scoped/captured/input and temporary holders; execute
the typed operations through the kernel and enable front only with the full
source matrix. Unpublished transfers need ownership through callbacks and errors.
The [managed-event safepoint amendment](managed-event-safepoints-proposal.md) is
**not approved**: RUNTIME_ABI 6 currently restricts collection to slot/region
boundaries. The proposal explains why event-only instruction safepoints would
be needed to prove bounded no-wait reuse. No new collection timing is enabled
by this component. P1 and full Icarus coverage remain incomplete.
