# Deep checks of the kernel's invariants in debug builds

[日本語](deep-checks.ja.md) · Proposal, 2026-11-08. Steps 1 to 3 are implemented.

## Why

kirimasim reports faults as values: the SIR gate (`sir::verifyDesign`) runs in every build, the VM ends a run with
`EndReason::Internal` (driver exit code 3), and each kernel area returns its own error for a call that breaks its
contract (heap, activations, NBA, objects, strings, strength, managed events, patterns, clocking). The debug
presets also build slang with its own assertions and the C++ library with its checks (`_GLIBCXX_ASSERTIONS`,
libc++ hardening), and run every test under the sanitizers.

What nothing checks is the kernel's own state against the invariants it is built to keep. If a kernel fault
breaks one, the run goes on and gives a wrong result instead of a fault. kirimasim has no `assert`, and an
`assert` would not fit: it vanishes from release builds and stops the process instead of returning an error.

## What is checked

| Area | Invariant | If broken |
| --- | --- | --- |
| Values | the bits above a value's width are 0 in both its a and b words (wide.h) | comparisons, reductions and printing go wrong silently |
| Future events | the event heap keeps its order by (time, region, sequence); the timing wheel's count equals the events in it; no event is earlier than the current time | events run out of order or are lost |
| Processes | a released record is done, and no queued item is current for it (stale ones may stay until they come up); the count of unfinished runtime processes is right; a process waits in at most one place | a reused record is woken by a stale event |
| Subscriptions | a waiter of a slot or a named event that is current (it carries its process's wait number) names a process that is not done | stale wake-ups |
| NBA | the count of live event controls equals the Active records; at the end of a time slot no captured NBA is ready or being applied | leaked or doubly released records |
| Heap | in each pool, the free places are distinct, empty and below the last generation, every other empty place is retired, and no empty place is marked | leaks, use after release |
| Activations | the live, lease and word counts equal the sums over the records, and the free chain holds only released records, once each | stale frame reads |

The store hands out element pointers, so the value check runs where the scheduler announces a change to what
watches the slot; a slot that nothing watches is not checked.

## How

- **A build option, not a run-time switch.** CMake option `KIRIMASIM_DEEP_CHECKS` is on in the debug presets
  (dev, asan, ubsan, tsan, fuzz) and off in release. The checks are written as `if constexpr` on a constant that
  a private kernel header sets from a compile definition, so both builds compile them. Nothing reads the
  environment, and there is no mutable global state.
- **A violation is a value.** The Context records the first violation (the check and what it saw). The scheduler
  ends the run at its next safe point with `EndReason::Internal` and `deep check <name>: <detail>`, which the
  driver reports as an internal error with exit code 3. No exception crosses the kernel boundary, and nothing
  aborts.
- **Where.** At each advance of time, when every region of the time slot is empty: the future events, the
  processes and their waits, the NBA records, the heap's pools and the activation records. The last advance of
  a run, which finds nothing ahead, checks them once more. The value check runs as described above.
- **What a correct run sees.** Nothing: the checks only read, count nothing that PerfCount reads, and change no
  result.

## Tests

- Every existing test runs with the checks in the debug presets, so the whole suite is their oracle.
- Each check has a mistake in `tools/mutants.py` that breaks the invariant it guards; a test has to fail through
  that check.
- A test that trips a check today shows a kernel fault. It is reported, with the check's message, before it is
  fixed.

## Order

1. The option, the Context's record and its report, with the value and future-event checks.
2. Processes and subscriptions.
3. NBA, heap and activations.

Each step is its own commit, with its mistakes and the debug-build time of the own suite before and after.

## Not confirmed

- Whether to lower the cost of step 3 in debug builds: it added about 8% to the dev suite (161.1 s without the
  checks, 174.1 s with them), where steps 1 and 2 were within the run-to-run spread.
- Whether CLI.md lists the build option. It is not a run-time option and does not change a correct run; the
  maintainer decides.
