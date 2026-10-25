# Complete net-strength publication and checked storage counts

[日本語](strength-store-publication.ja.md). Status: implemented kernel checkpoint, checked,
2026-10-26. Source MOS/switch execution remains work.

## Grounds and scope

The approved [MOS/switch contract](mos-switch-proposal.md) 1/6/7 and its
[normative mapping](p1-execution-contracts-2026-10-25.md), VALUE_SEMANTICS 5,
SCHEDULER_SPEC 9/12, SIR_SPEC 3.2/4.1 and RUNTIME_ABI 4/5 are the grounds.
Implementation, tests and benchmark sources are original; no external simulator
implementation or tests were read. No dependency, external C ABI, format version,
performance budget or managed-event collection timing changes.

`49bd3069` adds `StrengthStorage` to each Context and connects it to the real
Scheduler and Store. `enableStrengths` copies published intervals for every bit
and array element. They must project to the Store's current values. Registration
does not resolve or write the net, schedule work, create a process or announce a
change, preserving initial X until actual scheduled resolution. This API stores
published net state; it does not import ordinary physical origins or their side
masks. `read` and `readPacked` load complete ranges by numeric slot/element/bit
identity, with the packed representation of [strength SSA](strength-ssa-execution.md).

`publishStrengths` accepts a complete component result:

- Validate target domains, ranges, exact input sizes and endpoints. Copy all
  caller buffers and target information. Sort by slot/element/bit, reject
  overlapping parts and prepare one before/after value snapshot per element.
- Stage ordinary notices and their owned buffers before changing published
  fields. Commit every interval and every projected value before any callback.
- Announce internal interval changes separately. A strength-only update can
  notify devices without ordinary value events, value-change counters or VCD
  transitions. Ordinary notices retain their existing edge and value rules.
- Preserve each distinct committed transition, including publications inside an
  outer notification walk. Delayed ordinary notices own their value snapshots;
  they do not borrow scratch buffers from a completed publication.

Callbacks see a coherent component. Internal callbacks queue dependent work;
synchronous nested publication returns `Reentrant`. Registration/cancellation
and registry growth during callbacks are supported. A creation cutoff excludes
new/recycled watches from the publication already underway. Canceled rows reuse
storage with 64-bit generations; exhausted generations retire rather than wrap.
A stop during notification suppresses later callbacks while retaining the complete
committed snapshot.

A resource failure returns whether the whole snapshot had committed. Staging
failures leave all published fields unchanged. A later callback allocation failure
can stop an already complete commit; its caller must not retry that publication.
The failure path restores walk state and drops undelivered publication notices.
No partially published component is reported as successful.

The kernel interfaces are C++20 and independent of SIR, slang and LLVM. Registered
slots reject ordinary Scheduler writes, and registration rejects existing holds;
marking a registered slot for legacy holds also fails. Force/release integration
is still missing. Direct mutation through `Store::element` is not a supported
writer for registered slots. The future SIR loader/verifier must enforce the
same designated-publisher ownership. No source path currently registers these
slots, so existing source force behavior remains in its previous path.

## Width rounding found during review

The old `(width + 63) / 64` and `(width + 7) / 8` formulas could wrap a 32-bit
width before division. At `UINT32_MAX`, the required counts are 67,108,864 words
and 536,870,912 bytes, rather than zero. The verifier could consequently accept
huge integral constants with missing words.

`99dafab4` repairs the mechanism across kernel, VM, front, verifier, formatting,
memory files and reference conversion. Kernel `wordsFor`/`bytesFor` and SIR
`integralWords`/`integralBytes` centralize counts within their independent libraries;
`wide::chunks` uses the kernel helper. SIR does not acquire a kernel dependency.
The reference uses independent remainder/wide arithmetic. Already promoted
64-bit arithmetic in activation/pattern storage remains unchanged; the separate
32-bit-word formatter uses `size_t` promotion on the tested 64-bit target.

Three new tests check ten word boundaries through kernel/wide/SIR, twelve byte
boundaries through kernel/SIR, minimal coverage using 64-bit arithmetic, and eight
malformed constants across both state domains and signedness. They allocate no
huge vectors. This proves count/rejection behavior, not successful simulation of
a design with billions of bits or a new practical width limit.

## Validation

Sixteen original publication tests cover all 120 intervals at each tested bit
and array element (88,200 checks across six widths), initial packed layouts,
coherent callbacks during slot/watch growth, cancellation/reuse, all sixteen
logical transitions with four edge modes, VCD with and without glitches,
invalid/overlapping batches, stops and Context isolation. Allocation tests try
32 failure positions for registration and 32 for publication preparation, and
check a callback failure after commitment. A watch row is reused 512 times.
The three arithmetic tests above bring this checkpoint's new tests to nineteen.

All sixteen [compiled controls](../measurements/2026-10-26/strength-store-controls.json)
are caught: ten publication faults, four unsafe-rounding faults and two
retargeted legacy wide force/default controls. Every full build succeeds,
including the C++20 probe, before actual tests fail. Compilation failures do not
count as detections. Baseline and restored production pass 376 cases, including
29 unchanged dumps and 328 wide/force source cases. All 691 registered controls
apply; the separate broad focused check passes 141 tests.

[Final local CI](../measurements/2026-10-26/strength-store-validation.json) passes
all 21 checks: dev/ASan/UBSan 5,171/5,173, release 5,172/5,174, zero failures
and two known skips each. PerfCount, old dumps, static checks and fresh generated
counts pass. Both implementation commits pass the manual expected-change check;
no existing oracle changed. The record also retains the earlier green CI at
`49bd3069`, before the additional width-boundary coverage. Compiled controls
confirm the old formulas fail that coverage. Green finite CI by itself is not
a proof of complete feature or boundary coverage.

[Focused cost evidence](../measurements/2026-10-26/strength-store-speed.json)
compares the retained `0ec3cc57` release binary with `99dafab4`, using five
alternating pairs of 200,000 iterations on one macOS arm64 system. After estimated
startup subtraction, ordinary frame access is 0.260349→0.258658 s (−0.6%) and
legacy named-event roundtrip 0.047821→0.046906 s (−1.9%). All counters match;
median peak RSS is 9.203125 MiB for both binaries/workloads. No additional
regression is visible in this small sample; these differences are not a proven
speedup or a wall-clock gate. The after binary includes both changes, and neither
workload registers strength metadata. Publication traffic/source MOS costs remain
unmeasured. This also does not erase the earlier named-event cost already included
in the baseline. No performance threshold or budget changes.

## Remaining integration and limits

This checkpoint publishes an already computed component result. It does not
supply physical driver/origin identity, implicit-root import, the static network
SIR catalog, `strength.load`/`strength.drive`, Active dirty-component scheduling,
per-scalar inertial updates or the passive world solver. Their source connection,
force/release, ports and strength-aware formatting remain required before claiming
MOS/switch execution. Initial device-Z versus ordinary-driver-X rules still need
the real driver/scheduler connection. No new IR is introduced.

Allocation tests cover the Context's supplied memory resource and a throwing
ordinary callback. Exhaustive failure injection for every global allocator site
is **not confirmed**; a dedicated allocator harness or further resource-aware
notification staging would check it. Packed initial reads, varying raw published
intervals and earlier independent packed-value tests are separate evidence;
complete SIR load/drive source tests remain work. The public headers compile as
C++20; minimum-compiler, Linux and 32-bit target runs were not performed here.

No fresh Icarus probe/population, sv-tests or external simulator speed comparison
is claimed. The 21 historical Icarus-13-passing gaps remain work and are not the
entire missing-language inventory. P1/full Icarus coverage remains incomplete.
The separate managed-event instruction-safepoint proposal remains unapproved.

## Subsequent physical-origin checkpoint

[Physical origin execution](strength-driver-execution.md), `f93da11b`, now
connects copied independent-node/origin tables, complete-range resolution,
per-scalar inertial generations and coalesced Active work to the component
publisher. Sixteen tests and thirteen compiled faults cover the new kernel path.
This supersedes the corresponding driver/timing items above; those checkpoint
measurements remain historical. SIR load/drive/static rows, source sensitivity,
passive graphs, registered-net force/release, port handling, formatting and source
MOS execution remain work. The bank does not turn published net values into
physical roots or implement the passive solver by directed readout feedback.
