# Managed-event ownership during kernel execution

[日本語](managed-event-runtime.ja.md). Status: kernel implementation checkpoint,
2026-10-25; automatic-event source execution remains disabled.

The grounds are sections 5/6/8 of the [approved automatic-capture contract](automatic-capture-proposal.md),
[its specification mapping](p1-execution-contracts-2026-10-25.md), SIR_SPEC
3.2.1/7/8, RUNTIME_ABI 4/6, SCHEDULER_SPEC 9.4/9.5 and IEEE 1800-2023
6.8, 6.21 and 15.5.3. This extends the [registry checkpoint](managed-event-registry.md).
The implementation and tests are original; no external simulator implementation
or test code was read. No dependency, C ABI version or SIR format version changes.

## Ownership boundary

`EventValue` carries null, a permanent legacy handle or a managed token.
`Scheduler::resolveEventValue` rejects stale tokens and unversioned aliases of
managed physical objects. Typed waits resolve a concrete object once and acquire
pins before registration. Invalid/null named targets and staging allocation
failure preserve the existing wait. An initial monitor callback can cancel or
replace that wait; registration never resurrects its old pins.

`triggerValue` holds its target through synchronous callbacks and late notices.
`triggerNbValue` owns a concrete target independently of the issuer frame and
process record; it preserves NBA/Re-NBA origin, including process-ID reuse.
Pending records use serial-checked tickets and an intrusive free list. Exhausted
64-bit serials retire their records. Delay overflow releases the staged pin;
null triggers are no-ops. Delivery moves the pin into a local before making the
record reusable, so a callback can cancel, collect and grow/reuse the table.

Monitor specifications can declare typed event dependencies and capture actual
`triggeredValue` reads. Publication deduplicates objects, retains new dependencies
and filters obsolete ones after an indexed selection changes. Old dependencies
remain pinned during callbacks, including self-cancellation. Nested monitors
without capture do not add reads to the enclosing capture. The first capture
error is retained. Time advancing alone does not wake a triggered-state monitor.

Wake, re-wait, cancellation, process/program completion and scheduler termination
drop owned pins after detaching registrations. An executing callback retains its
local pins until return. Natural quiescence retains valid subscriptions for
external resumption; a future VM whole-run teardown must explicitly cancel them.
Context must outlive its pins. Tokens remain Context-local.

## Evidence and limits

Twenty-three new `ManagedEventRuntime` tests pass. They cover both scheduler
groups, mixed waits/iff, dynamic and static monitor dependencies, nested and
self-canceling callbacks, concrete delayed targets, issuer-record reuse,
termination, overflow, failed allocation and reusable records. Sixteen compiled
faults are caught on `77cb29bd`, before the allocator initialization correction
below; compile failures are not detections.

The old canceled-ticket fault initially **survived**: count and final-time checks
missed a wake at tick 1 instead of 3. Recording and asserting the actual wake time
caught the same fault with unchanged production source. Later error-latching and
cleanup changes plus two more tests preceded the final 16-control run. A separate
new regression test reproduced an allocation error being overwritten by a later
invalid read (`Internal` instead of `ResourceLimit`); retaining the first error
fixes it. [Control records](../measurements/2026-10-25/event-runtime-negative-controls.json)
preserve the initial survivor, source/test hashes and final detections.

Full clang-tidy subsequently identified allocator-dependent PMR move assignment
in a reused `Process`. `8b652e3d` reconstructs that empty record with its Context
allocator, preserving its wait generation; targeted clang-tidy and the 23 tests
pass. Dataset/control records retain their exact earlier source hashes, rather
than being relabeled as executions after this correction.

The fresh [87 probes](../measurements/2026-10-25/icarus14-event-runtime-probes.json)
preserve every native/reference status and passing byte. Historical Icarus-13
gaps remain 21. The exact [P1 population](../measurements/2026-10-25/event-runtime-population.json)
remains 1,451/1,466 with zero failures and 15 unsupported; pinned
[sv-tests](../measurements/2026-10-25/event-runtime-svtests.json) remains 244/344.
No fresh clean Icarus 13.0 run, Linux rerun or speed comparison is claimed.

The [final integrated local CI](../measurements/2026-10-25/event-runtime-validation.json)
passes all 21 checks: dev/ASan/UBSan pass 4,940/4,942, release
4,941/4,943, with zero failures and two known skips per preset.
Public C++20 headers, PerfCount, all 29 existing SIR dumps and static checks pass.
The three historical expectation migrations are separately approved in
`11096dc3`; their source fixtures remain unchanged. A [separate compiled reuse
control](../measurements/2026-10-25/event-runtime-reuse-control.json) catches a reset
wait generation; restored production passes that existing test plus all 23 new
tests. Its initial unused-variable compile failure is recorded and is not a
behavioral detection. Pre-integration failures are retained in the CI record.

PMR failure budgets do not exhaust all ordinary scheduler allocations. Full
64-bit pending-ticket exhaustion is not confirmed; inspect the retirement guard
and add a dedicated audit before making that claim. Canceled queue tickets may
remain as stale metadata until normal scheduler consumption.

## Remaining source connection

Automatic event construction and typed operands still need SIR/verifier/text and
VM/front connections. Ordinary/captured/SSA/input event holders must publish
complete roots and safe points; unpublished values need temporary pins. Only
then can source declarations be enabled and the complete original source matrix
run. Event rebinding/comparison, event-containing dynamic containers and public
VPI event values require separate contracts. P1 and full Icarus coverage remain
unfinished. This kernel checkpoint is the requested stopping point for merging.
