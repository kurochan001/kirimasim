# VM value access for shared automatic storage

[日本語](automatic-vm-frame-access.ja.md). Status: access-path migration,
2026-10-25; automatic environment construction is still in progress.

Subsequent checkpoint: the [lexical environment component](automatic-vm-environment.md)
constructs and directly verifies scoped owners and complete child environments.
Attaching it to `Interpreter` and constructing immutable scoped refs remain work.

## Contract and change

This implements the access transport required by sections 2–4/6–8 of the
approved [automatic capture contract](automatic-capture-proposal.md), mapped
to SIR_SPEC 7/8 and RUNTIME_ABI 4/6 in the
[approval record](p1-execution-contracts-2026-10-25.md). IEEE 1800-2023
6.21, 9.3.2 and 13.5.2 are the language references. No external implementation,
external test or new dependency is used. This does not select a physical
frame layout, formatVersion 1 or a new C ABI.

Previously `frameVar`, `frameElem` and `refWord` returned mutable native word
references. Scalar operations, wide operations, memory files, file input,
scanning, seed updates and call marshalling wrote those words directly.
Replacing just scalar assignments with shared bindings would leave the other
paths writing unrelated or unobserved words.

`detail::FrameValue` is a short-lived transport view of one value/element.
It can borrow ordinary frame words or use an immutable kernel activation
binding. It owns neither storage nor a lease and keeps no mutable value
cache. The bound variant reads through `ActivationStore::read` and writes
through `Scheduler::writeActivation`, with complete wide values and explicit
selected bit ranges. Kernel code still defines normalization, identity,
permission checks and observation. The ordinary path keeps word copies and
uses the existing kernel wide-selection primitives, with no allocation in
the view itself. Reference payloads retain both halves of their W4 encoding.

All existing instruction/file/scan/argument paths now read values or commit
values through the shared access interface. A clipped packed write commits
only the covered bits. `$fread` builds only the read-byte range and commits it
once; it does not read the rest of the destination to manufacture a write.
Whole arrays are transported element by element, preserving declaration order.
`bindArgs`, `readOuts` and `writeOuts` take the registered logical activation.
Both waiting and synchronous calls copy outputs back before retiring the
callee, exactly once. The task and deferred-call entry paths register before
copy-in. Registration alone does not yet construct an automatic root.

## Verification and limits

Seven original tests exercise 270 ordinary/bound selection cases against
an independent bit-by-bit model, full reference payloads, invalid buffers and
ranges, readonly access, selected read/write footprints, complete wide-value
notification in Active and Reactive groups, two-state normalization, and
reused-scope generation rejection. A hand-built SIR matrix runs 24 cases of
ordinary/ref array access, clipped packed writes and scalar copies, with
unrelated wide frame rows to check logical/physical row translation.
Five new compiled-control definitions target notifications, wide chunks,
element identity, readonly rights and selected reads. The two existing controls
for wait-ref lookup and `$fread` byte ordering are relocated to the same faults
in the new access paths; no expected output or existing suite body is changed.
The [nine compiled behavioral controls](../measurements/2026-10-25/vm-frame-access-negative-controls.json)
are detected by actual unit or source-test failures after successful builds.
An initial notification-omission fault stopped at an unused-member build warning;
that attempt is recorded and is not counted as runtime detection. Its corrected
form reaches and fails the notification test.

Removing the redundant word-vector pointer from `Act` also required relocating
the existing task-guard selection and first-override-sensitivity controls. The
former is detected by the existing ref-wait layout matrix. The latter survived
the existing assign/force/override cases: those checked later evaluations, whose
sensitivity was already registered. Add 36 new generated source cases across
module/generate/interface, variable force/net force/procedural assign, two/four-state
function values and direct/nested RHS calls. The net declaration remains four-state.
After the destination settles to zero, the override's first evaluation sets one,
which requires another evaluation. All 36 pass, and moving registration after
the first evaluation fails the actual new source test. Existing oracles stay intact.

The [separate source observations](../measurements/2026-10-25/override-initial-observations.json)
record all 36 native runs with `seen=0`, `seen=1`, `value=1`. Both measured reference
builds emit only `seen=0`, `value=1`; their compiler messages report one-time RHS
evaluation. The cached 13.0 build identifies itself as `v13_0-dirty`: the nature
of those build-tree modifications is **not confirmed**. Repeat with a confirmed
clean v13_0 build before using this observation for the pinned P1 criterion.
These probes are separate from the frozen 87-probe comparison and do not replace
its version or coverage evidence.

The [final validation record](../measurements/2026-10-25/vm-frame-access-final-validation.json)
combines all 21 local-CI checks at `9b30d6d2`. Dev/ASan/UBSan pass 4,629/4,631;
release passes 4,630/4,632, with zero failures and the same two skips. The first
dev/release run failed only because the generated README counts predated the
36 sources and one generator. Regenerating the counts and rerunning both
configurations passes; runtime source did not change between these checks.
The full first run's clang-format, clang-tidy and other static checks passed.
All 29 existing SIR dumps and PerfCount budgets remain unchanged and pass.

Fresh [native sv-tests](../measurements/2026-10-25/vm-frame-access-svtests.json)
retains all 344 judgments and 244 passes. An archive of its exact pinned public
revision is used, independent of another working tree. The
[exact archived P1 population](../measurements/2026-10-25/vm-frame-access-population.json)
retains 1,451/1,466 passes, zero failures and 15 unsupported forms. The
[87-probe rerun](../measurements/2026-10-25/icarus14-vm-frame-access-probes.json)
retains every native/14.0 status and passing judged output. The gap count is
22 against fresh 14.0 and the earlier recorded 13.0 counterpart; 14.0
still has seven oracle mismatches. The 13.0 counterpart and reference sv-tests
engines are not rerun for that inventory. Nonpassing output was absent from
the older portable records, so no equality of those bytes is asserted.

A [focused release measurement](../measurements/2026-10-25/vm-frame-access-speed.json)
uses an original automatic-function/wide-array/ref probe, with five alternating
paired samples on macOS arm64 and the same independent checksum. After
subtracting the n=1 startup median, the n=200,000 median changes from 0.2234 s
at `48b8c315` to 0.2377 s at `9b30d6d2` (about +6.4%); median peak RSS is
9.1875 MiB for both. Every reported counter is identical, including 9,400,020
VM instructions. This does not count internal value copies. As PERFORMANCE
1/3/4 requires, timing is reported, not used to pass/fail or alter count budgets.
This single-machine probe has no confidence interval, does not supply seven
nightly baselines, and does not remeasure the standard four speed benchmarks,
other simulators or Linux.

The VM's SIR-to-place resolver currently selects ordinary word views only.
The [new SIR homes](automatic-sir-metadata.md) remain explicitly rejected;
front emits none. The bound access variant is exercised directly by the new
tests, not by newly accepted source programs. This checkpoint closes no
Icarus inventory gap and does not establish P1 completion.

Next, resolve scoped/captured homes to their immutable bindings, construct
root/block/child environments transactionally, preserve full lexical extents,
and release executing owners independently of control frames. References need
their own immutable scoped origin and rights, rather than a pointer to a
parent's current binding table. Guard/watch dependencies must resolve against
the waiter's actual logical activation. Declaration initialization, parameter
binding initialization, copyback, return/disable/program cleanup and typed
roots must all work before removing the execution gate or enabling front
capture. Managed events and other field kinds still need their real providers.
