# Event-controlled NBA execution proposal

[日本語](event-nba-proposal.ja.md). Status: **approved (2026-10-25);
implementation in progress**. The original 2026-10-24 proposal body is retained
as the decision record; approval requests, future tense and unapproved-status
statements below describe that original proposal state. The user approved all
six contracts together on 2026-10-25. See the [specification mapping](p1-execution-contracts-2026-10-25.md).
Approval does not claim implemented support, authorize changes to existing
expectations, or introduce dependencies.

## Reason for review and primary basis

P1_SCOPE requires the Icarus-supported subset, but explicitly requires review
of missing detailed semantics. SCHEDULER_SPEC 8/13 already specify early RHS
and NBA target capture, one evaluation of a repeat count, and ordinary NBA
regions. They do not define persistent nonblocking event subscriptions,
overlapping items, late-ready ordering or callback ownership after the issuer
ends. SIR_SPEC 7.2 describes process-suspending waits; SCHEDULER_SPEC 10.8
currently permits freeing an ended issuer without considering these callbacks.
These contracts need amendments before implementation. This proposal selects
no dependency and does not change the P1 target.

Primary clauses checked: IEEE 1800-2023 4.6, 4.9.4, 9.4.2, 9.4.5, 10.4.2,
13.4.4, 15.5 and 18.14. Relative operand evaluation and ambiguous concurrent
orders must remain distinguished from standard-mandated ordering. The
notification coalescing and late-ready queue rules below are explicit project
choices for review, not claims that a reference implementation proves them.

## Proposed scheduler contract (add to sections 8, 9.5 and 13)

1. Evaluate the RHS, snapshotting every aggregate/container leaf before later
   effects. Evaluate the repeat count once, retaining its signedness, width and
   four-state domain. Evaluate LHS selectors once, then register the control
   synchronously before executing the next source statement. With no repeat,
   the count is one. Nonpositive or X/Z counts skip the control and enqueue an
   ordinary NBA. This fixes the project order as RHS, count, LHS, registration;
   the count/LHS relative order is a proposed addition to the existing RHS-first
   rule. The NBA cannot target automatic variables or container cells; strict
   slang diagnostics remain in force.
2. Registration does not suspend the issuer, create a language process, draw a
   child RNG seed, or add a child to `wait fork`. It is active immediately,
   including for a following `->e` in the same uninterrupted source process.
   Consecutive synchronous triggers/writes are distinct notifications and can
   exhaust the count without allowing another process to resume between them.
3. Reuse the slot, named-event, expression-monitor and `iff` rules of 9.5, with
   a registration-owned persistent countdown. OR/comma items are one control:
   at most one decrement per committed source notification, even when duplicate
   items or both a direct subscription and a monitor match. Evaluate relevant
   items in source order, including their existing guard rules, without
   short-circuiting after the first match. Update each monitor's previous value
   and dependencies before the next notification. A false/X/Z `iff` does not
   qualify. Separate writes, separate named triggers and queued callback-induced
   changes have separate notification identities. Guard/monitor writes are
   queued under the existing outermost-walk rule, not recursively interleaved.
   Allocation failure while maintaining dependencies ends with ResourceLimit.
4. An exhausted control unregisters exactly once and transfers one owned
   ordered payload to NBA, or Re-NBA for an issuer from the reactive set.
   Preserve the issuer's group; the triggering process does not choose it.
   Multiple aggregate leaves share one control and one countdown, then update
   in the existing declared leaf order. Invalid selections retain normal
   no-write behavior, but do not omit control/guard evaluations merely because
   the payload has no effective destination.
5. Assign an issue sequence to the complete statement at registration. Ordinary
   current-slot NBAs and newly ready event-NBAs share that issue order among
   updates still pending in the same region. Preserve the existing priority
   and order of timed NBAs that were already due when the slot opened. An event
   control that becomes ready during the slot joins the current-ready queue,
   even if issued in an earlier slot. Insert it by issue sequence into the
   unexecuted current-ready suffix; never undo or reorder an applied update.
   Becoming ready during NBA processing may therefore put an older statement
   before remaining newer current-ready statements, but cannot precede work
   already performed. Across causally distinct readiness times there is no
   retroactive ordering. Existing race-mode rules remain unchanged.

## Proposed lifetime contract (amend section 10.8; ABI 5/6)

A pending control owns its target descriptors, typed values, count, monitor
state and a generation-checked reference to the issuer's callback origin.
It survives normal issuer return, named disable and disable fork just as
already-issued NBAs do. Cancelling the issuer's ordinary waits does not cancel
this registration. It is not a live child and does not keep a completed program
initial thread alive. Normal program completion, `$exit`, global termination
and final execution retain their approved behavior; termination drops controls
that will no longer fire. A control without future external activity does not
prevent ordinary quiescent termination.

Add a reclamation condition: **no event-NBA control still needs this process's
callback origin**. Initially retain the kernel process record and tier frame
until the last such registration is ready or discarded. The tier must retain
the callback contexts they actually use; it must not leave pointers into popped
task activations. The strict language ban on automatic references in NBA event
controls remains. An origin supplies the existing process RNG stream, scope,
source/report identity and scheduling group, including after issuer end; neither
copying/reseeding its RNG nor attributing callbacks to the triggering process
is permitted. Callback invocation uses a fresh transient activation with the
origin context, not the issuer's suspended/resumed instruction position.

Reference counts and generation-checked handles belong to Context, without
shared ownership pointers or mutable global state. Release the retained origin
as soon as the last callback finishes; a ready NBA's owned values alone do not
keep the process record. Recheck ancestor reclamation when releasing it. Extend
the memory guarantee to live processes, their retained ancestors, pending reports
and pending event controls; it must not grow with already completed controls.

Typed tracing includes all pending-control payloads and callback roots before
any monitor initialization can execute. Publish the complete registration before
callbacks; use stable IDs/serials so callback-triggered registration cannot
invalidate a traversed pointer. Allocation/sequence exhaustion has the existing
ResourceLimit outcome, with no partially observable subscription or update.
Completion/discard removes dependencies, releases ownership and frees count and
payload exactly once. Hooks must be detached before callback contexts are freed.

## Proposed SIR and kernel boundary (sections 5.3, 7.2, 7.3)

Add a nonterminating `nba.event` instruction with a typed count, event-item
descriptor list and ordered typed target/value list. Event descriptors use
existing signal/named/monitor forms and guards. Capture operands must already
be owned snapshots; one instruction represents the whole assignment, including
zero effective destinations. No SSA value is borrowed after registration.
The verifier checks count type, target eligibility, leaf types/order, callback
signatures and valid descriptor references. Text round trips retain the whole
control rather than expanding it into a fork or per-leaf subscriptions.

The shared kernel owns countdown, matching, payload roots, readiness ordering
and lifetime. The VM mechanically copies typed operands and origin handles to
that API and publishes roots; it does not decrement/rearm on process resumes.
The public kernel interface remains C++20 and independent of slang/SIR. The
future C++ tier uses the same registration API. No C ABI change is selected by
this internal instruction; an external entry point requires its own contract.

## Original design evidence and required verification

[Portable observations and original sources](../measurements/2026-10-24/event-nba-design-probes.json)
record 15 original sources against both installed compiler/runtime pairs:
Icarus 13.0 stable and 14.0 development. These are exploratory observations,
not approved expected outputs or a feature-coverage total. Both references:

- Register before a following same-thread trigger, count two same-thread
  triggers, capture the count/index once, and skip a zero repeat.
- Leave the registration independent of `wait fork` and issuer disable.
- Complete `repeat(2) @(e or e)` after the first trigger; overlapping signal/
  expression items also complete after one write. The proposed coalescing rule
  deliberately needs review rather than copying this result.
- Produce 2 in both `q<=@(e)1; ->e; q<=2` and
  `q<=2; q<=@(e)1; ->e`. The proposed pending issue order gives 2 and 1.
  After the immediate NBA has drained before a later trigger, the observed
  final value is 1; the proposal preserves that causal behavior.
- Reject the automatic condition reference; strict kirimasim parsing rejects
  it too. This proposal adds no permissive slang mode.

Before accepting an implementation, exercise immediate registration, same-thread
repeated writes/triggers, duplicate/overlapping items, mixed/guarded events,
monitor reattachment, all typed count boundaries, issuer return/disable/reuse,
RNG continuation, nested callbacks, roots across collection, invalid targets,
multi-leaf snapshots, NBA-induced changes, timed/current queue interactions,
program completion and resource failure. Use independent kernel/SIR tests,
original source matrices and compiled mutations that fail observable outputs.
Do not weaken existing expected results. Run full local CI and versioned feature
comparisons; classify reference disagreements explicitly. Full P1 parity remains
unproven even if this mechanism is completed.
