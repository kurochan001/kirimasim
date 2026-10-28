# Proposal: program net updates and deferred assertion disable

Status: the user explicitly approved the MOS-component and delayed-propagation
amendment in section 1 on **2026-10-29**. Implementation is in progress.
Ordinary resolution and its strength-root bridge
already fall under the approved program rules. Section 2 implements the
existing approved flush rule, not a new flush point.
Baseline: `ef958a6569d7`.

This proposal concerns Issues [#63](https://github.com/kurochan001/kirimasim/issues/63)
and [#62](https://github.com/kurochan001/kirimasim/issues/62). It does not
change existing expected values or the time-slot reference algorithm.

## 1. Resolve program-driven nets in the originating evaluation set

The approved [program amendment](program-execution-proposal.md) requires
connected net driving and resolution in the reactive update (24.3.2).
However, [MOS/switch section 6](mos-switch-proposal.md) explicitly queues
every dirty strength component in Active, including Reactive-origin updates.
The kernel API comment in `sim.h` states the same fixed-Active rule.

Fixing an ordinary synthetic resolver to follow the driving update, including
its bridge into registered physical contributions, implements the existing
program rule. The approval requested here concerns changing the fixed-Active
component solve and the fixed Inactive/Active delayed-propagation policy.

The current implementation reproduces stale reads after a program's `#0`
both for an ordinary mixed-driver net and for a registered strength component.
Moving every resolver permanently to Reactive would also delay module-origin
updates and is not the proposed correction.

Approved semantic amendment:

- Internal net resolution caused by an update in the active evaluation set
  settles in Active; resolution caused by an update in the reactive
  evaluation set settles in Reactive, before subsequent Re-Inactive work.
- Preserve atomic component publication, complete strength comparisons,
  coalescing of uncommitted updates and separate committed transitions.
- Keep startup resolution in Active. Preserve the existing delay and
  cancellation rules; any queued propagation must retain the set of the
  update that scheduled it, including zero-delay Inactive/Re-Inactive work.
- Ordinary user processes retain their declared/caller thread set. A module
  procedure or continuous assignment awakened by a program still runs in
  Active after the reactive set drains. Do not prioritize that Active work
  ahead of the remaining Reactive/Re-Inactive/Re-NBA events.

Update the conflicting scheduling wording in `mos-switch-proposal.md`
section 6 and the relevant scheduler/SIR descriptions. Extend internal
representation only as needed to preserve that update set. External ABI
versions and the format-0 version stay unchanged; this is not the P2 freeze.

Verification must cover Active and Reactive changes on the same resolved
net, program drive/release with weak module contributions, registered MOS
components, Re-NBA/zero-delay propagation, startup and the unchanged ordering
of ordinary module waiters. Existing tests and expectations remain intact.

## 2. Identify the outermost procedure scope for deferred report flushing

[SCHEDULER_SPEC](../spec/SCHEDULER_SPEC.md) sections 15.3 and 15.4 require
discarding pending deferred assertions when the outermost procedure scope
is disabled. The current VM only finds live block activations, so it also
misses this rule after the block has left but its reports remain pending.
It must distinguish this named outermost scope from an inner named block.

Implementation of the existing rule:

- Applying `disable` to a procedure's outermost named scope discards that
  procedure's pending observed/final deferred reports, including pending
  reports after a completed activation. Preserve unique/priority reports
  and reports already confirmed in Observed/Postponed.
- Inner-block disable, ordinary block exit and delay resume are not this
  flush point. Keep the existing event/wait-resume flush behavior.
- Add an optional, numeric outermost-scope association to each SIR process;
  it remains available independently of the VM's live activation stack.
  Front identifies it from the procedure body, including a leading timing
  control, and emits a real disable operation for self-disable when a
  flush is needed. No runtime name lookup is introduced.
- Verify the numeric association in SIR and provide a kernel operation
  that discards deferred reports only. Preserve existing format-0 text
  expectations; its already incomplete dump may omit this internal field.
  A future complete SIR representation must retain it.
- Describe the numeric association in the SIR specification without changing
  the scheduler's approved flush points. External C ABI versions remain unchanged.

Verification must cover active and completed outer activations, self-disable,
inner disable, observed/final reports, preserved unique/priority reports,
reports from called subroutines and confirmed reports that must survive.

The 2017 IEEE text for 16.4/16.4.4 was independently checked; the complete
2023 text of these paragraphs remains **unconfirmed**. The implementation
rests on the project's already approved scheduler rule, not a new claim of
2023 verification. Confirm the corresponding 2023 clauses from an authorized
copy before adding such a claim. No IEEE text or examples are copied here.

## 3. Reported expectations that do not justify implementation changes

- The first #63 test expects a module waiter awakened by Re-NBA ahead of
  a Reactive waiter. That contradicts the approved reactive-set drain rule;
  immediate program completion can also end the run before the Active waiter.
- The #62 syntax test expects deferred assertion expressions to be evaluated
  after later assignments. Expressions and by-value arguments are captured
  at the assertion statement; only reporting/action execution is deferred.
- The #62 latch test expects a variable written by the latch itself to
  restart that latch. The implicit-sensitivity exclusion applies here.

Do not change implementation semantics to satisfy these expectations.
Any corrections of existing external tests belong to a separate change.

## 4. Implemented checkpoint

- `d5248cdf` fixes Issue #64's packed selections, fixed prefixes of
  multidimensional unpacked arrays and exclusion of block locals. New
  regressions also preserve `@*` identifier sensitivity and expression-modport
  dependencies. The SIR golden migration is isolated in `fa6c51c3`, with
  its [separate rationale](p1-static-prefix-expectation.md).
- `c7623af3` implements the existing outermost-disable rule described in
  section 2. Fault-injection tests cover scratch allocation and callback
  failure without exceptions crossing the kernel boundary.
- `a0c53242` fixes ordinary scalar/array resolution and the strength-root
  bridge under the existing program contract. It preserves the reference
  algorithm and the ordinary module processes' thread set.

At the `d72e1809` checkpoint, the amendment in section 1 remained unapproved
and unimplemented for fixed-Active MOS component solving and fixed-region
delayed propagation. The registered-MOS reproduction still read stale values
after program `#0`. The user subsequently approved that amendment on
2026-10-29; its implementation and verification are recorded separately below.

The combined implementation and separate golden migration pass development,
release, ASan and UBSan tests. Development has 8,199 tests: 8,197 pass and the
two existing unsupported cases skip. These changes do not establish P1
completion.

## 5. Verification record

- `tools/local_ci.py --presets dev`: configure, build, all 8,199 tests,
  banned API/SPDX/suite/global-state checks, README counts, clang-format
  and clang-tidy all pass. The two existing unsupported cases skip.
- Complete release tests: 8,198 pass, two skip (8,200 total).
  Complete ASan and UBSan tests: 8,197 pass, two skip each (8,199 total).
  Release flags were checked with `tools/check_release_flags.py`.
- The eight new unit tests were also rebuilt and rerun for release, ASan
  and UBSan after the final test-check tightening; all pass.
- `tools/check_expected_change.py` accepts the separate golden migration
  because it contains no implementation change, and accepts the subsequent
  implementation commits because they preserve existing expectations.
