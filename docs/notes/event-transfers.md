# Event ownership across argument and result transfers

[日本語](event-transfers.ja.md). Status: component checkpoint, 2026-10-26.
The current [published implementation](automatic-event-main-integration.md)
includes instruction collection and automatic scalar/fixed-array event defaults.
The component description and measurements below retain their original scope.

**Subsequent implementation (2026-10-26):** [instruction events](instruction-event-execution.md)
adds native-field publication, pre-growth collection and automatic scalar/fixed-array
source construction. Input pins now end after complete binding. The remaining-work,
retention-period and validation statements below describe the earlier component checkpoint.

## Contract and implementation

The grounds are RUNTIME_ABI 6's approved event-only amendment,
[automatic-capture 5/6/8](automatic-capture-proposal.md), SIR_SPEC 5.1/7/8 and
IEEE 1800-2023 6.8/13.5/15.5. Implementation, tests and controls are original;
no external simulator implementation or tests were read.

`1d2b7e6e` adds `EventTransfers`, an internal tier-1 owner backed by existing
kernel `ManagedEventPin` values. It retains each managed value in a typed
transfer snapshot before binding any formal or copying back any output. The
first managed value uses inline storage. Additional holders use the Context's
existing allocation source, with no new dependency or public interface.
Null and legacy permanent events need no registry pin.

The VM compiles event-parameter indices once from declared SIR types, excluding
reference parameters. Only selected event scalar/array words are retained;
integer, real, string and reference bits are not interpreted as event values.
Input and output snapshots remain independent of later variable replacement.
Failures undo the entire new batch while preserving any previously owned batch.
Malformed indices and stale values return errors; allocation failures return
the existing ResourceLimit result without leaking partially acquired pins.

Synchronous calls, `call.task` input binding, shared output copyback and moved-out
deferred arguments use that owner. A deferred call keeps its immutable source
instruction so transfer errors name the issuing source location. A scalar
event return receives a kernel pin before its SSA region is destroyed, and
keeps it through output callbacks until the caller publishes the result.
Input snapshots are conservatively retained through synchronous call completion;
this is not per-instruction liveness.

A completed waiting task now retires its enrolled activation even if output
snapshot/copyback fails, before its native word vector disappears. This prevents
an enrolled row from retaining a pointer to destroyed words. No instruction is
retried and no scheduling event is added. The container heap's collection timing,
C ABI 0.3, SIR formatVersion 0 and source acceptance remain unchanged.

## Verification and controls

Six component tests cover inline/null/legacy cases, typed arrays and opaque
aliases, buffer replacement, nested ownership and pending kernel pins, stale
values and malformed metadata, transactional spill failures at three allocation
points, and 1,024 exceptional entries with three reused physical rows.

Four new VM tests contain nine independently constructed verified SIR designs:
ordinary/scoped output plus event returns, waiting-task event-array copyback,
settled deferred execution after the issuer returns and its source is replaced,
and five deterministic allocation-failure paths. Those paths are synchronous,
waiting-task and deferred input, plus synchronous/waiting-task output copyback.
They stop with the positioned resource diagnostic, execute no callee/continuation
side effect after failure, preserve all caller output destinations and release
all pins. They do not force GC inside a partially published source transfer.
Event outputs in these tests verify SIR transport, not new source-language
permission to rebind initialized events.

[All 12 compiled faults](../measurements/2026-10-26/event-transfers-controls.json)
are caught: eight owner/transaction/type-selection faults and four real VM paths
that bypass the configured allocation source. Restored related tests pass.
These controls do not establish the still-disabled instruction-level collector.

[Validation](../measurements/2026-10-26/event-transfers-validation.json): full dev
CTest passes 5,551/5,553 with zero failures and two known skips. Every preset
builds fully, and dev/release/ASan/UBSan each pass 98 focused checks including
PerfCount. Changed C++/headers pass format/tidy and policy checks; all 779
mutation anchors apply. Existing fixtures, dump expectations and PerfCount
oracles are unchanged. Full local CI and full release/sanitizer CTest were not
run for this checkpoint; neither were new Linux, population, sv-tests or
external-probe measurements.

The [five-pair cost observation](../measurements/2026-10-26/event-transfers-speed.json)
uses the original ordinary frame/call and static named-event workloads. Median
large-minus-small estimates are 0.26326 → 0.26871 s (+2.1%) and
0.04868 → 0.05059 s (+3.9%). Both sides have 9.1875 MiB median large-run peak
RSS and identical counters. One macOS arm64 system and five alternating pairs
are an observation under PERFORMANCE 5, not confidence bounds or seven nightly
baselines; wall time is not a pass/fail gate and no budget changes.

## Remaining work

Active ordinary synchronous/guard fields still need complete publication,
including cancellation and pre/post-binding boundaries. SSA-region retirement
is conservative; precise instruction-point roots and collection before event-row
growth remain work. The full nonwaiting-loop, recursion, delayed-target,
cancellation, failure and source matrix must pass before enabling automatic
source construction or instruction collection. Class/ref/object providers and
the broader P1/Icarus coverage goal remain unfinished. No new source-probe gap
reduction or P1 completion is claimed by this component.
