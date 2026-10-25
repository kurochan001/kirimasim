# Declared strength networks through SIR and the VM

[日本語](strength-network-execution.ja.md). Status: implemented execution
checkpoint, 2026-10-26. Source MOS/switch support remains work.

## Grounds and scope

`cea83470` implements selected metadata reads and shared process wait lifetime.
`faadf4b7` connects normalized network declarations and three more SIR operations
to the real kernel. The grounds are the approved [MOS/switch contract](mos-switch-proposal.md)
1/6/7 and its [normative mapping](p1-execution-contracts-2026-10-25.md),
SIR_SPEC 3/5/7/10, VALUE_SEMANTICS 3.11/5, SCHEDULER_SPEC 9/12 and
IEEE 1800-2023 4.9.5, 28.7 and 28.16, summarized here in our own words.
Implementation and tests are original; no external simulator implementation or
tests were read. No dependency, C ABI, performance budget or event collection
timing changes. Tier 1 still calls kernel C++ APIs directly.

The existing [physical-origin bank](strength-driver-execution.md) now has a
verified SIR producer/consumer path, rather than only direct C++ test callers.
This does not enable the SystemVerilog front end's MOS lowering. The bank still
resolves independent nodes: passive edges, world correlation and graph discovery
are not implemented by this connection.

## Static declarations and ownership

SIR adds these plain declarations, independent of the kernel's C++ types:

- A `StrengthNode` row names one normalized physical slot/element/bit, its
  effective core tie kind, component ID, initial published complete range and
  source position. Its target explicitly has one element and one bit, including
  for scalar slots; ordinary Slot operands retain their previous scalar syntax.
- A `StrengthOrigin` row names a node, its initial complete contribution, its
  kind and source position. A process origin belongs to one static continuous
  process. Pull and supply origins have no process owner and fixed signed
  points of magnitude five and seven. IDs are typed table ordinals, not names.
- A process declares `strengthWrites` separately from ordinary net writes, and
  `strengthTriggers` separately from ordinary value/edge triggers. The existing
  `kNeedsStrength` flag identifies a registered physical net. Alias reads use
  its earlier physical slot and their own declared array bounds.

The verifier checks positive four-state net eligibility, complete bit/element
coverage, distinct normalized targets, dense component IDs, legal core kinds,
valid endpoints and source file/line/column positions. It checks origin owners,
implicit points, uwire physical-root counts, unique declared writes, and exact
instruction ownership/operands. A node's effective kind never silently creates
another pull/supply root.

Registered network nets have one publisher. Ordinary writes, mutable references,
mutation-oriented system calls and mutable call actuals cannot bypass it.
Read-only value loads, event controls, input call arguments and the explicitly
supported diagnostic/waveform calls remain readable. This access boundary still
needs source-level integration tests for all front-end operand forms; it is not
a claim that every SystemVerilog call form on a registered net is connected.
Force-marked physical nets and force through their aliases are rejected until
the registered-net hold bridge exists. Existing source force stays in its
previous unregistered path.

Hand-built network SIR receives full verification before the VM constructs
Store slots. Invalid metadata/ownership therefore cannot partly install a
network or expose a writable reference. The normal driver also verifies SIR;
this extra check protects direct VM adapters. Ordinary designs without network
metadata keep the existing execution path.

## Instructions and captured selectors

`strength.load` produces an owned `strength<w>` from a statically identified
registered net window. An array Slot operand with `numElems=1` selects a fixed
declaration-order ordinal. With `numElems=0`, it takes one captured integral
index per declared dimension. An optional final captured position selects within
the static parent bit window. A scalar needs no element indices. Without a
position, result width equals the window width; with a position, missing bits
may lie outside either side of it.

`strengthLoadShape` is the common static-shape decoder for verification and VM
loading. The VM reads each captured selector through its existing whole-width
index/element decoder, shared with ordinary accesses. Kernel
`readSelectedPacked` supplies the meaning of the resulting selection: unknown,
unrepresentable or out-of-bounds elements/positions read strong X, while a
partially outside bit selection retains in-range complete intervals and fills
only missing bits with `[-6,6]`. It never redirects an invalid selection to node
zero. Successful output clears packed tails and preserves surplus words;
invalid static shapes do not touch output.

`strength.drive !dN, <strength<1>>` updates the declared physical output through
the kernel's direct-drive API. `strength.drive.delay` adds captured normalized
rise/fall/off ticks and uses the same scalar inertial API as direct kernel
callers. Each output has its own pending generation and copied range. It
retains identical pending deadlines, cancels different results, and reports
unrepresentable delays through the ordinary positioned run-time diagnostic.
The issuing user process/frame is not retained by the timer.

Strength remains non-language SSA metadata. It is not allowed in slots, frame
variables, constants, ref targets, parameters or return types. Existing dominance,
parallel block transfers and no-SSA-across-wait rules continue to apply. Loads
also work inside nested functions that consume metadata locally and return an
ordinary projected value; this does not make language function returns carry
net strength.

## Loading, wakeups and cleanup

Before any process executes, the VM constructs published Store values and
complete intervals from the declared initial node state. This is initial storage
construction, not an observable write or an early resolution. It registers the
copied node/origin tables and queues the initial Active solve before device
processes. Static initializers see the declared initial state; subsequent Active
code sees the scheduled result. The loader does not derive initial published
values by resolving origin contributions. Front lowering must still construct
ordinary initial X/side masks and new-device Z under the approved rules.

`waitEventStrength` combines ordinary value subscriptions and internal complete-
range subscriptions into one kernel process wait. Numeric process IDs and wait
generations are kept by Context, not interpreter frame pointers. Any matching
alternative wakes once and cancels every other alternative through the existing
wait cleanup. Delay/named waits, cancellation, kill, process completion/reuse and
VM teardown use that same cleanup. Staged allocation failure keeps the previous
wait and requests resource termination; malformed strength ranges leave it intact.
Watch rows reuse the existing generation/serial registry and creation cutoff.

The VM transports the declarations and operands, and checks boundary errors.
All range projection/selection, resolution, pending-state equality, scheduling
and cancellation remain kernel operations. No new user process or hidden fork
child represents timer/dirty-component work. These POD values add no GC root
category, and the unapproved event instruction-safepoint amendment is unchanged.

## Text and validation

Format version stays zero. The writer emits deterministic `strength-node !nN`
and `strength-origin !dN` rows, process sensitivity/ownership lists, and the three
new instructions only when present. Existing dump expectations are unchanged.
No text parser or print/parse roundtrip is introduced; format-one grammar and
roundtrips remain part of the planned SIR-v1 phase.

Nineteen original tests cover:

- Seven kernel read/wait tests: 230,400 selected reads and 11,750,400 interval
  checks across six parent widths, all 120 intervals, three array elements,
  both parent windows, four result widths and valid/invalid positions. They
  check surplus/tail storage, ordinary/strength alternative cancellation,
  both process groups, wait replacement/kill/reuse, 512 one-row reuses and
  sixteen allocation budgets preserving the previous wait.
- Twelve SIR/VM tests: 5,760 complete interval/device/control designs against
  independent `valsem`, 7,680 array/alias/selector designs at widths up to 129,
  eight nested-function designs, twelve normal delayed designs and one expected
  wide-delay error, immediate/delayed strength-only reevaluation, startup before
  initial Active resolution, and process-free implicit roots. In total 13,465
  valid hand-built SIR executions include one expected run-time error.
- Twenty-seven malformed static cases, sixteen ownership/instruction cases,
  ten invalid-load/wait cases and four independent coverage/uniqueness/owner
  cases. Thirty-two malformed VM loads, including alias force, are rejected
  before Store construction. These are IR checks, not source feature counts.

One existing unit diagnostic assertion now includes `strength.load` in the
producer list; it still rejects the same invalid producer. Its existing
ordinary-consumer negative control is retargeted and compiled again. No suite
source, dump expected output or performance expectation changes.

All sixteen [compiled controls](../measurements/2026-10-26/strength-network-controls.json)
are caught after successful full builds, including the C++20 public-header target:
seven kernel selection/wait faults, four metadata/ownership faults, four VM
connection faults and the retargeted existing consumer control. Compilation
failure does not count as a detection. Baseline and restored production pass
125 cases, including all 29 unchanged dumps and PerfCount. All 719 registered
controls apply. The separate standalone public-header syntax probe passes as
C++20 with exceptions and RTTI disabled.

[Full local CI](../measurements/2026-10-26/strength-network-validation.json) passes
all 21 checks: dev/ASan/UBSan 5,206/5,208 and release 5,207/5,209, zero failures
and two known skips each. Static checks, generated counts and existing protected
oracles pass. Both implementation commits pass manual expected-change checks.
README counts use the fresh complete dev JUnit: 591 unit tests and 4,407 suite
sources. No source changed after final validation. These are local macOS arm64
results, not a new GitHub Actions or Linux run.

[Ordinary-path timing](../measurements/2026-10-26/strength-network-speed.json)
compares release `f93da11b` and `faadf4b7` in five alternating pairs at 200,000
iterations, after all build/CI work ended. Startup-subtracted frame-access
estimates are 0.255550791→0.256257333 seconds (+0.3%); legacy event estimates
are 0.047531292→0.047827875 seconds (+0.6%). Every counter is equal and median
peak RSS is 9.203125 MiB for both binaries and both workloads. These small-sample
ratios do not establish a stable regression. Neither workload uses network
metadata; the new execution traffic and source MOS remain unmeasured. Following
PERFORMANCE 4/5, PerfCount passes, wall time is reported rather than gated, and
no budget/threshold changes. This is not the seven-nightly-result baseline or
an external-simulator benchmark-table refresh.

## Remaining integration

Source MOS still needs front lowering, ordinary-driver/side import, structural
data provenance and separate captured control-expression propagation so a
strength-only data notice does not rerun an unrelated function or RNG call.
The static catalog here has nodes/origins, not the approved identity/passive-edge,
control-binding and graph connectivity catalog. The world-preserving passive
solver, conditional-edge timing, registered-net force/release, port collapse/
identity, `%v`, complete primitive-array source cases and source oracle migration
remain work. In particular, this bridge does not yet supply complete intervals
to `%v`, even for raw network IR. Do not use cached node readouts as physical
roots to fill those gaps.

Global allocator failure injection and 64-bit generation exhaustion are **not
confirmed**; use a dedicated allocator/near-exhaustion test facility to check
those paths. Source reference comparisons, Icarus-13 gaps, P1 population,
sv-tests, Linux and minimum compilers were not refreshed. The historical 21
passing-Icarus-13 probe gaps are not a complete feature inventory. This checkpoint
does not establish a source-level gap reduction or complete P1/Icarus coverage.

## Subsequent source checkpoint (2026-10-26)

[Directed MOS source execution](source-mos-execution.md) now connects six
kinds, owned ordinary-driver import and separate control capture. The scope
and validation above describe the earlier SIR-only checkpoint. Passive
solving, registered-net force, partial port identity and full formatting
remain incomplete.
