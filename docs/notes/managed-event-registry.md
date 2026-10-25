# Managed event identity, typed roots and event time-slot state

[日本語](managed-event-registry.ja.md). Status: kernel checkpoint committed as `13006ba7`,
2026-10-25; automatic-event source execution remains disabled.

**Later checkpoint:** [runtime operation ownership](managed-event-runtime.md) now
implements typed kernel wait/trigger/monitor pins (23 tests, 16 compiled controls).
Its SIR/VM/front connection remains work. The historical three-oracle migration
was separately approved and committed as `11096dc3`; the results below describe
the earlier `13006ba7` run and are retained as history.

## Grounds and implementation boundary

The basis is sections 5/6/8 of the [approved automatic-capture contract](automatic-capture-proposal.md),
its [specification mapping](p1-execution-contracts-2026-10-25.md), RUNTIME_ABI 4/6,
SIR_SPEC 3.2.1/7/8 and SCHEDULER_SPEC 9.4/9.5. The language clauses are
IEEE 1800-2023 6.8, 6.21 and 15.5.3. Implementation, tests and the generation
audit are original; no external simulator implementation or test code was read.
No dependency, source option, public C ABI version or SIR format version changes.

`Context::managedEvents` owns a registry of opaque, generation-checked event
values. Zero is null; malformed/stale nonnull tokens fail validation. A
move-only runtime pin retains the concrete object independently of source
holders, process ancestry and PCs. C++ pin operations check registry identity;
raw tokens remain Context-local and are not transferable values. Bookkeeping
allocation precedes backend allocation, and a failed create publishes no token.
The backend contains allocation failure and uses the scheduler's existing named
objects. Reuse clears waiters, monitor references and triggered state.

Typed source holders are traced at safe points; unpublished temporaries and
in-flight runtime use need pins. Trusted nonallocating providers enumerate roots
without executing source. Duplicate roots identify one object. A provider error
prevents the entire sweep, and reentrant mutation/collection is rejected.
Root-only changes make collection eligible. An intrusive free list reuses
physical objects without allocating during reclamation. Maximum generations
are retired rather than wrapped. No source-language process is added.

The kernel activation store now has an Event field provider. It enumerates all
live physical event fields/elements, including unread fields and scopes retained
by descendants. Capture views do not multiply physical holders. Ordinary
integers containing the same bits are not roots. Null and permanent legacy
static event handles need no managed root. Event-root dirtiness is separate
from heap-root dirtiness, so heap acknowledgement cannot lose an event-only
replacement or final scope release.

This checkpoint does **not** connect event creation or managed operands to
SIR/VM/front. Ordinary frame/slot/SSA event root publication and automatic
ownership of pins by pending triggers, waits and callbacks remain work.
The tests retain runtime pins explicitly when using existing scheduler APIs;
they do not establish that those legacy APIs acquire managed pins themselves.
Automatic event declarations remain diagnosed as unsupported, even unread ones.

## Existing event-state corrections

The boundary audit found that an untriggered event was reported triggered at
`UINT64_MAX`: the never-triggered timestamp sentinel equaled a valid time.
`Scheduler::triggered` now combines an explicit has-triggered bit with the
current-slot timestamp; trigger and managed-object reset maintain that bit.
This is one kernel rule for legacy and managed events, at every representable
time. Advancing time still causes no callback solely for clearing triggered.
An original source printed `before 1` before the fix and `before 0` after it;
its explicit trigger still prints `after 1`.

The same audit found that ordinary functions were blanket-rejected for reading
`.triggered`. Front now records their event-object reads and rejects them at
actual implicit-sensitivity callers, including nested/recursive calls, while
allowing ordinary calls and explicit guards. Explicit guards also keep the
physical event dependencies of function-local static events; excluding ordinary
called-frame slots must not exclude those persistent event objects. An original
nested-function wait previously ended at tick 7 without waking; it now prints
`woke 7` and `total 1`. Static implicit event sensitivity remains unsupported.

## Verification

Twenty original kernel tests cover token validation, pins, typed roots,
Context identity, reset/reuse, failed enumeration, reentrancy, stale wait/monitor
cleanup, legacy coexistence, Active/Reactive delayed targets, the maximum tick,
unread retained event arrays, root-only replacement and eight bookkeeping
allocation budgets plus an explicit backend failure. Whole-scheduler allocation
exhaustion is not confirmed; its ordinary containers need separate injection.

The [source generator](../../tools/gen_event_triggered_epoch_matrix.py) adds 27
original designs: seven containers at ticks 0/7/UINT64_MAX, plus global and
function-private named-event waits through nested calls at those three ticks.
The [boundary checker](../../tests/tools/check_event_triggered_function_boundary.py)
rejects 12 direct/nested/recursive function uses under four implicit contexts.
All 49 focused CTest cases pass. Sixteen actual compiled faults are caught by
these tests; compile failures and judging self-checks do not count.

The [standalone audit](../../tools/managed_event_generation.cpp) executes all
4,294,967,295 production-width generations on one row, then verifies retirement,
a distinct next row/token and rejection of the stale last token. It succeeds.
A separately compiled fault disabling retirement completes the same range and
fails the retired-object invariant (exit 4); it is not claimed to have reached
that fault's next-token check. No reduced-width generation knob was used.

The frozen 87-probe native/installed-Icarus-14 comparison preserves every status
and passing byte from the source-capture checkpoint. Historical Icarus-13-passing
gaps remain 21. The new 27-source event matrix passes natively; installed Icarus
14.0 devel rejects all 27 at compilation, so it provides no passing-output
cross-check for these new oracles. Clauses and the approved contract remain the
grounds; reference failures do not define native behavior. The exact population
remains 1,451/1,466, zero failures, 15 unsupported; pinned sv-tests remains 244/344.
No fresh clean 13.0 run, Linux rerun or standard speed comparison is claimed.

The [full local-CI record](../measurements/2026-10-25/managed-event-validation.json)
passes 17 checks; all four preset test steps fail only the same historical
three expectations. Dev/ASan/UBSan pass 4,914/4,919 and release 4,915/4,920,
with two known skips each. Builds, C++20 public headers, static checks,
PerfCount and all 29 existing SIR dumps pass. The implementation commit passes
its separate expected-change check. The [expectation migration](automatic-capture-expectation-migration.md)
remains unapproved and unchanged; all-green CI is not claimed.

Records retain [compiled controls](../measurements/2026-10-25/managed-event-negative-controls.json),
[the full generation audit](../measurements/2026-10-25/managed-event-generation.json),
[87 probes](../measurements/2026-10-25/icarus14-managed-event-probes.json),
[27 reference source attempts](../measurements/2026-10-25/managed-event-reference-matrix.json),
[population](../measurements/2026-10-25/managed-event-population.json) and
[sv-tests](../measurements/2026-10-25/managed-event-svtests.json).
Runs preceded `13006ba7`; recorded source hashes match it. This is the checkpoint
requested by the user before pausing further feature work. P1 and complete
Icarus coverage are unfinished.

## Resume from this checkpoint

1. Review the concrete historical expectation migration and, if approved, apply
   it in its own commit before reporting all-green local CI.
2. Connect `event.new` and typed loaded-event wait/trigger/triggered operands to
   SIR printing/verifying and tier 1. Keep legacy slot forms, strict diagnostics,
   the approved null behavior and single evaluation of target/delay expressions.
3. Publish ordinary/captured/SSA/input event roots and register complete kernel
   scope roots at safe points. Keep an unpublished value pinned until publication.
4. Let pending triggers and wait/guard/callback registrations own their concrete
   event pins, including rollback, delivery, cancellation and program completion.
   Preserve NBA/Re-NBA and avoid retaining the issuer's executing frame.
5. Add the full original automatic-event source matrix and compiled controls.
   Event rebinding/comparison, event-containing dynamic containers and public VPI
   event values remain outside this amendment and require their own contract.
