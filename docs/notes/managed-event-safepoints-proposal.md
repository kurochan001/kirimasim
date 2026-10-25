# Managed-event collection at published VM instruction boundaries

[日本語](managed-event-safepoints-proposal.ja.md). Status: **proposal (not approved)**,
2026-10-25. Proposed amendment to RUNTIME_ABI 6 and automatic-capture contract
5/6/8. No implementation of this new collection timing is authorized yet.

## Why review is needed

RUNTIME_ABI 6 currently says: "Collection runs only between time slots or at
region boundaries with all temporaries registered and no DPI call in progress."
The approved automatic-capture contract also refers to existing safepoints.
Meanwhile it requires event storage reuse proportional to retained values and
activities, including repeated automatic declarations and completed scopes.

A process or function can repeatedly enter a block and construct an event
without waiting. Boundary-only event collection can retain every unreachable
registry row until the process yields. Six new adapter/provider tests prove
bounded reuse with **explicit caller collection**, not with VM execution.
Enabling source construction without resolving this gap would not prove bounded
source-level reuse. The already documented container benchmark growth is a
separate issue; this proposal does not alter container heap collection.

## Proposed normative amendment

Add this managed-event exception to RUNTIME_ABI 6:

> The managed-event registry may additionally collect at a VM instruction
> boundary, including inside a synchronous function or guard evaluation, when
> the complete event root set has been published and every in-flight kernel
> operation retains its concrete event pins. Collection executes no source
> callbacks, creates no simulated event and changes no scheduling region or
> simulation time. No collector reentrancy or partially published event store,
> argument/return transfer or registration is permitted. A DPI call, including
> a suspended task, continues to forbid collection. Missing/unsupported event
> root providers forbid collection and source enabling; failure must not sweep
> a partial root set.

Keep the container heap's current boundary-only rule. This selects no new
dependency, C function-table ABI, public option, hidden configuration knob,
performance threshold or existing expectation change. Runtime checks use the
existing Context result/diagnostic policy for resource and internal failures.

## Required implementation and evidence

- Publish typed event slots, ordinary/scoped/captured fields, input values,
  every active SSA region (including nested/reentrant calls and callbacks),
  deferred arguments and unpublished return/argument values. Runtime waits,
  delayed triggers and monitor callbacks keep their existing concrete pins.
  Numeric/real/string/ref encoding bits are never guessed to be event handles.
- Before an `event.new` allocation can require another physical row, provide a
  legal collection opportunity with the complete root set. Internal batching
  may defer work only with bounded overhead proportional to retained roots and
  outstanding operations. No source instruction or side effect is retried.
- Clear inactive SSA/root descriptors and source-owner fields on unwind,
  return, disable, completion and failure, while preserving descendant extents
  and pending operations. Restore prior hooks on all exits. Do not collect
  during termination/final cleanup contrary to SCHEDULER_SPEC 16.2.
- Check long **nonwaiting** process/task/function loops, lexical reentry,
  recursion, event return/copy-in, unread arrays, nested guards, cancellation
  during callbacks, Active/Reactive delayed targets and source allocation
  failure. Assert peak/allocated physical rows are bounded, not just final
  zero live objects. Add compiled faults for omitted roots and premature reuse.
- Preserve old source/oracle files, all existing CI gates and PerfCount budgets.
  Measure new-path cost and confirm the ordinary non-event fast path. No full
  P1/Icarus claim follows from passing this event-specific matrix alone.

## Alternatives and recommendation

The complete VM publication protocol is **not confirmed**: the current temp
stack is shared across nested calls/callbacks and the current collector skips
native execution. Confirm the new protocol with the full matrix and omitted-root
controls above before enabling it. Termination/final routines keep their existing
deferred cleanup and allocation-limit policy; bounded normal-execution reuse is
not a claim of bounded allocation inside a long final-cleanup loop.

1. **Recommended:** approve the managed-event exception above. It uses the
   existing registry/provider/pin architecture and requires full publication
   before any new collection point.
2. Retain boundary-only collection and explicitly retain its scalability limit.
   This alone cannot satisfy the stronger bounded automatic-event reuse goal.
3. Replace traced event holders with immediate reference ownership for every
   slot, field, SSA temporary and transfer. This may avoid new collection
   points, but needs a separate ownership amendment, more mutation integration
   and review of existing provider tests; it is not implicitly authorized here.

The human decision is whether to permit option 1. Type-field/provider work and
the already approved oracle migrations can proceed independently meanwhile.
The publication protocol and the tests above must be complete before enabling
the exception or source automatic-event construction.
