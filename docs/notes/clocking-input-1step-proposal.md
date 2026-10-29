# Clocking inputs with one-step sampling

[日本語](clocking-input-1step-proposal.ja.md).
Status: **approved by the user (2026-10-29); the first subset is implemented and validated**.
Baseline: `cd663ebf9d66`.

Validation on macOS, 2026-10-29: all 21 `tools/local_ci.py` checks pass.
Dev/ASan/UBSan each pass all 8,220 tests; release passes all 8,221, with no
failures or skips. Both former skips retain their original source and expected
output. Added checks comprise six source cases, five kernel/SIR unit tests
and eight CLI diagnostic cases. The same-time repeated-`run()` check also
confirms that the kernel retains one snapshot per physical time. These results
describe the uncommitted working tree based on the baseline above.

This proposal fills the clocking-input portion of SCHEDULER_SPEC part 4 and
adds its typed SIR/kernel boundary. It does not change assertion or coverage
semantics, the time-slot reference algorithm, existing expected outputs, or
the external ABI version. P1_SCOPE permits the required scope but explicitly
requires review of missing detailed semantics before implementation.

## Supported first subset

Support named clocking blocks in static module/interface instances, with
input-only integral clockvars. Accept the default input skew and explicit
`#1step`, preserving the declared width, signedness and two/four-state type.
Input sources and the clock are static scalar or packed integral storage,
including statically selected bits/parts. A clocking input may name a static
source directly or use a supported static select through an input alias.

Support ordinary value-change, posedge and negedge clock controls and waits
on the block event, `@(cb)`. Keep clockvars distinct from their raw sources.
Clockvars are read-only to source procedures and have their type's default
value until the first clocking update. No startup clock edge is invented.

Other skews, output/inout clockvars, clocking drives, cycle delays, global
clocking, default-clocking inference, dynamic/automatic or nonintegral source
storage, side-effecting expressions and unsupported event-control forms
remain positioned unsupported diagnostics. This is partial clocking support.

## Sampling and publication

1. Before events of a new time slot execute, capture the registered source
   values once. This represents the current slot's Preponed values, equal
   to the preceding Postponed values. At time zero use the state after the
   existing static initialization and before ordinary initial procedures.
   Do not schedule a timer one precision tick before a future edge.
2. Recognize clock transitions with the existing shared value/edge rules.
   A change of an input alone must not publish a clockvar or wake `@(cb)`.
3. For each recognized clocking event, queue an internal Observed action.
   It commits that block's sampled input values as a group, then triggers
   the block event. Every input in the group uses the same slot snapshot.
4. Resume event waiters only after publication. Module waiters enter the
   next Active processing; callers belonging to a program retain the
   execution origin/thread set required by the approved program contract.
   The scheduler's existing region-drain algorithm decides the order.
   Do not rewrite `@(cb)` as `@(clk)` or `@(posedge clk)`.
5. Re-entry into Active/Observed within one simulation time must not
   recapture the Preponed snapshot. Distinct recognized clock transitions
   remain distinct clocking events, including when a clock changes in NBA
   or in the reactive set. An unrelated Observed pass does not republish.

Observed publication is a dedicated kernel operation on registered
clockvar destinations, not permission for user code to write ordinary
signals in Observed. Preserve deferred-assertion confirmation and ordinary
read-only-region restrictions. Existing event cancellation, process lifetime,
interrupt and termination behavior still applies.

## Representation and ownership

Use a typed SIR descriptor for the clock trigger, source ranges, clockvar
destinations, block event and source locations. Verify types, bounds,
ownership and disjoint destinations. Use numeric IDs during execution;
names remain diagnostic metadata. The incomplete format-0 dump may omit a
descriptor initially, but any future complete SIR form must retain it.

The shared kernel owns sampling state and Observed publication. The VM
registers the descriptors before ordinary time-zero execution and reads
clockvars through ordinary typed loads. There must be no VM-only sampling
algorithm, mutable global state, new dependency or environment setting.
Static registered blocks must not keep a simulation alive without events.
Allocation failures must leave no partially registered sampler or callback
and must follow existing Context error handling.

## Verification and skip removal

Keep `tests/suite/cb/input_sample_1step.sv` and its two expected output lines
unchanged. Remove only its skip entry after it succeeds with the following
independent checks as well:

- Default and explicit skew; input aliases; 1/32/65/129-bit values and all
  four logic values; retained signedness and two-state conversion.
- Time-zero state, an empty gap between slots, input writes before and at
  the clock event, NBA writes, multiple blocks and atomic multi-input reads.
- Posedge/negedge/value-change controls, clocks updated from Active/NBA/
  Reactive, multiple edges in one slot, repeated waits and waiter cancellation.
- Kernel-native checks that compare an explicit reference snapshot with
  committed clockvars and verify publication before event delivery.
- Negative source/SIR cases for unsupported forms and malformed descriptors;
  meaningful allocation-failure and Context-isolation checks.

Updating the skip list is a stricter test requirement, not an expectation
migration. Existing suite bodies, expected output files, PerfCount budgets
and other protected expectations are not changed to make the feature pass.

## References and remaining confirmation

The existing test cites IEEE 1800-2023 14.4, 14.10 and 14.13. The approved
SCHEDULER_SPEC already describes Preponed/previous-Postponed sampling.
[Dave Rich's explanation](https://verificationacademy.com/forums/t/input-sampling-for-clocking-blocks/41134)
distinguishes raw, sampled and clockvar values and points out the revision's
clarifications. It supports the need to keep these values separate; it is
not a substitute for checking the complete 2023 clauses.

The complete 2023 wording and all clocking combinations are **unconfirmed**.
Check 4.4.2.1, 14.4, 14.10 and 14.13 against an authorized copy before claiming
complete 2023 clocking support. No standard text or example is reproduced here.
