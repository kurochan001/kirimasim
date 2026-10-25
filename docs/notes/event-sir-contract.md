# Typed managed-event values in SIR

[日本語](event-sir-contract.ja.md). Status: SIR checkpoint `efebd540`, 2026-10-25;
VM execution and automatic-event source construction remain unimplemented.

## Grounds and boundary

The [approved automatic-capture contract](automatic-capture-proposal.md) 5/6/7,
its [normative mapping](p1-execution-contracts-2026-10-25.md), SIR_SPEC 3.2.1/7/8,
RUNTIME_ABI 4/6 and IEEE 1800-2023 6.8, 6.21 and 15.5 are the grounds.
Implementation and tests are original. No external simulator implementation or
test code was read, dependency introduced, new IR created or C ABI selected.

`event.new` produces one typed `event` value without operands. It is neither a
terminator nor a wait. The existing named wait, mixed event wait, immediate
trigger and nonblocking trigger accept typed SSA/constant event operands as
well as their existing legacy slot operands. Direct frame references are not
event values: load the typed value first. Mixed items keep their individual
`iff` guards, signal edges and existing order. Nonblocking delays stay integral.

An event constant can only contain one zero word, no unknown-word vector and
no byte string. Nonnull identities must be constructed or loaded from typed
storage, rather than forged from integral constants. Typed event operands have
no edge, selection or block-argument count. Existing SSA dominance and
no-live-SSA-across-wait rules continue to apply; store values needed later in
their typed home. The draft printer emits `event.new event`, `%tN` targets and
`const event null`. Format version stays **0**. No parser or text roundtrip
guarantee is introduced by this checkpoint.

## Evidence

Eight new `EventSir` tests exercise 17 valid designs and 40 malformed designs.
All 75 focused EventSir/Sir/VerifyMatrix tests pass. Eight independently compiled
faults in arity, result type, constant validation, operand domain, hidden
selection, mixed-item recognition, opcode spelling and null printing are caught.
Compile failures and judging self-checks do not count as detections. Restored
production passes all 75 focused tests.

The [full local CI](../measurements/2026-10-25/event-sir-validation.json) passes
all 21 checks: dev/ASan/UBSan 4,948/4,950, release
4,949/4,951, zero failures and two known skips each.
All 29 existing dumps and PerfCount pass, and existing expectations are unchanged.
[Control evidence](../measurements/2026-10-25/event-sir-controls.json) retains exact
source hashes and failing test names. README counts are generated from dev JUnit.

## Remaining connection and approved migration

This checkpoint verifies and prints the representation. It does **not** execute
the new forms through tier 1 or enable them in front. Existing automatic-event
source rejection and canonical-event activation-field rejection remain in place.
The [three exact oracle migrations](event-field-expectation-migration.md) were
approved in `965806f4`; none is applied here. Apply their first two only with the
real event-field provider, and their source-input migration only with complete
source execution. Keep implementation and existing-oracle commits separate.

Next, connect canonical event fields to `ActivationPlan`, then publish every
ordinary/scoped/captured/input/SSA event holder and the complete retained-scope
provider. Keep unpublished temporaries pinned until publication. Connect
`event.new`, typed scheduler operations and triggered-read capture mechanically
in tier 1, including positioned null-wait errors and cleanup on all exits.
Only then enable front construction/copies/indexed operations and validate the
full original source matrix. Test bounded physical reuse for repeated scopes
and declarations **without** a time wait, as well as suspension and descendants.

No new source-level Icarus comparison was run or gap removed at this IR-only
checkpoint. The previous 21 historical Icarus-13-passing gaps are not a complete
inventory. Population, sv-tests, Linux and speed measurements were not refreshed.
P1 and full Icarus coverage remain unfinished.
