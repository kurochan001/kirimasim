# P1 expansion toward Icarus coverage

[日本語](p1-icarus-progress.ja.md). Status: in progress, 2026-10-27.

**Revision (2026-10-24):** old P2→P3, old P3→P4 and old P4→P5; new P2 (SIR v1) is inserted, with tier numbers unchanged ([mapping](phase-renumbering.md)).

The goal is P1 functional support at least as broad as Icarus Verilog 13.0.
Complete parity is **not confirmed**. The previous pass-rate gates and the
small probes below cannot establish it. The
[scope amendment](p1-icarus-scope-proposal.md) was approved on 2026-10-23
and incorporated into [P1_SCOPE](../spec/P1_SCOPE.md), advancing necessary
P3/P4 and previously out-of-scope work into P1.

## Current implementation and review status (2026-10-27)

The [main integration record](../measurements/2026-10-27/conditional-nba-main-integration.json)
preserves the earlier measurement-time records and verifies unchanged validated
implementation/tests, fresh local CI checks with a corrected dev rerun, and
commit-level expectation guards.

**Main integration `7bc5fa85`: NBA payload prerequisite.** [owned values and pending issue order](event-nba-payload-foundation.md)
now use the shared kernel, with 24 independent unit tests and ten caught compiled
faults. This does not enable source event-controlled NBA; subscriptions, retained
callback origins and typed SIR/VM transport remain. The existing source gap stays
open and P1 remains incomplete.
[Final-layout local CI](../measurements/2026-10-27/nba-payload-validation.json)
passes 21/21 checks: dev/ASan/UBSan 7,013/7,015; release 7,014/7,016,
zero failures and two known skips each. A measured queue-memory regression
was removed by reusing the existing event word; its before/after evidence is retained.

**Main integration `a8bb17cc`:** [conditional dynamic-array/queue values](container-conditional-execution.md)
add shared front lowering of short-circuit branches, independent operand snapshots,
whole-element unknown defaults and nested content equality. New coverage has
351 source designs across nine execution homes, a Context fixture and eight caught
compiled faults. Both reference inventories now have 95 original probes; all
eight additions pass natively and both Icarus versions reject them. The previous
87 probes improve `queue_conditional` from unsupported to passed; the other 86
retain their native status/stdout, and seven known gaps remain.
[Full local CI](../measurements/2026-10-27/container-conditional-validation.json)
passes 21/21 steps: dev/ASan/UBSan 6,989/6,991 and release 6,990/6,992,
zero failures and two known skips each. Existing oracles/specifications/budgets
are unchanged. P1 remains incomplete.

**Main integration `624123b2`:** [live runtime-process limits](process-limit-execution.md)
implement approved CLI 2.7 with one kernel admission/completion rule. Static
processes and retained ended records do not count; over-limit children never
start. Coverage includes 90 original designs, 13 CLI contracts, ten kernel
tests, independent Context limits and eight caught compiled faults.
The [integration record](../measurements/2026-10-27/process-limit-main-integration.json)
confirms unchanged validated code/tests and commit-level expectation protection.
Full local CI passes 21/21 steps: dev/ASan/UBSan 6,637/6,639 and release
6,638/6,640, with zero failures and two known skips each.
No existing oracle/specification/budget is changed. P1 remains incomplete.

**Main integration `9e6a149d`:** [non-parameterized class static storage](class-static-execution.md)
adds eager initializer discovery, canonical inherited slots and static-method
access through existing typed storage. Parameterized static storage is rejected
explicitly instead of silently omitting effects; objects remain incomplete.
The [integration record](../measurements/2026-10-27/class-static-main-integration.json)
confirms unchanged validated implementation/tests and the commit-level oracle
guard. Full local CI passes 21/21 steps: dev/ASan/UBSan 6,625/6,627 and
release 6,626/6,628, with zero failures and two known skips each.
No existing oracle or specification changes; P1 remains incomplete.

**Main integration `f100cd86`:** [event initialization copies](event-initializer-execution.md)
in `dfddb94b` share objects for scalar values and fixed-array patterns, including
null; static initialization remains once before time zero. The user approved
the exact three-case oracle migration, separately applied in `2da32daa` with
all 24 original checker inputs byte-identical. New coverage has 208 source
designs, 14 strict boundaries, four SIR constructor checks and seven caught
compiled faults. [Full local CI](../measurements/2026-10-27/event-initializer-validation.json)
passes 21/21 steps: dev/ASan/UBSan 6,057/6,059 and release 6,058/6,060,
zero failures and two known skips each. Fresh fixed population/sv-tests and
both 87-probe comparisons preserve every prior pass and native stdout.
P1 remains incomplete; these finite comparisons are not a parity proof.
The [main integration record](../measurements/2026-10-27/event-initializer-main-integration.json)
confirms unchanged validated source and preserves the separate approval history.

**Earlier main baseline `63c64d83`:** [source event inputs](event-input-execution.md)
now share object identity through scalar/fixed-array calls, waiting tasks,
retained descendants and deferred snapshots. Runtime-selected input subarrays
use the common typed storage copier. The 104 new designs, policy boundaries
and four compiled controls pass. Full local CI passes 21/21 steps, with
dev/ASan/UBSan at 5,847/5,849 and release at 5,848/5,850, zero failures and
two known skips. Fresh population/sv-tests and both 87-probe comparisons
preserve the prior counts and all prior passes; the linked note records the
implementation digests and limits. [Post-commit local CI](../measurements/2026-10-26/event-input-committed-validation.json)
also passes all 21 steps before main integration. No existing expectation or
specification is changed.

**Earlier automatic-event main checkpoint:** `e909f022` adds [automatic declaration events and
instruction collection](instruction-event-execution.md), publishing native/SSA/transfer
roots before physical growth. Its 176 source designs, 12 VM tests with 32 verified
SIR designs, one registry query test and 12 compiled controls pass. Both 87-probe
comparisons reduce gaps from eight to seven; four/seven reference mismatches stay.
The fixed population remains 1,456/1,466 with zero failures and ten unsupported;
sv-tests remains 244/344. The [one historical rejection-oracle migration](automatic-event-expectation-migration.md)
was separately approved and applied in `4ba6b63d`. [Post-approval local CI](../measurements/2026-10-26/automatic-event-approved-validation.json)
passes 21/21 steps: dev/ASan/UBSan 5,741/5,743, release 5,742/5,744,
with zero failures and two known skips each. The six commits after `1c970f5d`
were fast-forwarded into main and pushed as `304e1aec` on 2026-10-26;
[the publication record](automatic-event-main-integration.md) identifies the
hosted CI snapshot (all 14 jobs successful) and preserved validation. P1 remains incomplete.

### Earlier component and integration checkpoints

The following paragraphs retain their measurement-time counts, approvals and
local/unmerged status. They do not reopen resolved approvals or describe the
current publication state.

**Earlier transfer checkpoint:** `1d2b7e6e` adds [event transfer ownership](event-transfers.md)
for input/copyback snapshots and returns, with six component tests and four VM
tests covering nine SIR designs. Five real VM allocation-failure paths stop
before callee effects or partial output replacement. All 12 compiled faults
are caught; full dev CTest passes 5,551/5,553 with zero failures/two known skips,
and all four presets pass 98 related tests. Five-pair ordinary frame/event cost
observations are +2.1%/+3.9%, with identical counters. Active-field/instruction
publication and automatic source construction remain work. This checkpoint is
local, not merged/pushed; external source probes were not refreshed.

**Earlier main integration (`1c970f5d`):** that integration includes source passive
switches, typed SSA event regions, the approved event-only safepoint contract
and the README's remaining-feature inventory. The user separately approved the
exact tran expectation migration, applied in `77c9d188` without source fixture
changes. [Integration validation](main-integration-2026-10-26.md) records the
post-migration checks: all 21 local CI steps pass; dev/ASan/UBSan pass
5,541/5,543 and release 5,542/5,544, each with zero failures and two known skips.
Older integration/failure/approval statements below remain historical
measurements. Complete P1 coverage remains unproven.

### Earlier checkpoints before this integration

**Earlier integration:** main `07b7de82` includes slang 12.1, the passive kernel,
and the separately applied nmos oracle migration (`6f301460`). Its pre-change
full local CI passes all 21 checks. The older checkpoint paragraphs below
retain their original validation and pending-review statements; these do not
mean the migrated nmos oracle still fails on current main.

**Source passive switches:** `76203da2` connects [all six bidirectional kinds](source-passive-execution.md)
through the same typed graph. All 143 original source designs and four presets'
224 focused checks pass; nine compiled faults are caught. The independent-graph
factorization keeps publication atomic, including 65 uncertain groups and
132-node snapshots. Final dev CTest has 5,532 passed, two known skips and two
obsolete tran-rejection failures. The [exact migration](passive-expectation-migration.md)
is proposed and unapplied pending human approval. The 87 original probes now
have eight Icarus-passing gaps against each reference, with four/seven reference
mismatches. Source comparison records preserve 110 matches out of 135 compiled
Icarus 13.0 designs (143 total), plus reduced array/scalar observations.
Force/ports/formatting and the broader P1 goal remain unfinished. This work is
committed locally, not integrated into main or pushed. The separate
[managed-event instruction-boundary safepoint amendment](managed-event-safepoints-proposal.md)
was approved as recommended option 1 on 2026-10-26 and incorporated into
RUNTIME_ABI 6 and automatic-capture 5/6/8. Complete root publication and its
acceptance matrix remain implementation work; source automatic-event construction
and the new collection timing are not enabled by approval alone. Older checkpoint
statements below retain their measurement-time approval status.

**Typed SSA event regions:** `53dbe0e1` installs [nonallocating active-region providers](event-temporaries.md)
using immutable typed word offsets. Caller roots survive nested calls/callbacks
and vector relocation, and regions retire on every body exit. Seven component
tests and eight compiled controls pass; all four presets pass 69 focused tests.
Final full dev CTest has 5,539 passes, two known skips and the same two historical
tran-rejection failures. Five-pair ordinary frame/event timings differ by
+0.3%/+0.5%, with every counter unchanged. Complete transfer/field publication,
SSA liveness, instruction collection and source automatic-event construction
remain work. Population, external probes and sv-tests were not refreshed for
this component; no source coverage reduction or full-P1 completion is claimed.

### Previous SIR checkpoint

**Passive-edge SIR/VM execution:** `9c663835` connects
[static edges, control ownership and conduction timers](passive-sir-execution.md)
to the existing shared graph. Seven new tests include 4,560 independent
interval/control/direction comparisons, malformed entry rejection, potential
uwire ownership and actual inertial control/data execution. Full dev CTest
passes 5,389/5,391 with zero failures and the two known skips. Source passive
switches, port identity, registered-net force and full strength formatting
remain work; no new source-probe gap reduction or complete parity is claimed.

### Earlier validation checkpoints

**Passive shared-kernel runtime:** `a2aa7573` adds [physical-origin graph worlds and conduction timing](passive-network-execution.md)
to the real scheduler/Store path. Nineteen tests compare 14,445 reference states,
including 46,980 destination intervals, rootless rings, shared-world correlation,
mixed wired kinds, 130-node resistance, potential uwire ownership and scalar
pending/timing/resource rules. Fifteen new graph faults and thirteen existing
origin faults all compile and are caught; restored production passes 300 focused
checks with the earlier directed-MOS sources, unchanged dumps and PerfCount.
Full local CI completes 17/21 checks successfully: all four preset test steps
fail only the unchanged historical nmos oracle in `cli/options`. Dev/ASan/UBSan
pass 5,380/5,383; release 5,381/5,384, each with two known skips. All 110 tidy
sources and the C++20/no-exception/no-RTTI public-header probe pass. Ordinary
frame/event counters match; five-pair timing is +1.1%/+2.4%, without graph traffic.
This is the user's requested stopping checkpoint. The work branch is based on
`62148877`; `origin/main` at `6432683f` includes a separate slang 12.1 merge,
whose integration and revalidation remain work. SIR/static-edge transport and
source passive switches, port identity, registered-net force and full formatting
also remain work.
The exact nmos expectation migration remains unapproved and unapplied.

**Directed MOS sources (partial):** `e7e2ec59` connects [six source MOS kinds](source-mos-execution.md),
owned ordinary contributions and separately captured controls/selectors to the
shared strength bank. All 154 new source designs, six positioned boundaries and
278 focused checks pass. Eleven compiled faults are caught; all mutated source
bytes are restored. The Icarus 13.0 run compiles all 154 sources: 113 complete
results match, while 36 full-width selector, three approved-startup and two
notif0 transition differences remain recorded. The last pair has a reduced
original reproducer; the reference implementation cause is not confirmed.
Full local CI completes 17/21 checks successfully; each of the four preset test
steps fails only `cli/options` (dev/ASan/UBSan 5,361/5,364, release 5,362/5,365;
two known skips each). The historical nmos rejection remains unchanged pending
the exact [expectation migration](mos-expectation-migration.md). Lint and policy
checks pass. Restored focused production passes 281 checks. Ordinary-path
timings/counters are recorded separately; new source MOS traffic is unmeasured.
Passive solving, registered-net force, partial/retained port identity and
full-interval formatting remain work. No whole-tool parity or P1 completion is claimed.

**Declared-network SIR execution:** `cea83470` and `faadf4b7` connect
[selected reads/shared waits and verified network SIR](strength-network-execution.md)
to the real Context-owned origin bank. Static declarations, initial ranges,
physical ownership, captured selectors, independent-node load/MOS/drive/delay,
separate sensitivity and cleanup now run through tier 1. Nineteen new tests
include 13,465 hand-built SIR executions and before-construction rejection of
malformed network input. Sixteen full-build compiled controls are caught;
restored production passes 125 cases including all 29 old dumps and PerfCount.
Full local CI passes all 21 checks: dev/ASan/UBSan 5,206/5,208,
release 5,207/5,209, zero failures and two known skips each.
Front/source MOS, passive graphs/solver, registered-net force/release, ports and
formatting remain work; no source-level Icarus gap reduction or P1 completion
is claimed.

**Physical origins and scalar timing:** `f93da11b` connects
[stable origin tables and inertial updates](strength-driver-execution.md) to the
shared scheduler and component publisher. Independent nodes resolve complete
ranges, including strength-only changes, without creating user processes.
Sixteen new tests include 12,600 reference comparisons, callback-driven circuits,
scalar deadlines, overflow and atomic failure. Thirteen compiled full-build
faults are caught; restored production passes 91 cases with all 29 unchanged
dumps and PerfCount. Full local CI passes all 21 checks: dev/ASan/UBSan 5,187/5,189,
release 5,188/5,190, zero failures and two known skips each.
SIR load/drive and static rows, source sensitivity, the passive solver,
registered-net force/release, ports, formatting and source MOS remain work.
No source-level Icarus gap reduction or full P1 completion is claimed.

**Complete net-strength publication and width bounds:** `49bd3069` connects
[Context intervals and atomic publication](strength-store-publication.md) to the
real Store/Scheduler, keeping range-only notices separate from ordinary values,
edges and VCD. `99dafab4` fixes wrapping word/byte counts across storage paths
and malformed huge-constant acceptance. Nineteen new tests and sixteen compiled
full-build controls pass; restored production passes 376 cases, including 29
unchanged dumps and 328 wide/force source cases. Full local CI passes all 21
checks: dev/ASan/UBSan 5,171/5,173, release 5,172/5,174, zero failures and two
known skips each. Two ordinary-path cost probes keep counters/RSS equal with
no additional regression visible in five alternating pairs. Physical origins,
SIR load/drive, dirty scheduling, passive solving and source MOS remain work.
No Icarus gap reduction or P1 completion is claimed.

**Owned strength SSA execution:** `0ec3cc57` connects
[typed make/MOS/projection](strength-ssa-execution.md) through the shared kernel
and VM. Thirteen original tests include 12,384 packed-MOS reference comparisons
and 2,034 hand-built VM designs, both parallel branch arguments and nested calls.
Nine compiled full-build faults are caught; all 67 focused cases pass, including
29 unchanged dumps. Full local CI passes all 21 checks: dev/ASan/UBSan
5,152/5,154, release 5,153/5,155, zero failures and two known skips each.
Net/driver stores, load/drive operations, scheduling, passive solving and source
MOS enabling remain work. No source-level Icarus gap reduction is claimed.
P1 remains incomplete.

**Approved indexed-event CLI migration:** `c53ccfe7` records the human decision;
`28f5c972` applies [the exact expectation-only migration](indexed-event-expectation-migration.md).
The complete historical source is unchanged, one acceptance entry is added and
only its six obsolete diagnostics removed. All 14 CLI tests and all 21 local CI
checks pass: dev/ASan/UBSan 5,139/5,141, release 5,140/5,142, zero failures and
two known skips each. This supersedes the pre-migration failures at the following
checkpoints, whose results remain recorded. The instruction-safepoint amendment
remains unapproved; automatic source construction and full P1 coverage remain work.

**Pure full-strength MOS operations:** `59522c50` adds
[all six MOS kinds and identity/nonresistive/resistive transfer maps](mos-strength-operations.md)
with independent point/control enumeration. Ten new tests cover all 120 intervals
and 11,520 device/control combinations; nine compiled full-build faults are caught,
and all 24 focused MOS/strength tests pass. Public interfaces compile as C++20
with exceptions/RTTI disabled. Full CI has 17 successful checks and four test
steps failing only the unchanged historical `cli/options` oracle: dev/ASan/UBSan
5,138/5,141, release 5,139/5,142, two skips each. Net/driver storage, SIR,
scheduling, passive solving and source enabling remain required work. No
new source comparison or Icarus gap reduction is claimed; P1 remains incomplete.

**Loaded event selections from source:** `2108dd72` connects
[runtime fixed-array targets and getters](indexed-event-execution.md) through the
shared typed place decoder. All 160 original designs and 16 policy boundaries
pass; four compiled faults are caught and the 192-case restored check passes.
Full CI has 17 successful checks and four failed test steps: after refreshing
generated counts, dev/ASan/UBSan pass 5,128/5,131 and release 5,129/5,132, with
only `cli/options` failing and two skips each. Its six historical unsupported
index diagnostics await [separate migration approval](indexed-event-expectation-migration.md).
The historical input and every existing expectation are unchanged. All 87
statuses/passing bytes remain unchanged; the 21 historical Icarus-13-passing gaps
remain. Installed Icarus-14 cannot compile the new event-array matrix, which
therefore cannot serve as a reference-value oracle. Automatic event construction
and the instruction-safepoint amendment remain work/unapproved. P1 is incomplete.

**Typed event execution in the VM:** `7e188c56` connects
[construction, holders, waits/triggers and actual object reads](event-vm-boundaries.md)
at existing region/slot collection boundaries. Eleven new execution tests and two
kernel tests cover ordinary/shared arrays, input/return, unread extents,
Active/Reactive pending targets and callback detachment. `39aa6aaa` fixes the
reporting regression found by full CI; `09b37d0c` reduces small-wait staging cost.
Thirteen compiled faults are caught and restored production passes 76 focused
cases. Automatic source construction and instruction-level collection remain
gated; the safepoint amendment is not approved. P1 remains incomplete.

The [final full CI](../measurements/2026-10-26/event-vm-validation.json) passes all
21 checks: dev/ASan/UBSan 4,967/4,969, release 4,968/4,970, zero failures and
two known skips each. All 87 native/Icarus-14 statuses and passing bytes are
unchanged, leaving 21 historical Icarus-13-passing gaps. A focused cost check
keeps counts/RSS equal, with ordinary transport about +0.5% and legacy named-event
roundtrip about +16.5% after reducing its initial +27% regression. This remaining
cost is recorded; no performance budget changed. Population/sv-tests were not rerun.

**Typed activation event providers:** `1dc06096` connects canonical event fields
to [retained lexical roots and ordinary root metadata](event-activation-roots.md).
Six new tests pass; five compiled faults are caught. Approved migration items
1/2 were applied separately in `ecbe4abb`; source item 3 is still pending.
The full CI passes all 21 checks: dev/ASan/UBSan 4,954/4,956, release 4,955/4,957,
zero failures and two known skips each. Fresh 87-probe statuses/passing bytes
are unchanged. Interpreter root publication, typed execution and source
construction remain work; explicit component collection does not prove bounded
no-wait execution. The event-only safepoint amendment is not approved.

**Typed SIR event values:** `efebd540` adds [event construction and loaded
wait/trigger operands](event-sir-contract.md). Eight tests cover 17 valid and
40 malformed designs; all 75 focused tests pass, and eight compiled faults are
caught. Format version stays 0. VM/root/front connections and the approved
three-oracle migration remain pending; no source-level gap reduction is claimed.

The [SIR checkpoint's full CI](../measurements/2026-10-25/event-sir-validation.json)
passes all 21 checks: dev/ASan/UBSan 4,948/4,950, release
4,949/4,951, zero failures and two known skips each.

**Runtime event ownership:** [typed kernel operations](managed-event-runtime.md)
now retain concrete targets through waits, NBA/Re-NBA delivery, monitors and late
callbacks. All 23 new tests pass; 16 compiled runtime faults were caught before
the allocator initialization correction, with exact hashes and an initial
coverage hole preserved. A separate compiled process-reuse fault verifies the
correction. SIR/VM/front automatic-event enabling remains work. The frozen
87 probes, population and sv-tests remain unchanged, leaving 21 historical
Icarus-13-passing gaps. Historical expectation migration was separately approved
and committed as `11096dc3`; it is included on main. P1 remains incomplete.

The [integrated full CI](../measurements/2026-10-25/event-runtime-validation.json)
passes all 21 checks: dev/ASan/UBSan 4,940/4,942, release
4,941/4,943, zero failures and two known skips per preset.
README counts come from the generator and the fresh dev JUnit report.

**Earlier managed-event checkpoint:** the [kernel registry and event-slot corrections](managed-event-registry.md)
add 20 tests, 27 original source designs and 12 implicit-function boundaries.
All 49 focused CTest cases pass. Sixteen compiled faults are caught, and a
separate production-width audit traverses all 4,294,967,295 generations; disabling
retirement fails the retired-object invariant after that full range. Source
automatic-event construction and VM-managed pin ownership are not enabled.
The frozen 87-probe comparison, population and sv-tests are unchanged, leaving
21 historical Icarus-13-passing gaps. The [full CI](../measurements/2026-10-25/managed-event-validation.json) passes
17 checks; four test steps fail only the same three old expectations.
Dev/ASan/UBSan pass 4,914/4,919 and release 4,915/4,920, two skips each.
The checkpoint note records exact resume steps. Expectation migration was pending
at that run; the later approval and migration are recorded above.

**Source automatic capture:** [front now emits complete lexical environments](automatic-front-capture.md)
in the working implementation. All 186 original source designs plus generator/metadata
checks pass, including retained unread typed fields and empty scopes; five compiled
faults are caught. Only `automatic_fork_capture` changes in the fresh 87-probe
comparison, to passed. Historical Icarus-13-passing gaps fall from 22 to 21;
population stays 1,451/1,466 and pinned sv-tests 244/344. Three historical
rejection checks needed [separate expectation approval](automatic-capture-expectation-migration.md),
subsequently recorded in `11096dc3`. The earlier [full local CI](../measurements/2026-10-25/front-capture-validation.json)
passes 17 checks and fails four test steps only on those three expectations:
dev/ASan/UBSan 4,865/4,870, release 4,866/4,871, two skips each.
All-green status and complete automatic event/P1 coverage are not claimed.

**Interpreter automatic environments:** `b17234e4` now executes valid represented
SIR homes through [the shared ownership/reference adapter](automatic-vm-execution.md).
Nineteen new execution tests cover joins, call/copyback, reentry, descendants,
ref/static/readonly bindings, guards, typed roots, cancellation and allocation
failure. Fourteen compiled faults are detected; the relocated birth-serial
control exposed an old coverage hole closed by a reclamation test. The
[full local CI](../measurements/2026-10-25/activation-interpreter-final-validation.json)
passes all 21 steps: dev/ASan/UBSan 4,680/4,682, release 4,681/4,683, zero failures
and two known skips. All 87 native/14.0 statuses/passing bytes are unchanged;
exact population stays 1,451/1,466 and pinned sv-tests 244/344. Five focused
ordinary-transport pairs show about +4.0% wall time with equal counters and
+16 KiB peak RSS. Front source capture, owned event/ref/object providers and
full Icarus coverage remain work; the historical 13.0 record still leaves 22 gaps.

**Scoped-reference component:** `589bd26b` preserves immutable declaring origins,
checks generation/rights and resolves actual field/array selections without
owning borrowed targets. The [implementation record](automatic-vm-scoped-refs.md)
adds 11 original tests and detects 13 compiled controls (nine new, four relocated).
The shared ordinary layout decoder now runs in the Interpreter; complete
ownership/reference attachment and source capture remain gated. The
[full local CI](../measurements/2026-10-25/scoped-reference-final-validation.json)
passes all 21 steps: dev/ASan/UBSan 4,661/4,663, release 4,662/4,664, zero failures
and two known skips. Fresh native/14.0 results preserve all 87 statuses/passing
outputs; historical 13.0 still leaves 22 gaps. Exact population stays 1,451/1,466
and pinned sv-tests 244/344. Focused ordinary transport cost is about +1.3% in
five pairs, with equal counters/RSS; this is not a full speed comparison.
Descriptor generation exhaustion remains not confirmed. P1 remains incomplete.

**Environment-component final check:** `615afa0d` passes
[all 21 local-CI checks](../measurements/2026-10-25/activation-environment-final-validation.json).
Dev/ASan/UBSan pass 4,650/4,652 and release 4,651/4,653, with zero failures
and two known skips. All ten compiled controls fail actual component tests.
The resource-backed object factory closes the initial object-allocation
injection gap; Interpreter/scoped-ref attachment and source capture remain work.
The finite comparison inventory, population and sv-tests are not remeasured here.

**Lexical environment component:** the [VM construction layer](automatic-vm-environment.md)
adds 21 direct tests for scoped ownership, immutable captures, inherited/unread
extents, reference rights, allocation rollback and typed roots. Ten compiled
faults are detected. Interpreter attachment and source capture remain gated;
this component alone closes no Icarus inventory gap.

**VM transport validation:** [all 21 checks, including the documented count-only retry](../measurements/2026-10-25/vm-frame-access-final-validation.json)
pass at `9b30d6d2`. Dev/ASan/UBSan pass 4,629/4,631; release 4,630/4,632,
with zero failures and two known skips. Nine compiled faults are detected;
36 new initial-override cases close the surviving control's coverage hole.
Native macOS sv-tests stays 244/344; the exact P1 population stays 1,451/1,466,
zero failures and 15 unsupported. All 87 judged statuses/passing outputs stay
unchanged against fresh 14.0; the earlier 13.0 counterpart still leaves 22 gaps.
The [focused cost](automatic-vm-frame-access.md) is about +6.4% wall time with
identical counters, not a new budget or a full speed comparison. P1 remains incomplete.

**VM frame transport:** the [access-path migration](automatic-vm-frame-access.md)
removes mutable word references from instruction, ref, file/scan and argument
operations. Seven original tests include 270 selection cases and 24 complete
SIR executions. Scoped bindings are tested directly; SIR environment construction
and source capture remain gated, with no Icarus gap counted as closed.

**Final automatic-SIR check:** `56a10e05` passes [all 21 checks](../measurements/2026-10-25/activation-sir-final-validation.json).
Dev/ASan/UBSan pass 4,585/4,587 and release 4,586/4,588, zero failures and two
known skips. All nine compiled verifier controls are detected. This is SIR
representation/verification only; the VM/source binding adapter remains work.

**Automatic SIR metadata:** the [representation/verifier checkpoint](automatic-sir-metadata.md)
adds sixteen original tests for scope homes, captures, rights, lifetime metadata
and ownership graphs. Initial dev passes 4,585/4,587, zero failures and two known
skips. New homes remain explicitly gated in VM until its full binding/access
adapter is ready; no source capture or Icarus gap is claimed closed.

**Final automatic-wait validation:** `efb3f3d1` passes [all 21 checks](../measurements/2026-10-25/activation-wait-final-validation.json).
Dev/ASan/UBSan pass 4,569/4,571 and release 4,570/4,572, zero failures and two
known skips. Seven compiled controls are detected. Fresh native macOS sv-tests
is 244/344; the exact pinned population is 1,451/1,466, zero failures and 15 unsupported.
All 87 engine statuses and passing judged bytes remain unchanged against both
references. Source capture and full P1 completion remain work.

**Automatic-storage wait dependencies:** the [kernel connection](automatic-storage-observation.md)
adds fourteen original tests for owned extents, dynamic/static ranges, edges,
heap replacement, cancellation and resource failures. After the initial-evaluation
fix, dev validation passes 4,569/4,571 with zero failures and two known skips. Source capture stays
disabled; this does not close the 22 remaining passing gaps or complete P1.

**Final automatic-storage foundation check:** `f7c45c43` passes [all 21 checks](../measurements/2026-10-25/activation-foundation-final-validation.json):
4,555/4,557 dev/ASan/UBSan and 4,556/4,558 release, zero failures and two known skips.

**Automatic-storage foundation:** the [kernel layer](automatic-storage-foundation.md)
adds 14 original tests, eleven compiled controls and a full 32-bit-generation
exhaustion probe. Source capture remains disabled; this foundation does not
claim to close any of the remaining 22 passing gaps.

**Final fixed-array validation:** `d89a60c6` passes [all 21 checks](../measurements/2026-10-25/fixed-array-final-validation.json):
4,541/4,543 dev/ASan/UBSan and 4,542/4,544 release, zero failures and two known skips.

**Fixed-array port implementation (2026-10-25):** the [execution record](fixed-array-port-execution.md)
covers 358 sources, four SIR tests, fourteen boundary checks and eight compiled
negative controls. The same 87 probes now have 22 passing gaps per reference
and unchanged four/seven reference mismatches. The proposal's 59 designs now
have 51 matches, seven legality rejections and one unsupported partial-array
port. macOS sv-tests remains 244/344; population remains 1,451/1,466, zero failures
and 15 unsupported. Pre-implementation statements below retain their checkpoint dates.

**Further approval (2026-10-25):** the user approved all six remaining detailed
[execution contracts](p1-execution-contracts-2026-10-25.md). They are normative
amendments now; implementation proceeds separately, starting with complete-state
delayed three-state gates. Earlier pending-status statements below retain their
checkpoint provenance. Existing expectation changes still require separate review.


Source `20274407` now implements [complete delayed three-state propagation](delayed-gate-state-execution.md).
All 240 original source cases and three direct tests pass; eight compiled
controls are caught and restored sources pass 345 selected tests. Nine quality
checks and the four-configuration execution checks pass. The full observed
CTest sets retain **one failure**, an obsolete existing rejection oracle;
its expectation-only migration is separately awaiting approval. The stronger
12-case draft passes in every configuration. Eight new ordinary gate cases
pass both references and native, and all eight fail the retained native baseline.
This does not claim P1 completion or completion of the other five contracts.

**Oracle integration (2026-10-25):** the user separately approved the exact
migration and `2b38b984` integrates it. The [full integration record](../measurements/2026-10-25/delayed-side-integration-validation.json)
passes all 21 local-CI checks: zero failures, 4,177/4,179 dev/ASan/UBSan and
4,178/4,180 release, with the same two known skips. The earlier one-failure
paragraph above is the pre-approval checkpoint, not the current state.

The approved [multidimensional binary input](multidim-fread-execution.md) and
[runtime array methods](runtime-array-methods.md) now execute. The latter has
270 original source cases, four direct unit tests and 36 explicit key-diagnostic
cases. This includes six in-place-write key cases added after a compiled
snapshot-omission control exposed that deletion-only tests were insufficient.

The unchanged 87 original probes now have **24 passing gaps against each
Icarus version**, down from 25 (13.0) and 31 (14.0 development), with the same
four/seven reference mismatches. Ten native observations change from failure
to passing; older passing observations and all reference statuses/expected
outputs are retained. These finite probes do not establish complete P1.
The [final validation](../measurements/2026-10-25/approved-array-validation.json)
passes all 21 local-CI checks. Dev/ASan/UBSan pass 3,933/3,935 and release
3,934/3,936 with the same two skips. All nine compiled controls are caught.
The pinned macOS sv-tests result grows 234 → 244/344 with no pass regression;
the quantitative gate's Icarus 14.0 `04cc493` passes 210. P1 population remains
1,451/1,466, zero failures and 15 unsupported. Linux was not rerun. Historical
results below retain their recorded revisions.


`2ad73ae` adds [bit queries on fixed unpacked aggregates](aggregate-bit-queries.md)
through the existing shared streaming/value path. All 136 new source cases
pass, including wide two/four-state leaves and single receiver/control
evaluation. Dev now has 3,500 passes out of 3,502, with the same two known
skips. Automatic arrays of records and record refs remain storage gaps;
dynamic/string bit streams and source `%p` remain unfinished.

The [2026-10-25 comparison recheck](icarus-13-comparison.md#earlier-query-and-monitor-recheck-2026-10-25)
retains all 87 previous engine statuses and passing outputs: there are still
25/31 Icarus-passing gaps and four/seven reference mismatches. Two compiled
[omission controls](../measurements/2026-10-25/p1-coverage-negative-controls.json)
fail actual new source/kernel tests; restored sources rebuild successfully.

The [full local-CI record](../measurements/2026-10-25/p1-coverage-validation.json)
for source `2ad73ae` passes all 21 checks. Dev/ASan/UBSan have 3,500 passes
out of 3,502; release has 3,501 out of 3,503. Each retains the two known
skips. Existing expectations, all 29 SIR dumps and PerfCount budgets are
unchanged. These results do not approve the pending semantic proposals.

The [2026-10-25 heap file-monitor follow-up](heap-file-monitor-lifetime.md)
adds 20 original source cases and two scheduler tests, and fixes killed
display callbacks retaining their registration and heap dependencies.
At that monitor checkpoint, the dev suite had 3,363 passes out of 3,365,
with the same two known skips.
The versioned Icarus gap counts below are unchanged; full P1 coverage and
the pending semantic amendments remain work.

The [typed-pattern SIR/execution checkpoint](pattern-sir-execution.md) also fixes
display-monitor dependencies on existing queue cells. All 122 selected tests,
including eleven new SIR tests, and all 21 CI checks passed at that earlier
checkpoint. It had 3,340 passes out of 3,342 in dev/ASan/UBSan and 3,341 out
of 3,343 in release, with the same two skips. Source `%p` front integration
remains. Comparison counts below retain their earlier provenance.

The current implementation includes [wide UDP input evaluation](wide-udp-inputs.md),
[fork-local declarations](fork-local-declarations.md),
[wide L/H strength masks](wide-gate-strength.md),
[aggregate blocking timing](blocking-aggregate-timing.md),
[aggregate value capture](aggregate-value-capture.md),
[blocking repeated event controls](repeated-event-controls.md), corrected
[integral binary input](../P1_FEATURES.md#integral-binary-input-with-fread)
and the [complete strength-interval kernel API](strength-range-foundation.md).
The strength API remains a foundation: MOS/switch source execution and `%v`
are unfinished. The [supported inventory](../P1_SUPPORT.md) lists tested forms
and limits. P1 remains incomplete; C++ generation remains P3.

`156e65c` adds a [shared-kernel assignment-pattern renderer](pattern-format-foundation.md).
Fourteen new unit tests and all 22 older Format tests pass. Typed SIR/execution
followed at `4c69843`; front integration remains. Source `%p` is still
unsupported and no 87-probe gap is claimed closed by this API foundation.
The strengthened 112 enum combinations and six compiled controls pass their
checks. At that renderer stage, full CI had 3,329 passes/two known skips out
of 3,331 in each of dev/ASan/UBSan; release had 3,330/two out of 3,332. All 136 Tools and
required LLVM-22 checks pass, and all 513 control entries apply. See the
[hashed validation record](../measurements/2026-10-24/pattern-format-negative-controls.json).

The latest [MOS/switch investigation](mos-switch-investigation.md) records
28 original programs and 56 observations across both reference versions.
It identifies strength-only dependencies, passive cycles, port attenuation
and unresolved interval/force/startup questions. These exploratory observations
are separate from the feature inventory below and do not approve new semantics
or add implemented support.

The UDP implementation at `52652eb` passes 106 new source cases and five
new unit tests. Four compiled controls pass the older four UDP tests and fail
new source stdout or wide value assertions. The old level-row mutant follows
the new helper; all 507 registered controls apply.

The earlier UDP local CI passes with native source `52652eb` and the registry/count
follow-up (2026-10-24). Dev/ASan/UBSan each run 3,317 tests: 3,315 pass and
two known skips remain. Release runs 3,318: 3,316 pass and the same skips.
All 136 Tools tests, 106 new source cases and five new unit tests pass.
Release flags, banned features, SPDX, suite files, global state, expectation
protection, README counts, clang-format and clang-tidy pass. Existing suite
bodies/expectations, performance counts and all 29 SIR dumps are unchanged.

The [class investigation](class-execution-investigation.md) records 46 original
programs separately: native has one model match, 42 unsupported forms, one
output mismatch and two legality rejections. A known omission of unused class
static-initializer side effects corrects the earlier blanket diagnostic claim.
Reference failures/differences are not adopted as oracles. This neither changes
the 87-item inventory nor adds class implementation or approved contracts.

At the wide-UDP checkpoint (`52652eb`, 2026-10-24), the same **87 original
probes** ran against both installed references; the current counts are 22/22
(see above):

- Icarus `13.0 (stable) (v13_0)`: 25 Icarus-passing gaps, four reference
  result mismatches.
- Icarus `14.0 (devel) (s20260301-411-gca6b8d376)`: 31 Icarus-passing gaps,
  seven reference result mismatches.

See the [versioned comparison and portable data](icarus-13-comparison.md).
The compiler driver, preprocessor, parser/elaborator and runtime were all
verified as 13.0 for that baseline. This measures the finite 13.0 inventory;
complete coverage, full parity and Linux results for these probes remain
unconfirmed. All earlier 84 statuses and passing judged outputs are unchanged.
The new combinational/sequential wide UDP probes pass both references and native
execution. A new dense table still reaches the native 15-second subprocess bound.
The expanded baseline has 27/33 gaps and the native at that checkpoint
25/31; it combines preserved 84-item records and three fresh probes on the
same retained source. The earlier 84-item cohort retains 24/30 gaps. This
closes two newly inventoried mechanisms and exposes a compiler limit, without
changing older cohort results.

The reduction/ordering, multidimensional input and extrema amendments were
approved on 2026-10-25 and now have the tested subsets linked above.
The following six detailed contracts were approved on 2026-10-25; their complete
implementation remains work:

- [Event-controlled NBA registration, lifetime and ordering](event-nba-proposal.md).
- [Delayed three-state side-only transitions](delayed-gate-side-proposal.md).
- [Full-strength MOS, passive networks, ports, force and formatting](mos-switch-proposal.md).
- [Shared automatic storage, fork capture and automatic event ownership](automatic-capture-proposal.md).
- [Whole fixed-array ports, alias views, element-wise resolution and pulls](fixed-array-ports-proposal.md).
- [Class identity, initialization, dispatch, typed ownership and object state](class-runtime-proposal.md).

The MOS proposal covers thirteen switch probes and one partial-port probe in
the existing 13.0 gap list. Its new 18-source matrix records 2,304 scalar
samples per reference across three loading conditions, plus ten original
boundary programs. Loaded results agree with the truth/reduction/resolution
model; isolated resistive strength tokens expose a context-dependent reference
difference. The proposal explicitly requests review of startup, network
ownership, local wired resolution, force and strength-only timing. No MOS
implementation is claimed by that proposal; the independent fork work extends
the feature inventory as described above.

The automatic-storage proposal replaces planned per-child copies with shared
activation bindings and specifies lexical lifetime, typed roots, synchronization,
copyback, ref restrictions, automatic event construction and cleanup in the
shared kernel. Its [32 original source observations](../measurements/2026-10-24/automatic-capture-design-observations.json)
include native parse/run results and both references. They are design evidence,
not approved oracles. Event rebinding and event output/inout copyback remain
outside that proposal; at that checkpoint (2026-10-24), no production
implementation had been added. Later checkpoints implemented automatic-storage
layers.

The [array-port proposal](fixed-array-ports-proposal.md) records
[59 original design programs](../measurements/2026-10-24/fixed-array-port-design-observations.json).
Strict slang accepts 52 and rejects seven legality controls. Each reference
matches 36 legal model outputs, rejects 15 legal sources and differs on one;
34 model-matched sources remain native gaps. Native passes four controls and
rejects the other 48 legal examples. Explicit input-var reevaluation differs
in the references; their acceptance of two illegal writer programs does not
justify weakening slang. At that checkpoint (2026-10-24), no array-port or
resolved-array implementation had been added; later work implemented
[fixed-array ports](fixed-array-port-execution.md).
The earlier array-port documentation/probe follow-up passed all 135 Tools tests, source-hash
checks, passing-output comparisons and changed-document file-link checks.
At that step, production source and expectations were at `c6d9c3d`, with
full four-configuration CI at `392687e`. The subsequent UDP source is described above.

The [UDP record](wide-udp-inputs.md) separates 106 passing native source cases
from 54 dense source variants blocked by pinned slang coverage checking.
Both references match 25 of the runnable cases; their remaining compilation,
execution and value differences are preserved. Direct kernel/reference checks
also cover 65,537 inputs, without claiming source compilation at that size.
The [bounded compilation data](../measurements/2026-10-24/udp-edge-coverage-observations.json)
and [upstream issue draft](slang-udp-coverage-issue-draft.md) preserve the unresolved
frontend behavior. No upstream issue/comment has been sent.

The broader development-version cases remain tracked. For external pass
rates, macOS sv-tests and the population were remeasured on 2026-10-25; the
nightly run measured Linux sv-tests at 234 on 2026-10-24. Performance
measurements are separate records from 2026-10-23 (macOS) and 2026-10-24
(WSL2); these source changes have not been timed there. See
[benchmark conditions](../BENCHMARKS.md).

```sh
python3 tools/icarus_feature_probe.py --kirimasim build/dev/src/driver/kirimasim \
  --extended --output build/p1-feature-probes.json --require-parity
```

This uses PATH; the [13.0 command](icarus-13-comparison.md#reproduce) selects
matching binaries explicitly. Both measured references still give exit 1
for remaining passing gaps. A zero result would not establish full parity.


The [class runtime proposal](class-runtime-proposal.md) adds a detailed contract
and fourteen original boundaries beyond the preserved 46. Each reference has
four model matches, six compile failures, three runtime failures and one
unjudged observation; native has twelve unsupported forms and two mismatches.
At that checkpoint (2026-10-24), no class implementation, contract approval or
87-item inventory expansion was claimed. The contract was approved on 2026-10-25.

## Wide L/H side masks and the delayed boundary (2026-10-24)

`37a2726` implements the already approved two-state side-mask representation
without the old 32-bit front restriction. The VM preserves both halves at the
full net width and parses mixed driver tuples; the front tracks their actual
control-flow block. The [73-source matrix](wide-gate-strength.md) covers four
three-state gates, six widths through 257 bits, both net directions and
module/generate/interface signals, plus short-circuit function controls.
All 73 pass the current native binary and both measured Icarus versions;
all 73 stop as unsupported on the retained old binary.

An independent SIR test covers eight widths, both weak background values and
both driver orders. Four compiled corruptions fail new source stdout, with
three also detected by the independent SIR assertions. Existing expectations
and all 29 SIR dumps remain unchanged.

`57a64ef` preserves the unreviewed delayed-side boundary in all blocks. Six
positioned rejections and six undelayed unresolved-net acceptances cover
widths 1/65/257 and function controls; reverting the guard to the entry block
is detected by this real CLI test. All five compiled controls exclude
anchor/judging self-check failures as evidence, and restore/rebuild source.

On the same 79-item inventory, 13.0 gaps change 21 to 20 and development gaps
27 to 26. The other 78 statuses and four/seven reference mismatches stay
unchanged. [Versioned portable records](icarus-13-comparison.md) include the
old native results; historical 75/78-item files are preserved. This does not
establish complete 13.0 coverage or remeasure performance.

The [delayed-side proposal](delayed-gate-side-proposal.md) records six
exploratory observations, distinguishes ordinary value transitions from
side-only timing and specifies complete-state cancellation/atomic updates.
Its experimental implementation was removed pending approval. MOS/switches
and arbitrary resolved-interval propagation remain unfinished.

## Blocking aggregate timing and positional copies (2026-10-24)

Implementation `6195846` adds blocking intra-assignment delay/event/repeat for
whole fixed arrays, unpacked structures and structure arrays, using the
existing approved rules and private typed frames. It also corrects structure
array copies to match each side's own declared bounds. The
[74-source matrix](blocking-aggregate-timing.md) adds eight contexts, wide
two/four-state payloads, concurrent automatic ref calls, container roots,
guarded events, large counts and invalid targets. The retained old binary
rejects 72 and miscomputes two; all new cases pass. Five compiled mutations
are detected by their new stdout expectations, excluding self-check failures.

The historical CLI input with six delayed array statements stays byte-identical.
Its obsolete unsupported expectations are retired separately in `36aa2ae`,
without implementation changes. The implementation review uses that branch
as its base and passes the expectation-protection check. The combined tested
stack is `396131e`; no old suite or performance oracle is weakened there.

The same 78-probe inventory is measured on both references: 20/four gaps/
reference mismatches for 13.0, 26/seven for development. The earlier 75 retain
all statuses; the three additions pass kirimasim and fail reference compilation.
Their [portable records](icarus-13-comparison.md) are separate from the earlier
75-probe artifacts. These are new execution forms, not a measured reduction
of Icarus-passing gaps.

An [event-NBA amendment](event-nba-proposal.md) now specifies registration,
count coalescing, origin/RNG lifetime, pending queue order and a shared SIR/kernel
contract. Fifteen original programs have 30 versioned reference observations.
The proposal remains pending; its observed overlapping-item and NBA-order
differences are not adopted as oracles. Full P1 coverage remains unproven.

## Aggregate capture and exact-13.0 baseline (2026-10-24)

Source `917c715` restores the already approved RHS-first rules for ordinary
aggregate assignments and NBAs, and snapshots container leaves before
later field/selector effects. It adds runtime fixed-subarray targets and
fixed-array members in hidden function-return storage through existing SIR.
The [72-source matrix and validation](aggregate-value-capture.md) cover eight
contexts and 1/32/65/129-bit two/four-state payloads. The retained old binary
fails 64 outputs and rejects eight forms; the corrected cases pass.
Nine compiled aggregate mutations are caught without anchor/judging
self-checks, including eight caught by new stdout expectations. Restored
sources rebuild before full CI. No semantic amendment is needed to restore
VALUE_SEMANTICS 3.1/3.13 and SIR_SPEC 7.3.

Three comparison probes expand 72 to 75; both references reject these
forms while kirimasim passes. Two probes explicitly test the project
RHS-order policy, and one tests a standard runtime subarray target. The
existing gap counts are unchanged. The new 13.0 and development records
measure different baselines; their 20/26 and four/seven counts are not a
cross-version improvement. Event-controlled NBAs still require independent
registration and remain unsupported. Full P1 support remains unproven.

## Blocking repeated event controls (2026-10-24)

Source `87ae3a2` implements the existing approved SCHEDULER_SPEC 8/9.4.5
rule with frame-held counts and ordinary SIR waits. It adds no opcode,
dependency or scheduling choice. Seventy-five new source cases cover eight
execution contexts, integral/real/string/container snapshots, typed counts,
late targets, mixed events and `iff`. All pass. Forty-eight match Icarus;
26 cannot compile there and one narrow-count case reaches its watchdog.
Three deliberate compiled mutations fail actual new stdout expectations;
anchor/judging self-check failures are not counted as semantic evidence.
See the [execution and validation note](repeated-event-controls.md).

Four new comparison probes expand the inventory from 68 to 72. On the same
72, the old binary reporting `51cafd204560` has 29 Icarus-passing gaps and
the new source has 26; the original 68 retain their 26 gaps. A one-bit X/Z
count probe follows the no-wait rule of 9.4.5: kirimasim prints `7 9`, while
Icarus prints `0 0`. It is the seventh reference mismatch, not a passing gap.
No existing oracle is weakened. Event-controlled NBAs still need asynchronous
registration and remain unsupported. Full P1 parity is not established.

## Complete strength-interval resolver foundation (2026-10-24)

Source `30a6ebc` exposes the complete interval result already specified by
VALUE_SEMANTICS 5.2–5.4. It adds no resolution policy or semantic amendment.
The previous kernel computed a hull but returned only 0/1/X/Z. Ranges such
as `[3,6]` and `[6,6]` both project to 1, so a value-only interface loses
information needed by MOS/switch propagation. The
[API and verification note](strength-range-foundation.md) describes the scope.

The allocation-free C++20 kernel now provides `StrengthRange`,
`resolveStrengths`, `resolveStrengthBit` and explicit `strengthValue`
projection. Arbitrary intervals, including ambiguous strengths entirely
on one side of zero, share the same point-state hull and wired-tie rule
as driver-bit resolution. Existing `resolveBit`/`resolve4` value interfaces
delegate to that calculation. The independent `valsem::resolveSignals`
enumerates point choices rather than using the kernel algorithm.

Five new unit tests cover all 120 valid intervals, 158,400 interval-pair
comparisons under eleven net kinds, empty/single intervals, 11,264 complete
driver/strength/side-mask cases, 512 seeded triples in six permutations and
16,384 masked wide-word bit cases at default fuzz scale. The existing nine
strength tests remain unchanged and pass alongside the new five. The public
kernel-header C++20 target builds. A standalone C++20 caller also compiles
without exceptions/RTTI and preserves `[-7,-6]` while reading it as logic 0.

Four deliberate mutations compile successfully and pass the prior nine
value-only strength tests but fail actual new range assertions. They collapse
either same-value endpoint, keep only the highest input point or ignore later
inputs. In particular, the endpoint collapses keep the four-state result
unchanged while corrupting strength information. Restored sources rebuild
and all fourteen strength tests pass. The local focused logs are separate
from anchor/judging-tool self-checks, which are not semantic evidence.

Full local CI at `30a6ebc` passes all four configure/build/test presets,
release flags, banned features, SPDX, suite files, global state, expectation
protection for all ten unpublished commits, README counts, clang-format and
clang-tidy. Dev/ASan/UBSan each run 2,831 tests (2,829 pass, two known skips);
release runs 2,832 (2,830 pass, the same skips). Unit tests now total 298;
suite/tool/generator counts remain 2,346/128/123. No existing suite or
performance expectation was changed.

The comparison rerun with a binary reporting `30a6ebc` retains 68 probes,
26 Icarus-passing gaps and six reference mismatches in
`build/strength-range-icarus-probes.json`. Installed Icarus compiler/runtime
both report `14.0 (devel) (s20260301-411-gca6b8d376)`.
`--require-parity` returns 1 for the remaining gaps; exact-13.0 and complete
P1 parity remain unconfirmed. No source-level MOS gap is counted as closed.
Matching the original cohort by the 44 probe names in tool revision
`50911e1` still gives 19 gaps, with no missing names. Probe additions can
change their list positions, so that historical cohort is identified by name.

The source-level VM still consumes projected values. Resolved net-interval metadata,
strength-only change propagation, MOS control/attenuation, bidirectional
networks and `%v` integration remain unfinished. This foundation does not
close those language gaps or change the full P1 target. Missing integration
contracts still require review; the existing three semantic proposals remain
pending. Historical population, performance and nightly measurements keep
their recorded dates and revisions.

## Integral `$fread` copyback contract restored (2026-10-24)

Source `6b39508` restores the already approved SIR_SPEC 8 argument rule;
it does not introduce a new semantic amendment. System-function frame outputs
copy back as blocking assignments, and an inout actual is evaluated for its
input value and again for a written output. This is the project's approved
4.9.7 interpretation. SYSTEM_TASKS 6.3 supplies the integral partial-read rule
of 21.3.4.4. The early capture in `db40e27` conflicted with that contract.
The [correction rationale](fread-copyback-correction.md) records the error,
the independent evidence and the separate review boundaries.

The expectation-only branch `codex/fread-copyback-expectations` at `2911a39`
corrects nine mechanism oracles and raises the generator to version 2. All
144 width/state/context value expectations remain unchanged. Two additional
checks inside the mechanism cases test valid heap-element copyback at the
new index and preservation of the input value across a later argument's
mutation before partial EOF. With the old binary, all nine corrected cases
fail through stdout differences; the 144 value cases and generator check pass.

The dependent `codex/fread-copyback-implementation` branch reads the actual
for copy-in, evaluates descriptor/start/count values once left to right,
performs the shared byte input, and finds the target again only for a positive
byte count. Direct queue-element expressions therefore select the variable's
current value at copyback; a real ref actual keeps its established identity.
On EOF/no write there is no copyback. `$fgets`, scans and plusargs keep their
existing output-only copyback behavior. Kernel, VM and SIR encoding are unchanged.
The corrected source passes all 154 focused source/generator tests.

Two old negative controls were invalid: evaluating an inout selector for
both phases and selecting from a replaced destination at copyback are not
semantic defects under SIR_SPEC 8. Those earlier catches are retracted as
conformance evidence. The corrected seven controls all build and fail actual
source assertions: rejecting extra arguments, dropping/reversing optional
argument effects, evaluating copy-in twice, delaying copy-in until after
optional arguments, clearing unfilled old bits and copying back at EOF.
One is caught as unsupported execution; six are stdout differences. Restored
sources rebuild and all 154 focused tests pass; anchor/judging self-checks
are excluded from the evidence.

Full local CI at `6b39508` passes all four configure/build/test presets,
release flags, banned features, SPDX, suite files, global state, the source
commit's expectation guard against its review base, README counts,
clang-format and clang-tidy. Dev/ASan/UBSan each run 2,826 tests (2,824 pass,
two known skips); release runs 2,827 (2,825 pass, the same skips). The previous
144 reference value comparisons remain valid; they did not test the
incorrect phase-sensitive oracle. An additional original partial-input probe
starts with `a[0]=24'h123456`, `a[1]=24'h654321`, i=0 and one input byte 81.
`$fread(a[i++],fd,0,0)` returns 1, leaves i=2 and produces
`a[0]=123456`, `a[1]=813456`, preserving the input actual's old low bits.

The comparison was rerun with a binary reporting `6b39508`:
`build/fread-copyback-icarus-probes.json` retains 68 probes, 26 Icarus-passing
gaps and six reference mismatches. The phase-sensitive direct targets are
not newly passing Icarus comparisons. Installed compiler/runtime remain
`14.0 (devel) (s20260301-411-gca6b8d376)`; `--require-parity` returns 1.
Exact-13.0 and complete P1 parity remain unconfirmed. Historical external
population, performance and nightly measurements keep their dates/revisions.

The isolated source review uses the expectation branch as base and changes
no existing suite body or expected file; the expectation correction itself
contains no `src/` change. Publication-boundary inspection also confirms all
459 scalar-fread matrix files are new to `origin/main` (`aae29de`), so the
new-test exception permits the unpublished feature's complete corrected diff
in one PR against that original base. The published-base expectation guard
passes. The review branches are retained, and the validated correction is
integrated into local main without publishing a PR or pushing. Existing
approved timing rules are restored; the three pending proposals remain pending.

## Array-extrema contract audit and comparison correction (2026-10-24)

The next required mechanism check found a distinction between Icarus execution
and the standard oracle. IEEE 1800-2023 7.12.1 was checked locally: empty
min/max input returns an empty queue; a nonempty input returns one selected
element, including tied extreme keys. The earlier observation that Icarus
returns both tied elements remains a valid measurement, but it is not the
standard expected result. The older paragraph below now marks that distinction.
No existing simulator expectation has been changed to imitate the reference.

Tool revision `8c55153` appends seven original probes without changing the
previous 61: unique minimum/maximum, duplicate minimum/maximum, a projected
key, and empty dynamic/queue input. On int values `[4,7,6]`, the installed
Icarus returns one minimum 4 and maximum 7, while kirimasim diagnoses the
methods as unsupported. Empty input gives Icarus an empty result. These four
probes confirm missing execution support independently of the tied-key cases.

For `[4,7,4]` and `[4,7,7]`, Icarus returns two results where one is required.
For `[4,7,6].min() with (-item)`, it returns 4 where selecting by the key
requires the original element 7. The latter three cases are reference
`result_mismatch` records, not Icarus-passing gaps. The pinned slang MIT
constant evaluator has a one-element min/max result path; that source
observation is not a completed test of constant/parameter forms. No Icarus
or Verilator source or tests were read.

The finite inventory now has **68 probes, 26 Icarus-passing gaps and six
separate reference result mismatches**. The previous 61 retain 22 gaps; the
four added passing cases account for the increase to 26. The other three
reference mismatches retain their prior inputs: `nested_packed_select_bounds`,
`class_virtual_dispatch` and `dynamic_array_queries`. All per-engine statuses,
outputs and diagnostics remain in the report. This is an expanded inventory,
not a runtime regression or a full-coverage proof.

The JSON report now explicitly includes `reference_mismatches`, alongside
`gaps`. The `--require-parity` exit contract still checks only Icarus-passing
gaps: a zero exit cannot certify that the simulator or the reference meets
every oracle. New judging-tool tests cover a confirmed gap, a wrong reference,
both engines wrong and both correct, plus a reference-only selection. They
preserve independent statuses and prevent reference mismatches being counted
as passing gaps. All 128 dev Tools tests pass, including these regressions.
No native implementation changed in this audit. The last four-preset native
CI record remains `db40e27`, not a new four-preset run at the tool revision.

The local result is `build/extremum-icarus-probes.json`; its simulator binary
reports `db40e27`. Both installed Icarus compiler/runtime report `14.0 (devel)
(s20260301-411-gca6b8d376)`. `--require-parity` returns 1 for the remaining
gaps. Exact-13.0 and complete P1 parity remain unconfirmed. Population,
performance, nightly and CTest counts retain their measured dates/revisions.

```sh
python3 tools/icarus_feature_probe.py \
  --kirimasim build/dev/src/driver/kirimasim --extended \
  --output build/extremum-icarus-probes.json --require-parity
ctest --preset dev -L Tools --output-on-failure
```

The [extrema proposal](array-extrema-proposal.md), recorded in `b8abd43`, requests review of traversal,
tied/unordered keys, source snapshots and exactly-once effectful key evaluation
before implementation, as P1_SCOPE requires for missing semantic details.
The separate reduction/ordering and multidimensional-input proposals remain
pending; this audit does not approve them or enable the runtime methods.

## Integral binary-input arguments and captured targets (2026-10-24)

**Correction:** the capture-once target policy at this historical revision
was unapproved and contradicted SIR_SPEC 8. The restoration above supersedes
that policy and the two invalid target-timing mutation labels below.

Source `db40e27` completes integral `$fread` calls with optional start/count
arguments (21.3.4.4). Their values do not restrict an integral read, while
their supplied expressions execute once left to right under VALUE_SEMANTICS
3.1. Empty optional arguments are accepted by strict slang. The existing
kernel syscall and binary-input operation remain shared and unchanged.

At that revision, the destination was captured before later arguments and
reused at copyback, including stable heap cells. That incorrectly replaced
the approved copyback-time target evaluation. Partial reads and zero-byte
copyback suppression are retained by the correction, with the target found
at the approved phase.

The [new matrix](../../tools/gen_scalar_fread_matrix.py) adds 153 original
source cases: 144 width/state/context combinations and nine mechanism cases.
Widths are 1/7/8/9/32/64/65/129, in two/four states. Nine contexts include
module initials, automatic/static tasks, forks, interfaces, packages,
programs, generate and functions. Cases check ignored negative/zero/unknown
values, omitted start, file position, partial EOF, target evaluation,
ref/packed/concat/heap storage and held identities. Intentional argument
side effects retain strict slang's positioned warnings. Existing suite
bodies, expectations and performance counters are unchanged.

All 144 value combinations also match the installed Icarus's output. For
these reference-only runs, the empty-start spelling is replaced by explicit
zero, because Icarus rejects the empty argument; expected output is unchanged.
Kirimasim's registered cases retain the original standard-permitted spelling.
The reference comparison does not establish the separate ref/side-effect
mechanism cases or override the project's evaluation-order contract.

Historically, seven mutations built and triggered the old assertions. The
two labeled as evaluating the target twice and following a replaced heap
binding were valid under the approved contract, so these catches cannot
establish conformance. The corrected oracle and controls above replace this
invalid target-timing evidence. The runnable scalar-value example and its
`2 8112` output in both engines remain valid.

Two probes were appended without changing the original 59. With the
unchanged baseline binary reporting `b78437a`, these 61 probes have 23
Icarus-passing gaps; the new integral call passes after the fix, leaving 22.
The original 59 retain 21 gaps; the additional remaining gap is whole
multidimensional `$fread`, now included in the inventory. This inventory
extension must not be mistaken for a regression from 21 to 22 or for a
complete coverage proof. Exact-13.0 and complete parity remain unconfirmed.

The final comparison binary reports `db40e27`; compiler/runtime both report
`14.0 (devel) (s20260301-411-gca6b8d376)`. Local before/after reports are
`build/scalar-fread-before-icarus-probes.json` and
`build/scalar-fread-after-icarus-probes.json`. `--require-parity` returns 1
because the remaining gaps are real, not a successful full-parity check.

Full local CI at `db40e27` passes dev/release/ASan/UBSan configure/build/test,
release flags, banned features, SPDX, suite files, global state, expectation
protection for both unpushed commits, README counts, clang-format and
clang-tidy. Dev/ASan/UBSan each run 2,826 tests (2,824 pass, two known skips);
release runs 2,827 (2,825 pass, the same skips). Counts now include 2,346
suite cases, 123 generators and 128 tool tests. Historical population and
performance dates/revisions are unchanged. All 265 checked local links in
the updated documents and new proposals exist.
This mechanically green run did not detect the incorrect target-timing oracle.

The [multidimensional-input proposal](fread-multidim-proposal.md) in
`4af9ff0` records numerical-index traversal, linear start/count and
cross-row original observations. Its address interpretation awaits review
under P1_SCOPE before implementation. The separate
[array-method proposal](array-methods-proposal.md) also remains pending.
Classes, VPI, switches, specify/timing and the other P1 gaps remain work.

Exploratory inputs outside those 61 probes also expose dynamic-array
`min()` and queue `max()` gaps (7.12.1). For original int inputs `[4,7,4]`
and `[4,7,7]`, Icarus returns queues `[4,4]` and `[7,7]`, while kirimasim
diagnoses the methods as unsupported. These observations are not included
in the inventory's 22-gap count; their full locator/value rules need checking
before implementation. **Subsequent standard check:** 7.12.1 requires one
result even for tied extreme keys. The two-element observations above are
reference mismatches, not correct expected output. Unique-key inputs
separately establish the missing execution support. See the newer audit above.
The finite inventory remains incomplete.

## Documentation and publication recheck (2026-10-24)

The documentation recheck at `b78437a` keeps the implemented container range
and its remaining forms separate. The array-method proposal remains awaiting
approval; reductions and ordering described by that proposal are not enabled.
The historical external population counts and performance measurements retain
their recorded revisions and dates.

Full publication CI on the working tree based on `b78437a` passes all four
configure/build/test presets, release flags, banned features, SPDX, suite
files, global state, expectation protection for all eight previously
unpublished commits, README counts, clang-format and clang-tidy.
Dev/ASan/UBSan each run 2,672 tests (2,670 pass, two known skips); release
runs 2,673 (2,671 pass, the same skips). The skips remain
`suite/cb/input_sample_1step` and `suite/nets/net_port_drivers_part`.
The changed documents' 231 local link targets exist, and the identical
English/Japanese reproducer below was rerun with the documented results.

The expanded comparison was rerun at `b78437a`: the same 59 probes still have
21 Icarus-passing gaps, and `--require-parity` returns 1. Compiler and runtime
both report `14.0 (devel) (s20260301-411-gca6b8d376)`. The local artifact is
`build/docs-publish-icarus-probes-20261024.json`; it is not a full coverage proof.

A separate original probe confirms another required gap: `$fread` into a whole
multidimensional integral memory (21.3.4.4). This probe is **not included** in
the 59-probe inventory or its 21-gap count. Save this source as `probe.sv` in
a scratch directory:

```systemverilog
module top;
  bit [7:0] m[2][2];
  int fd, n;
  initial begin
    fd = $fopen("bytes.bin", "rb");
    n = $fread(m, fd);
    $display("%0d %0d %0d", n, m[0][0], m[0][1]);
  end
endmodule
```

```sh
python3 -c 'from pathlib import Path; Path("bytes.bin").write_bytes(bytes([4, 7]))'
iverilog -g2012 -s top -o probe.vvp probe.sv
vvp probe.vvp
kirimasim probe.sv
```

The installed Icarus compiles and runs successfully, printing `2 4 7`.
Kirimasim at `b78437a` exits 2 with `unsupported: $fread into a memory of this
kind`. The support inventory now explicitly limits fixed integral memories to
one dimension. This is a documentation correction, not an implementation of
multidimensional input or a claim of complete Icarus parity.

## Runtime concatenation and queue rvalue slices (2026-10-24)

Source `919d11f` extends the approved container contract with unpacked
concatenation and reading queue slices (7.10.1, 7.10.4, 10.10). It adds no
SIR operation or VM/kernel value rule. `Lowerer::appendContainerRange` is the
common typed construction loop; unbounded buffers retain every converted
operand until the actual destination applies its bounds. Operands can be
scalars, fixed arrays, returned arrays, dynamic arrays, queues and nested
values. Sources are evaluated once, in source order.

Slices capture the source value before evaluating bounds once left to right;
`$` uses that captured length. Normalization keeps a sign bit for arbitrary
unsigned bounds and clips negative lows and excess highs. X/Z, reversed and
disjoint ranges append nothing, including empty sources. Result elements
are independent copies; existing refs survive queue assignment as detached
cells under the approved lifetime rules. Slice assignment targets remain
unsupported.

The original sequence matrix adds 256 cases: eight execution contexts,
six widths (1/4/32/64/65/129), two/four states and two destination kinds,
plus 64 mechanism cases. Cases cover returned arrays, nested actual bounds,
real/string values, held refs, source calls and in-place source changes from
bound expressions. A separate signed/X/Z widening probe from 32 to 129 bits
also passed. Existing suite bodies, expectations and performance counts are
unchanged. The generator itself is registered with CTest.

All nine deliberate sequence mistakes built and were caught by actual source
output differences: extra elements, sign loss on each bound, unknown-to-zero
conversion, excluding the last element, negative clipping, borrowed source
values, repeated source evaluation and premature buffer bounds. The restored
sequence/CLI run passes 257 tests. These focused checks exclude anchor-only
and judging-tool self-checks, which cannot prove value semantics.

Full local CI at `919d11f` passes all four configure/build/test presets and
all static checks: release flags, banned features, SPDX, suite files, global
state, expectation protection for the seven unpublished commits, README
counts, clang-format and clang-tidy. Dev/ASan/UBSan each run 2,672 tests
(2,670 pass, two known skips); release runs 2,673 (2,671 pass, the same skips).
The documented concatenation/slice example runs with the recorded output
and no warnings. Core and sequence container matrices now total 496 source
cases, alongside the existing 11 CLI diagnostics.

The expanded comparison was rerun with a binary reporting `919d11f`: 59
probes and 21 Icarus-passing gaps remain. Runtime concatenation and queue
slices pass kirimasim; those original inputs are rejected by the installed
Icarus compiler. Compiler/runtime versions remain `14.0 (devel)
(s20260301-411-gca6b8d376)`. `--require-parity` returns 1 for the remaining
gaps. Exact-13.0 and complete parity remain unconfirmed. Historical external
population and benchmark measurements retain their original dates/revisions.

```sh
ctest --preset dev -R 'container_sequence_' --output-on-failure
python3 tools/mutants.py --only 'container sequence:'
CTEST_PARALLEL_LEVEL=6 python3 tools/local_ci.py
python3 tools/icarus_feature_probe.py \
  --kirimasim build/dev/src/driver/kirimasim --extended \
  --output build/container-sequence-icarus-probes.json --require-parity
```

The [array-method amendment](array-methods-proposal.md), recorded in `583c3d0`,
awaits approval before enabling its dependent reductions/ordering. It covers
empty results, key ordering, reference identity and a new shared value-swap
operation. The independently approved sequence work above did not enable
those proposal choices. Classes, VPI, switches, specify/timing and the other
P1 gaps remain work; the full goal is still active.

## SystemVerilog dynamic-array and queue execution (2026-10-24)

Implementation `a0cdade` connects front lowering to the existing shared heap;
`5b5f440` and `50911e1` complete static checks and hierarchical diagnostics.
The [support inventory](../P1_SUPPORT.md#dynamic-container-foundation) and
[executable example](../P1_FEATURES.md#dynamic-arrays-and-queues) describe the
implemented range. This is partial container support, not completion of P1.

The common lowering path now handles construction, initialization, deep
assignment copies, element and packed writes, queue methods, actual-bound refs,
value arguments/returns, nested values and container members of fixed arrays
and unpacked structures. Whole-value delayed assignments keep snapshots.
Dynamic foreach saves its bounds in frames across waits, and first-dimension
queries use the shared size operation (7.5, 7.6, 7.10, 12.7.3, 13.5, 20.7;
SIR_SPEC 5.8). Queue pattern temporaries retain all elements until the actual
variable applies its bounds, including nested equivalent ref types. Wide
indices keep their full integral width. Element NBAs remain illegal.

The source matrix adds 240 original cases: 160 width/state/kind/context
combinations and 80 mechanism cases. Eight contexts cover module initials,
automatic and static tasks, forks, interfaces, packages, programs and generate.
Additional cases cover argument isolation, moved/detached refs, nested bounds,
evaluation order, real/shortreal/string/null-chandle values, explicit heap waits,
delayed copies, foreach/queries and fixed/structure members. Eleven CLI cases
check negative/X/Z sizes and positioned diagnostics for implicit heap sensitivity
and the existing enclosing-automatic fork restriction. Existing suite bodies,
expected output and performance counts are unchanged.

Source tests found that forwarding an empty container ref into a called guard
lost its variable-binding dependency. The VM's shared ref transport now records
that binding, while the kernel still decides heap changes and scheduling.
Direct/called waits wake after the empty variable is replaced. Hierarchical
container reads, including through called functions, are explicitly rejected in
implicit sensitivity until its static trigger encoding can represent heap reads.

Mutation validation also exposed a judging-tool defect: replacing an anchor
makes `tools/mutants_apply` fail even if simulated values are unchanged.
`40a8f87` keeps anchor/judging self-checks on the clean baseline and excludes
those two self-checks during mutated runs. A regression reproduces the old
false catch, checks a surviving mutation and a genuine failure, and checks
source restoration. Focused compiled runs separately caught all nine deliberate
container mistakes through actual source-output differences or CLI assertions;
all nine built, and the restored 241-case source/CLI baseline passed. Self-check
failures are not used as evidence of correct simulator semantics.

Full local CI at `50911e1` passes dev/release/ASan/UBSan, release flags,
banned features, SPDX, suite files, global state, expectation protection for
all four unpublished commits, README counts, clang-format and clang-tidy.
Dev/ASan/UBSan run 2,415 tests (2,413 pass, two known skips); release runs
2,416 (2,414 pass, the same skips). The corrected runner's judging-tool
regression passes as part of these runs. The documentation example also runs
with the recorded output and no warnings.

The original 44 comparison probes now have 19 Icarus-passing gaps, down from
23. Fifteen original container probes were added, including remaining forms
rather than only successful ones. The expanded inventory has 59 probes and
21 Icarus-passing gaps; the two newly measured gaps are dynamic-array `sum()`
and queue `sort()`. Both Icarus compiler and runtime report
`14.0 (devel) (s20260301-411-gca6b8d376)`; the simulator binary reports
`50911e1`. `--require-parity` returns 1 as expected. Exact-13.0 and complete
parity remain unconfirmed. On the three-element dynamic-array query probe,
Icarus returns 2 for `$increment`, while kirimasim retains -1 under the locally
checked rule of 20.7. No expectation was weakened to match that divergence.

```sh
CTEST_PARALLEL_LEVEL=6 python3 tools/local_ci.py
ctest --preset dev -R '^(suite/containers/|cli/dynamic_containers$)' --output-on-failure
python3 tools/mutants.py --only 'container front:'
python3 tools/icarus_feature_probe.py \
  --kirimasim build/dev/src/driver/kirimasim --extended \
  --output build/front-container-icarus-expanded.json --require-parity
```

External sv-tests/population counts and performance measurements retain their
recorded revisions and dates; they were not remeasured in this round.

At that revision, remaining work included runtime array concatenation and
slices, now implemented as recorded above. Remaining work includes container
conditional values, additional array methods, mixed/omitted dynamic foreach
dimensions, nested/runtime dimension queries, implicit heap sensitivity,
fixed-aggregate/class/event elements and the other P1 mechanisms. Class/VPI,
switches, specify/timing and complete Icarus coverage remain unfinished.

## Documentation refresh and publication checks (2026-10-24)

The publication check reruns the committed implementation at `30c5dbc`.
README diagrams and glossary entries, and older Unreleased changelog entries,
now distinguish the implemented typed SIR/VM execution, automatic collection
and heap-dependent waits from unfinished SystemVerilog container lowering.
The uncommitted front-lowering work is excluded from this validation and
publication; it is not evidence of supported language execution.

The four local CI test presets pass: dev/ASan/UBSan each run 2,173 tests
(2,171 pass, two known skips), and release runs 2,174 (2,172 pass, the same
skips). Release flags, banned features, SPDX, suite files, global state,
expectation protection for the five previously unpublished commits, README
counts, clang-format and clang-tidy all pass. All 423 relative documentation
links resolve. Only documentation was modified during this rerun.
The extended original comparison at `30c5dbc` still has 44 probes
and 23 Icarus-passing gaps. Both Icarus compiler and runtime report
`14.0 (devel) (s20260301-411-gca6b8d376)`; `--require-parity` returns 1 as
expected for those gaps. Exact-13.0 coverage and complete parity remain
unconfirmed. Historical external-population counts and benchmark results
retain their original measurement dates and revisions.

```sh
CTEST_PARALLEL_LEVEL=6 python3 tools/local_ci.py
python3 tools/icarus_feature_probe.py \
  --kirimasim build/dev/src/driver/kirimasim --extended \
  --output build/docs-push-icarus-probes-20261024.json --require-parity
```

## Kernel/VM heap-dependent monitors (2026-10-24)

Source revision `71966e1` captures heap reads during guard/watch evaluation and
reattaches dependencies after selection or binding changes. Weak watch counts
on containers/cells skip unrelated writes. Matching monitor IDs and serials
are queued in the existing ordered change stream after commit. The tier
publishes each heap mutation before its next instruction; callbacks defer
notifications until the outermost walk ends. A stale notice cannot evaluate a
reused monitor record. Cancellation and interpreter exit restore hook ownership.

The VM uses the same path for size and stable-cell reads, including ref reads
inside called functions. As for static slot dependencies, private containers
of called functions are excluded; global and direct hierarchical reads remain.
The iterative, deduplicated `Heap::reaches` query examines current private
container graphs without read notifications, GC marking or ownership changes.
Capture/notice allocation failure returns ResourceLimit, with the responsible
VM instruction's source position, and never accepts an incomplete read set.

New tests comprise one heap query test, five scheduler tests and two VM tests.
The scheduler checks selected/unselected cells, normalized no-ops, B-bit changes,
structure/index/binding reattachment, size-only reads, detached cells, both
process groups, 1,000 wait registrations with bounded lists, cancellation/reuse,
hook ownership and capture/notice failure. VM matrices cover eight direct/called
size/cell waits, 16 private/global/hierarchical/call-forwarding combinations and
four resource/cleanup cases. The previous unsupported-watch unit now checks an
actual wakeup/value/time; existing suite bodies and output expectations remain.

All eight deliberate monitor mutations compiled and were caught by actual
assertions (exact dependency matching, watch markers, reattachment, serials,
hook ownership, private-variable exclusion, callee ref reads and incomplete
capture). Restored heap/scheduler/VM tests pass 106 cases. Dev passes 2,171 of
2,173 tests, with the same two known skips. There are 31 heap tests and 24
container VM tests.

Full local CI at `71966e1` passes all four configure/build/test presets:
dev/ASan/UBSan each run 2,173 tests (2,171 pass, two known skips); release runs
2,174 (2,172 pass, the same skips). Release flags, banned features, SPDX, suite
files, global state, expectation protection for all four unpublished commits,
README counts, clang-format and clang-tidy pass. The expectation guard flags
the changed unsupported-watch unit for review: it now proves implemented
wakeup/value/time instead of expecting the old unsupported diagnostic. Local
documentation targets were checked: 269 exist, none missing.

The original extended inventory was rerun at this revision: 44 probes and
23 Icarus-passing gaps remain. Both compiler and runtime report
`14.0 (devel) (s20260301-411-gca6b8d376)`. Exact-13.0 and complete parity remain
unconfirmed. `--require-parity` returns 1 because the measured gaps remain.

```sh
ctest --preset dev -R '^(Heap|Scheduler|VmContainer)' --output-on-failure
python3 tools/mutants.py --only 'heap monitor:'
python3 tools/icarus_feature_probe.py \
  --kirimasim build/dev/src/driver/kirimasim --extended \
  --output build/heap-monitor-icarus-probes.json --require-parity
```

External sv-tests/population and benchmark figures retain their recorded
revisions and dates; they were not remeasured in this implementation round.

Front lowering and source-language execution matrices still remain. No
SystemVerilog dynamic-container feature is enabled by this extension. Classes,
VPI, switches, specify/timing and the other full-P1 comparison gaps remain work;
the finite inventory cannot prove Icarus 13.0-or-broader coverage.

## Typed heap observation foundation (2026-10-24)

Source revision `2c674a6` exposes heap read/change hooks for the dynamic
monitor mechanism of SCHEDULER_SPEC 9.5. Reads distinguish container element
sequence/size from individual cell values, including detached cells. Copying,
snapshots and construction also observe the source cells they read. Unknown
or out-of-range element selection still records the container structure, so
future growth can invalidate that observation; empty/stale tokens do not
fabricate a cell identity. Observations are weak identities, not GC roots.

Committed writes emit a cell notice only after normalization changes its
stored words; nested owned transport emits one when its handle changes.
Insertion, push, pop and erase emit container-structure notices after commit.
Failed operations, invalid indices, empty pops, dropped insertions and equal
normalized writes emit no changes. Clearing/replacing a variable does not
mutate the old object; slot/frame binding changes remain a separate dependency.
Hooks are Context-local and exchange their previous owner. Callbacks must not
throw, mutate or collect the heap; an observer owns its failure bookkeeping.

Five new kernel tests cover read identities, 40 width/state/kind/entry write
combinations, three queue bounds, nested transport, allocation failures on
three mutation paths, hook ownership and isolation. Callback reads verify that
changed values have already committed. All eight deliberate observation
mutations were caught by actual assertions after compiling; the restored
baseline passes. The targeted heap/SIR/VM/negative-source run passes 68 tests.
The generator preserves all existing suite files and expectations.

Full local CI at `2c674a6` passes configure/build/test for dev, release, ASan
and UBSan. Dev/ASan/UBSan each run 2,165 tests: 2,163 pass and the same two
known skips remain. Release runs 2,166: 2,164 pass, two skip. Release flags,
banned features, SPDX, suite files, global state, expectation protection for
both unpublished source commits, README counts, clang-format and clang-tidy
all pass. Changed unit assertions are flagged for review; the cell-NBA test
keeps the unchanged-value check and strengthens its exact prohibition/position
check. There are 30 heap tests and 22 container VM tests. Local documentation
targets were checked: 263 exist, none missing.

The original extended inventory was rerun at `2c674a6`: 44 probes and
23 Icarus-passing gaps remain. Both compiler and runtime report
`14.0 (devel) (s20260301-411-gca6b8d376)`. Exact-13.0 and complete parity remain
unconfirmed. `--require-parity` returns 1 because gaps remain. Reproduce:

```sh
python3 tools/icarus_feature_probe.py \
  --kirimasim build/dev/src/driver/kirimasim --extended \
  --output build/heap-observation-icarus-probes.json --require-parity
```

External sv-tests/population and performance figures retain their dated
revisions; this run does not remeasure them.

This is the observation API, not completed monitor integration. Scheduler
subscription/reattachment and VM guard capture still need connection; front
continues to reject SystemVerilog dynamic containers. Full Icarus parity
remains unproven, and the full goal stays active.

## Correction: element NBAs are illegal (2026-10-24)

Earlier integration lists below incorrectly treated container-cell NBA targets
as a missing P1 feature. IEEE 1800-2023 6.21 and 10.4.2, and the existing
SIR_SPEC 3.3, prohibit nonblocking writes to elements of dynamically sized
arrays, including queues. No specification amendment or extension is adopted.
Whole-value NBAs to eligible static container variables remain distinct.

The pinned slang rejects original dynamic-array and queue inputs at parse
with exit code 2 and the assignment's position. Six new generated negative
cases cover both array kinds and whole-element, bit and part targets. The
VM's two original SIR cases now name the prohibition and check its source
position while preserving the unchanged-value assertions. Element NBA execution
remains rejected. The earlier statements calling it unfinished work are
superseded by this correction; heap-dependent waits remain genuine work.

Compiler-only comparison of those same six original negative cases with
Icarus `14.0 (devel) (s20260301-411-gca6b8d376)` gives exit 0 for the two whole
element targets, and compiler abort/exit 134 for the four bit/part targets.
Runtime behavior of the accepted negative cases is unconfirmed. These are
acceptance/diagnostic differences, not evidence that an illegal construct
must become a kirimasim execution feature; strict slang diagnostics remain.
Recheck the legal-side rejection with:

```sh
ctest --preset dev -R '^suite/illegal/nba_(dynamic_array|queue)_' --output-on-failure
```

## Automatic typed-root collection (2026-10-24)

Source revision `3281727` adds automatic collection for the current container
SIR at drained region/slot boundaries. The VM publishes typed slots and fixed
arrays, live process/task activations, retained frame/cell references and
deferred call arguments. The scheduler exposes owned NBA snapshots, pending
drivers and held values without depending on SIR. The VM filters these by slot
type; ordinary numeric/real bits are not conservative roots. Stopped kernel
activations are omitted unless a live reference reaches them.

Eight new VM tests cover 24 repeated replacement/churn combinations, eight
fixed-array cases, 12 active/reactive NBA cases, 16 deferred-call cases and six
opaque-value cases, plus dropping the last reference without a heap mutation,
restoring callback ownership and stopping a suspended activation. Each churn
case runs 100 rounds and checks live counts and pool tables of at most eight
entries. Detached cell writes remain visible while the reference lives.
Deferred callbacks use the common `bindArgs` path for static and automatic
parameters. One new heap test checks mutation tracking and failed marking;
two scheduler tests check safe boundaries and owned pending payloads.

Allocation tests now distinguish failed assignment from failed collection
after a committed assignment. The former preserves the exact original token;
the latter preserves both complete values until a successful retry. Exact
counts exclude unreachable temporaries reclaimed by the new safe points.
Existing suite bodies, expected output and performance counts are unchanged.

The dev run has 2,154 tests: 2,152 pass and the two known skips remain.
There are now 25 heap tests and 22 container VM tests. The targeted 49-test
heap/VM/scheduler run passes.

Full local CI at `71ee082` passes all four configure/build/test presets:
dev/ASan/UBSan each have 2,154 tests (2,152 pass, two known skips), and
release has 2,155 (2,153 pass, the same skips). Release flags, banned features,
SPDX, suite files, global state, expectation protection for all six unpublished
commits, README counts, clang-format and clang-tidy all pass. Expectation
protection flags the changed unit assertions for review; the exact identity
and count checks above explain the stricter reachable-value accounting.
Local targets in the updated documentation were checked: 263 exist, none missing.

The extended original probe inventory was rerun at `71ee082`: 44 probes,
23 Icarus-passing gaps. Compiler and runtime both report
`14.0 (devel) (s20260301-411-gca6b8d376)`. Exact-13.0 and complete parity remain
unconfirmed. `--require-parity` returns 1 because those gaps remain. Reproduce:

```sh
python3 tools/icarus_feature_probe.py \
  --kirimasim build/dev/src/driver/kirimasim --extended \
  --output build/heap-roots-icarus-probes.json --require-parity
```

External sv-tests/population counts and performance measurements retain their
recorded revisions and dates; they were not remeasured in this documentation refresh.

No collection occurs inside process/guard callbacks, after termination is
requested or during final cleanup. The existing final termination policy is
preserved (SCHEDULER_SPEC 16.2). Final allocations are released with Context.
Class/DPI/VPI root providers, front lowering, heap-dependent change
notification, cell NBA targets and language execution matrices remain work.
SystemVerilog dynamic arrays and queues are still rejected, and the full
Icarus-or-broader goal is not achieved.

```sh
ctest --preset dev -R '^(VmContainer|Heap|Scheduler[.](SafePoints|PendingValue))' --output-on-failure
```

The sections below record earlier stages and their then-current limitations.

## VM container instructions and actual references (2026-10-24)

Source revision `c762840` mechanically dispatches the 16 typed SIR instructions to the
shared heap. Layout/type registration occurs once and kernel errors return
through the existing source-position paths. Variable references carry their
actual type even when empty or viewed through a different equivalent ref
formal. Element references carry stable cell tokens, actual layouts and their
origin for Postponed checks. Advancing a forwarded fixed-array frame ref
preserves its type metadata.

Fourteen new independent SIR tests cover 216 element/bound/storage combinations,
72 ref-call combinations, four forwarded-array cases, 36 selected-cell cases,
96 packed writes against `valsem`, 12 receiver/argument-side-effect cases,
32 invalid-index cases and four waiting-task cases. They also check source
positions, once-per-position warnings, invalid sizes, overflow, deterministic
allocation failures and eight Postponed combinations. A new kernel test
checks copied descriptors, stale handles and already-owned nested transport.

The positive instruction matrix fails with the previous VM implementation,
and passes after restoring this extension. All eight deliberate mutations
(formal receiver bounds, frame-ref metadata, snapshot conversion, wrong cell
index, repeated warnings, four-state defaults, Postponed origin and size-error
classification) were caught by actual VM assertions; the restored baseline
passes. Existing suite bodies and expectations were kept.

Dev has 2,143 tests: 2,141 pass and the two known skips remain. Targeted
clang-tidy, banned-feature, formatting and mutation-anchor checks pass.

Full local CI on `c762840` passes dev/release/ASan/UBSan configure/build/tests,
release flags, banned features, SPDX, suite files, global state, expectation
protection for all three unpublished commits, README counts, clang-format and
clang-tidy. The extended original comparison was rerun at the same revision:
44 probes and 23 Icarus-passing gaps remain. Both installed Icarus compiler
and runtime report `14.0 (devel) (s20260301-411-gca6b8d376)`; exact-13.0 and
full parity remain unconfirmed.

This does not enable SystemVerilog dynamic containers. Front lowering,
complete safe-point roots and collection, heap-dependent change notification,
cell NBA targets and source-level execution/diagnostic matrices remain. The
adapter does not collect automatically yet, so a detached cell surviving a
wait is not evidence of root completeness. Deferred writes through cell refs
and heap-dependent watched expressions are explicitly rejected in this
intermediate implementation. Four guard/watch cases check the diagnostic
before a cell token can be mistaken for a slot reference; two cell-NBA cases
check that rejection leaves the value unchanged. The last measured
23 Icarus-passing gaps and the full P1 objective remain open.

```sh
ctest --preset dev -R '^(Heap|ContainerSir|VmContainer)[.]' --output-on-failure
python3 tools/mutants.py --only 'container VM:'
python3 tools/icarus_feature_probe.py \
  --kirimasim build/dev/src/driver/kirimasim --extended \
  --output build/container-vm-icarus-probes.json --require-parity
```

The final command returns 1 because the measured gaps remain.

## Nested-bound conversion and actual-type snapshots (2026-10-23)

Source revision `480037e` fixes the original nested queue copy failure recorded
below. `Heap::copy` and `construct` accept equivalent element shapes rather
than requiring identical registration IDs. Registration computes shape
equivalence once, ignoring nested queue capacities but preserving kinds,
word masks, state domains, defaults and field positions. Validation performs
no allocation or native recursion; language type checking remains in the tier.

The common iterative `Heap::clone` applies every destination queue bound and
records any truncation in the transaction. Construction, whole copies,
element writes, append, push and insert return that warning only after a
successful commit. `Heap::snapshot` copies using the actual source type and
bounds, preserving independent cells before later side effects.

Eight new kernel tests cover 432 copy combinations, nine value-entry paths,
branching aggregates, 2,500 nested levels, snapshot capacities, descriptor
registration failures and each allocation stage of ten operation paths.
They verify values, source independence, retained cell identities, detached
roots, warnings and rollback/reuse. The positive matrix fails on the previous
implementation and passes with the extension. The original C++20 probe now
returns success instead of InvalidHandle. Six deliberately wrong conversions
were caught by actual heap assertions; the restored baseline passes.

Dev passes 2,128 tests: 2,126 pass and the same two known skips remain.
All 23 heap tests pass. The independent scalar-container model was rerun at
scale 100, seed 20261023, and passed 2,520,000 operations. No existing suite
body or expected output was changed.

Full local CI on `480037e` passes dev/release/ASan/UBSan configure/build/tests,
release flags, banned features, SPDX, suite files, global state, expectation
protection, README counts, clang-format and clang-tidy. The extended original
comparison was rerun at the same revision: 44 probes and 23 Icarus-passing
gaps remain, using the installed compiler/runtime
`14.0 (devel) (s20260301-411-gca6b8d376)`.

```sh
ctest --preset dev -R '^Heap[.]' --output-on-failure
KIRIMA_FUZZ_SCALE=100 KIRIMA_FUZZ_SEED=20261023 \
  build/dev/tests/unit/kirimasim_unit_tests \
  --gtest_filter=Heap.IntegralOperationsAndStableReferencesMatchIndependentModel
python3 tools/mutants.py --only 'heap conversion:'
python3 tools/icarus_feature_probe.py \
  --kirimasim build/dev/src/driver/kirimasim --extended \
  --output build/nested-conversion-icarus-probes.json --require-parity
```

The final command returns 1 because the comparison gaps remain. Mutation
evidence above came from actual heap assertions, rather than an anchor check.

Front/VM execution, actual variable metadata for ref bindings, complete roots,
source-level diagnostics and notifications remain unfinished. This change
does not enable SystemVerilog dynamic containers or close the last measured
23 Icarus-passing gaps. The full P1 goal remains unachieved.

## Published documentation and comparison check before this extension (2026-10-23)

The typed container SIR implementation is `a8f63b5`. Source revision
`f00c807` also makes descriptor initialization explicitly nonthrowing, fixing
the clang-tidy rejection of scalar types with static lifetime. Compile-time
checks cover scalar initialization, absent descriptors and numeric IDs;
all 72 Sir/ContainerSir tests pass.

The full local CI run passes dev/release/ASan/UBSan configure/build/tests,
release flags, banned features, SPDX, suite files, global state, expectation
protection, README counts, clang-format and clang-tidy. Dev has 2,120 tests:
2,118 pass, with the same two known skips. Existing suite expectations are
unchanged; the initialization fix adds compile-time checks only.

The extended comparison was rerun with `f00c807`: 44 original probes still
leave 23 Icarus-passing gaps. Both installed Icarus compiler and runtime
report `14.0 (devel) (s20260301-411-gca6b8d376)`. No SystemVerilog container
execution or full parity is claimed. Reproduce this latest measurement with:

```sh
python3 tools/icarus_feature_probe.py \
  --kirimasim build/dev/src/driver/kirimasim --extended \
  --output build/docs-update-icarus-probes-latest.json --require-parity
```

The expected exit code remains 1 because the measured gaps are open.
External sv-tests/population counts and performance measurements retain
their original revisions and measurement dates.

## Typed container SIR implementation (2026-10-23)

The heap foundation now has typed container descriptors/references, 16 SIR
operations, verification and text support (SIR_SPEC 5.8). Captured receiver
references separate binding from argument values. `container.snapshot`
captures the actual source value, `container.copy` creates a value of a
specified type, and `container.assign` applies the actual target variable's
bounds. Raw handle transport does not implement language value copying.

Checking 6.22.2 and 13.5.2 established that queue capacities do not distinguish
equivalent ref targets, including nested queues. An original positive check
failed with the initial strict-descriptor verifier; iterative equivalence
now accepts it. Execution must preserve the actual target type/capacity,
including empty variables, rather than replacing it with the formal's bound.

Ten new SIR tests cover 96 type/bound combinations, 49 malformed cases,
input/ref/const-ref arguments, individual/fixed-array container references
and 2,500 nested equivalence levels. Negative cases require the corresponding
diagnostic, so an unrelated rejection cannot hide a missed check.

All nine deliberate mutations (receiver descriptor/kind, size type, copied
element type, descriptor cycle, nonempty constant, ref-bound equivalence,
snapshot type and assignment element compatibility) were caught by actual
SIR tests, excluding mutation-tool anchor checks. The restored baseline passes.

VM execution, front-end production, roots, diagnostics and change notification
are not connected yet. Kernel copying across differing nested queue bounds
and runtime references retaining actual target types still need integration
and tests before the language producer is enabled. The 44 probes and 23
Icarus-passing gaps remain open; the full P1 target is unfinished.

At the SIR stage, an original kernel input with the same integral element type
and inner queue capacity changed from 1 to 3 confirmed that `Heap::copy`
returned InvalidHandle: it required identical element-layout IDs. This is the
mechanism addressed by the nested-bound extension above. At that
stage, dev passed 2,120 tests: 2,118 pass and the same two known skips
remain. Existing suite bodies and expected outputs were preserved.

## Published baseline before this extension (2026-10-23)

The published baseline `6a1830a` includes the five delayed-NBA/packed-target/
named-event rounds and the approved dynamic-container heap foundation.
Its push-time local CI passed dev/release/ASan/UBSan, static checks and all
14 then-unpublished commit expectation checks. Dev has 2,110 tests:
2,108 pass and the two known skips remain. The changes were pushed to main.

The heap has 15 kernel tests, a passing 2,520,000-operation model comparison
and six caught mutations. SIR/front/VM integration, complete safe-point
roots and SystemVerilog dynamic-container execution are unfinished. The
44-probe comparison still has 23 Icarus-passing gaps; exact-13.0 coverage
and complete parity remain unconfirmed. See [P1_SUPPORT](../P1_SUPPORT.md)
for the current inventory and [architecture](../ARCHITECTURE.md) for the
kernel/language boundary.

The extended inventory was rerun with the `6a1830a` binary during this
documentation update: 44 probes and the same 23 gaps. Both installed Icarus
compiler and runtime report `14.0 (devel) (s20260301-411-gca6b8d376)`.
Reproduce the comparison with:

```sh
python3 tools/icarus_feature_probe.py \
  --kirimasim build/dev/src/driver/kirimasim --extended \
  --output build/docs-update-icarus-probes.json --require-parity
```

`--require-parity` currently returns 1 because these gaps remain. This
finite inventory does not establish full language or interface parity.

## Implementation history

The sections below record each round's measurements and decisions. Their
counts and approval status describe that round, rather than the latest
snapshot above.

Two implementation-and-test rounds have added:

- Unformatted `%u` and `%z` output through the common kernel formatter,
  with arbitrary widths, zero padding, X/Z handling and native word byte
  order. A generated CLI matrix checks 132 programs / 14,256 conversions,
  including file, postponed, packed and string destinations. Three new
  unit tests check raw words, dirty padding and runtime formats. String
  destinations omit NUL; packed and file destinations preserve it.
- Ref-memory readmem/writemem operations, including const-ref writing,
  forwarding, waiting calls, automatic storage and selected rows. The
  63 generated suite cases cover seven shapes in nine homes, both radixes,
  ranges, multidimensional and enum memories, and preservation of adjacent
  rows. Selecting a subarray now completes its remaining coordinates in
  `Lowerer::refOf`, avoiding the invalid partial `ref.make` found by this
  matrix. Existing expectations were preserved.

The original binary and ref-memory tests fail with the pre-change binary
`499198369a07`, respectively on unsupported `%u` and unsupported ref
`$readmemh`; the new implementation passes them. The runnable examples in
both language versions of P1_FEATURES were executed successfully.

Local CI passed on the expanded working tree: dev, release, ASan and
UBSan configure/build/tests, release flags, banned features, SPDX, suite
files, global state, README counts, clang-format and clang-tidy. Dev has
1,572 tests: 1,570 pass and the two previously allowed skips remain.
Existing test changes only add assertions and CMake registrations.
New Python SPDX headers and new/changed document links were also checked.
Publication checks additionally validate each unpublished commit for
changes to existing expectations.

## Reproducing the comparison

The installed comparison pair reports Icarus compiler/runtime
`14.0 (devel) (s20260301-411-gca6b8d376)`. The exact 13.0 release has not
been measured in this round. Only original test inputs and executable
behavior were used; no Icarus source or tests were read or imported.

```sh
python3 tools/icarus_feature_probe.py \
  --kirimasim build/dev/src/driver/kirimasim \
  --output build/p1-icarus-probes.json --require-parity
python3 tests/cli/run_binary_format_tests.py \
  --kirimasim build/dev/src/driver/kirimasim \
  --iverilog iverilog --vvp vvp --container initial \
  --api write --api display --api fwrite --api fdisplay
```

The first command intentionally returns 1 on the current working tree:
seven of the sixteen original probes pass Icarus and fail kirimasim. Six
are standard mechanisms: dynamic arrays, queues, class objects,
MOS switches, bidirectional switches, specify paths. The seventh, `wire real`, is an
[Icarus extension](https://steveicarus.github.io/iverilog/usage/icarus_verilog_extensions.html);
strict slang correctly rejects it. Supporting it needs an explicit
compatibility policy, not relaxation of the default language checks.

The second command passes four programs / 432 byte-exact conversions in
Icarus. The larger kirimasim-only matrix remains the source of evidence
for the additional containers and string/packed destinations. Icarus
warned and truncated a binary string result at an embedded NUL; kirimasim
follows its approved string-value rule instead. This difference is not
hidden by the comparison tool.

The probes are a seed inventory. VPI, FST, SDF, timing checks, other
Icarus extensions and interactions still need their own original probes.
Both-failing associative-array and Icarus-failing ref-memory probes say
nothing about the full support of those mechanisms. Extend the inventory
as work continues; do not turn a passing seed set into a full parity claim.

The official [13.0 release notes](https://steveicarus.github.io/iverilog/releases/v13-0-release-note.html)
give release context; they are not a complete coverage specification.

## Binary-input preparation

The next continuation checked IEEE 1800-2023 21.3.4.3 and Table 21-7
against the current specifications. Their general whitespace rule and the
choices needed for transfer widths and partial fields need an explicit
[binary-scan amendment](binary-scan-proposal.md), approved on 2026-10-23
together with the phase-scope amendment. The semantic rules are now in
SYSTEM_TASKS 6.3 and SIR_SPEC 8. Binary-input implementation is recorded in the next section;
existing expectations are unchanged.

An original [observation tool](../../tools/binary_scan_probe.py) records
448 cases and distinguishes failed runs from observed behavior. A
nonzero/omitted-width subset completed with 374 observations matching the
approved project rules. Width-zero behavior differs, and grouped runs also
exposed native crashes in two installed Icarus development builds; see
the amendment for the recorded limits. These results guide review rather
than claiming conformance or full parity.

## Binary-input implementation

The third round implements the approved binary scan rules in shared kernel
`readBinary`, with destination widths from SIR. Constant/runtime formats,
file/packed/string inputs and all ten containers use the same semantics.
The CLI matrix checks 40 programs / 17,120 conversions, two continuation
programs and 40 rejected formats/types. Four added unit tests cover raw
bytes, every byte of short fields, widths, masks, suppression and type rules.
Existing suite bodies and expected output remain unchanged.

```sh
python3 tools/binary_scan_probe.py \
  --kirimasim build/dev/src/driver/kirimasim --engine kirimasim \
  --require-kirimasim --output build/binary-scan-all-implemented.json
```

All 448 project-rule observations match. The nonzero-width reference subset
also matches in both tools (374/374 each). The initial feature probe now has
eight Icarus-passing gaps: seven standard mechanisms (dynamic arrays,
queues, classes, programs, MOS, bidirectional switches and specify paths)
and the `wire real` extension. Full parity is still unconfirmed. The next
[program execution amendment](program-execution-proposal.md) addresses a
conflicting `$exit` description and missing execution metadata.

## Expanded mechanism inventory

`tools/icarus_feature_probe.py --extended` retains the original 16-case
seed and adds 22 original probes (38 total). `--only <name>` selects an
individual probe. The measured development executables still identify as
Icarus `14.0 (devel) (s20260301-411-gca6b8d376)`, not exact 13.0.

```sh
python3 tools/icarus_feature_probe.py \
  --kirimasim build/dev/src/driver/kirimasim --extended \
  --output build/p1-icarus-extended.json --require-parity
```

25 probes pass Icarus and fail kirimasim after program execution was added. They include subcases of the
existing gaps plus event-controlled NBAs, delayed NBAs with runtime array
indices, nested packed selection targets and strengths across a bit-select
port connection. This count is of probes, not distinct language features;
adding probes can increase it without regressing the implementation.

Net declaration assignments with delay pass both. The separate bare-net
delay probe is rejected by both; array-slice assignment is also rejected
by both in this measurement. The class virtual-method probe runs to code 0
in Icarus but prints the base result 3, instead of the derived result 9
specified by this original test (8.20). The report distinguishes normal-exit
result mismatches from native execution failures, and records expected bytes
alongside actual bytes. None of these outcomes establishes complete support
or absence of support for its whole mechanism. No simulator source was read.

Binary-input regression additionally passes dev (1,575 passes, two known
skips). Three deliberate binary scanner mutations (skipping whitespace,
ignoring destination widths, assigning short fields) are each detected by
its four new binary unit tests; the normal sources pass after restoration.

Final binary-input local CI passed all presets (dev, release, ASan, UBSan),
release flags, banned features, SPDX, suite files, global state, unpublished
commit expectation checks, README counts, clang-format and clang-tidy.
The detailed program amendment is now approved and implemented; see the next section.

## Program execution

The program amendment was approved on 2026-10-23 before source changes.
The shared scheduler now owns program initials, dynamic origin, descendant
termination and `$exit`; SIR supplies program instance identities and the
VM supplies root/child registration. Implicit program port assignments use
the program's reactive set. Calls declared elsewhere retain the caller's
set and origin. Natural completion and already confirmed report calls are
kept distinct from newly canceled execution.

The generated matrix has 124 cases: 96 combinations of subroutine scope,
direct/task/fork entry, instance placement and 32/129-bit values, plus 28
lifetime, exit, port, anonymous-program, deferred-call and stress cases.
Seven new scheduler tests and one SIR test cover kernel semantics, context
isolation and record reuse; CLI checks 15 illegal-input combinations and
two program identities. The former rejection fixture keeps its design body
and checks an unordered output multiset in a separate expectation commit.
Existing suite bodies and expected values remain unchanged.

Dev regression passes 1,711 cases: 1,709 pass and two known skips remain.
The 16-case seed inventory has seven Icarus-passing gaps; the extended
38-case inventory has 25. Program VCD scopes remain unimplemented, as do
classes, dynamic containers, clocking and concurrent assertions. This is
progress toward the full P1 target, not a complete-parity claim.

Program local CI passed dev/release/ASan/UBSan configure, build and tests,
release flags, banned features, SPDX, suite files, global state, all six
unpublished commit expectation checks, README counts, clang-format and
clang-tidy. Four deliberate mutations (module-like program identity,
Active initial execution, lost fork origin and unended descendants) each
fail the program-specific tests; the restored implementation passes.

The next [dynamic-container amendment](dynamic-containers-proposal.md)
fills the missing heap/instruction contract, including stable outdated
reference cells and tracing roots. It was later approved on 2026-10-23;
the heap foundation and remaining integration are recorded below.

## Runtime targets of delayed NBAs

The next round implements the existing 10.4.2/9.4.5 and SCHEDULER_SPEC 5/13
rule for fixed-array indices, packed bit/part positions and concatenations.
`Lowerer::storePlace` supplies the same evaluated places used by ordinary
writes; `nba.elem.delay` and `nba.bits.delay` pass captured ranges and
values to the shared kernel's existing NBA scheduling. No new semantic
choice or dependency was needed. Ref/string-character delayed targets and
event-controlled NBAs remain unsupported.

The new matrix has 64 cases: eight containers, 1/32/65/129-bit widths and
two/four-state storage. It checks descending/negative/multidimensional
indices, single evaluation, value and delay capture, partial and X/Z/wide
indices, X/Z data, concatenations, packed members, ordering and NBA/Re-NBA.
CLI checks 42 execution/diagnostic combinations and both SIR spellings;
one new verifier unit checks valid forms and ten negative rows check
malformed operands and forbidden frame targets. The old rejection fixture
keeps its design body and now checks acceptance in a separate expectation
commit, justified by 10.4.2. New tests check the resulting values.

Dev passes 1,778 tests: 1,776 pass and the two known skips remain. The
38 original comparison probes now have 24 Icarus-passing gaps; the original
16-case seed still has seven. The compiler/runtime remain
`14.0 (devel) (s20260301-411-gca6b8d376)`; exact-13.0 and full parity remain
unconfirmed. The runnable example in both P1_FEATURES versions was checked.

## Aggregate delayed NBAs

A further round extends the same scalar delayed-store mechanism to whole
fixed arrays and unpacked structures. `Lowerer::flushStores` delays an
already captured aggregate snapshot. `copyArrayStorage` retains runtime
prefix indices of structure array members and traverses remaining dimensions
from each side's own left bound; ordinary aggregate assignments share this
path. No new value/scheduling choice or dependency was introduced.

Another 64-case matrix crosses the same eight containers, four widths and
two domains. It covers differing array bounds, self-overlap, patterns with
counted side effects, default patterns, real/string/fixed-array members,
dynamic source and destination structure indices, and zero-delay NBA/Re-NBA.
Existing aggregate, structure and SIR tests also pass. Dev has 1,843 tests:
1,841 pass and the two known skips remain. The runnable example was checked.

Two new original probes make the extended inventory 40 cases. Whole-array
and unpacked-structure delayed NBAs pass kirimasim; the installed Icarus
compiler rejects both, so these are not Icarus-passing gaps. There are still
24 such gaps, and seven in the original 16-case seed. Exact-13.0 and full
parity are unconfirmed. Event-controlled NBAs, blocking aggregate timing,
refs/string-character delayed targets and further aggregate forms remain
work. The dynamic-container proposal was subsequently approved; its current
implementation status is summarized at the top of this document.

## Delayed streaming NBAs

A third round reuses `Lowerer::lowerUnpack` and `storePlace` for streaming
NBA targets with a delay. It preserves existing 11.4.14.3/VALUE_SEMANTICS 8
reordering and captures the RHS, delay and selectors once in the established
order. Another 64-case matrix crosses eight containers, four widths and
two domains, with right/bit/byte/65-bit orders, partial final blocks,
leftmost-source truncation, X/Z, runtime selectors, whole arrays and #0.
Dev passes 1,908 tests: 1,906 pass and the two known skips remain.
The bilingual example was executed. No existing expectations changed.

A new original streaming-delay probe brings the extended inventory to 41;
kirimasim passes it and the installed Icarus compiler rejects it. There
remain 24 Icarus-passing gaps and seven in the original seed. These finite
observations do not establish full parity or exact-13.0 support.

## Bounded nested packed targets

The fourth round retains a static parent bit range in `store.bits`,
`nba.bits`, their delay form and `store.ref.bits`. `Lowerer::narrowRuntime`
keeps that range, and compound reads select it before their child. The
kernel's `inside` clips relative positions inside the parent and returns
absolute storage ranges; narrow/wide VM paths only decode these operands.
Frame ranges are verified and represented in SIR text too. Runtime-position
parents and out-of-storage constant parents remain explicit unsupported forms.

The new 112-case matrix crosses 14 storage/execution forms, four widths
and two domains. It checks X/Z and wide indices, partial ranges, compound
reads, single evaluation, adjacent bits/elements, slots, frames, refs,
packed members, ordinary NBA and delayed NBA. A new kernel test compares
20,000 bounded writes with composed `valsem` operations (scaled with the
existing fuzz setting); a new SIR test checks range validation and text.
The historical packed-member rejection fixture keeps its input body and
moves to acceptance in a separate expectation commit.

Dev passes 2,023 tests: 2,021 pass and the two known skips remain. The
bilingual example was executed. A new original boundary probe and the
previous nested-select probe pass both tools: the extended inventory now
has 42 probes and 23 Icarus-passing gaps; the seed remains seven gaps.
The compiler/runtime versions and exact-13.0/full-parity limitations have
not changed.

All three delayed-target mutations, all three aggregate mutations and all
three streaming mutations were detected by their respective new execution
tests, excluding the mutation-tool self-checks. An initially surviving
aggregate-index mutation exposed a missing differing-bound member case;
that case was added before final validation. Restored sources pass.

## Mixed named-event waits and per-item iff

The fifth round registers named events, slot subscriptions and expression
watches in one scheduler wait generation. `Scheduler::waitAny` owns the
registration; `trigger` evaluates only the triggered item's guard, retains
false guards and cancels stale items when any item wakes the process.
`Lowerer::lowerTiming` emits typed named items and guards in `wait.event`;
the VM decodes them. A single unguarded named wait keeps its old form.
C ABI 0.3 appends `wait_mixed`, preserving all previous field offsets and
entry signatures. Both APIs call the same kernel implementation.

The 64-case matrix covers eight containers and eight event shapes,
including same-slot rearming, false/X guards, changing a condition without
an event, named/signal/watch mixtures, NBA triggers and Reactive resumes.
Five new scheduler tests check guard invocation, bounded false-item lists,
first-item cancellation, cancellation before guard access, queued triggers
and registration before watch initialization. A C ABI test checks the new
entry and retained table prefix; a SIR test rejects edges on named items.
The old named-iff rejection fixture keeps its body and migrates to
acceptance in a separate expectation commit. The example was executed.

Dev passes 2,095 tests: 2,093 pass and the two known skips remain. Across
these five rounds, 368 suite cases were added, along with kernel/SIR/CLI
checks and generator checks. The comparison inventory now has 44 original
probes: mixed named-event waiting passes both tools; named-event iff passes
kirimasim and is rejected by the installed Icarus compiler. There are
still 23 Icarus-passing gaps, seven in the original seed. Compiler/runtime
versions remain `14.0 (devel) (s20260301-411-gca6b8d376)`; exact-13.0 and
complete parity are unconfirmed.

The [dynamic-container amendment](dynamic-containers-proposal.md) was
approved on 2026-10-23 after this round. Its implementation status is
recorded below. These additions do not close the full P1 objective.

## Mutation validation of all five rounds

All 15 deliberate mutations, three per mechanism, were detected by the
new execution/reference tests. The packed-target checks catch losing the
parent bound, losing its offset and reading the whole root in a compound
assignment. Named-wait checks catch ignoring false iff, reading canceled
guards and registering named items in a different generation. All restored
baselines pass. Mutation-tool self-checks were excluded from this evidence.
The entries are retained in `tools/mutants.py` for future validation.

The publication check is `tools/local_ci.py`: dev/release/ASan/UBSan builds
and tests, release flags, banned features, SPDX, suite files, global state,
each unpublished commit's expectation boundary, README counts and both
Clang tools. Its summary accompanies publication. Existing expectation
migrations remain separate from implementation commits. The dynamic-container amendment was subsequently approved; the P1
objective is not complete.

## Approved dynamic-container heap foundation

The user approved the complete dynamic-array/queue amendment on 2026-10-23,
including its project choices. SIR_SPEC 4.2.1 and RUNTIME_ABI 6 record the
contract in both languages before implementation.

`Context::heap()` now owns a typed heap with generation-checked container
and element-cell tokens. `Heap::construct`, `copy`, `fromValues` and nested
writes make independent values; queue insertion, deletion and movement keep
the identities of retained cells. Old refs can read and write detached
cells. Destination queue bounds apply to copying and insertion, and the
kernel returns warning categories for the execution tier to report.
Flattened element layouts preserve integral words, real/string/opaque-handle
words and typed nested-container edges, without slang or SIR dependencies.

Allocation uses a selectable standard memory resource. Transactions roll
back every newly allocated container and cell on failure; failed mutations
leave the variable, old refs and output buffers unchanged. Public operations
return errors and do not leak allocation exceptions. Iterative copying and
tracing handle deep nesting without recursive native calls. Exact collection
follows explicit container/cell roots, retains reachable detached cells and
reuses reclaimed entries with new generations. Marking failure performs no
sweep. The caller must supply all roots at an allowed safe point.

Fifteen new kernel tests include 56 integral width/domain/bound combinations
and 450 operations each (25,200 total), compared after every operation and
collection with the independent list-based `valsem::ContainerValue` model.
Other tests cover 2,500 nested levels, real/shortreal/string/aggregate layouts,
same-size resize, self-copy, queue truncation, every allocation point of
construction/copy/mutation/marking, 4,096 table-reuse cycles and two isolated
Contexts. Six deliberate mutations (wrong cell, shared nested value, leaked
rollback cells, missed marking edge, ignored generation and wrong insertion
position) were all caught by actual kernel tests, excluding helper checks.
The restored baseline passes. A 100-sample run seeded with `20261023` also
passes 2,520,000 operations; independent samples keep its cost proportional
to the scale. The nightly reference gate now includes the heap tests.

Reclamation and rollback use pre-sized free-list storage and indexed writes,
so their no-allocation contract is structural. A further fault-injection test
reuses reclaimed entries, fails partway through copying and retries before
collection, checking old reference contents and stale-token rejection. Missing
(X/Z) construction sizes return InvalidSize; invalid root kinds are rejected
even with token zero.

This is the shared kernel foundation. The front end and execution tiers are
not connected to these heap operations yet; SystemVerilog dynamic-array/queue
support and automatic safe-point root publication remain unfinished. The
44-probe inventory and its 23 Icarus-passing gaps therefore remain unchanged.
This foundation does not complete the approved amendment or the P1 target.

The final source validation at `3d7b1a4` passes `tools/local_ci.py`:
dev/release/ASan/UBSan configure, build and tests; release flags; banned
features; SPDX; suite files; global state; all 13 then-unpublished commit
expectation checks; README counts; clang-format and clang-tidy. Dev has
2,110 tests: 2,108 pass and the same two known skips remain. The unit assertion
review notices reflect indenting the unchanged matrix and replacing its
word comparator with an equivalent fatal assertion; comparisons of length,
a and b remain present. No suite body or existing expected output changed.
