# P4 performance and P4–P7 roadmap proposal

[日本語](p4-performance-proposal.ja.md) · This English text is authoritative.

**Status: main roadmap and specification amendment approved 2026-11-27.**
Review base `b1ad0cb9`. F5 and the F6 overlay remain separate proposals.
[Stage A measurements](p4-performance-profile.md) · [Main review diff](p4-performance-review.patch) · [Separate F6 review diff](p4-local-slots-review.patch)

## What the measurements support

- **Ibex.** The 2-state native median was 10.625 s for 433,198 cycles against
  0.1097 s for one-thread Verilator 5.054. On this host it used 503,199 retired
  instructions per simulated cycle. All 130,473,903 island member evaluations
  reached generated C++. Samples under island evaluation or identifiable NBA
  application accounted for 63.5% of scheduler-thread samples. Writes and
  notifications outside attributed NBA application had 25.7% self samples,
  island scheduling and wait handling 32.7%, and generated C++ 16.8%.
  The interpreter outside islands had 2.6% self samples. Start with the
  event path. Treat NBA changes as a separate experiment.
- **CVA6 synthesis view.** The 2-state result was 344.794 s against 0.8635 s
  (0.250%). Only 4.16% of island member evaluations reached generated C++.
  Interpreter self samples were 59.2% across four windows. The regfile and
  scoreboard `process-frame` rejections and instruction-store rejection
  merit a separate eligibility investigation. Fix generally supported forms
  and exercise their mechanism matrices. Require a separate specification
  proposal before changing the existing whole-island fallback rule.
- **Daily workloads.** At `+n=200`, 14 of 28 inputs made zero generated calls.
  `comb_chain` retained 32 VM instructions per increment and
  `hierarchy_fanout` retained 1,811. `nba_shift` issued 81 NBAs per cycle and
  spent 44% of sampled CPU on NBA registration and identifiable application.
  The fixed 25-workload 0.3.0 comparison of 1.17× ran at load averages 97–108.
  Repeat it at low load before setting an absolute gate. Compiling waitful
  bodies and modifying the scheduler are independent experiments.
  [Source measurements and limits](p4-performance-profile.md)

Stage A reported the same firmware CRCs and final cycle counts in each
full CoreMark comparison. The current Ibex probe inputs match all 37 RTL
files at OpenROAD `404977691e84`. The earlier Ibex text names
`a9adcf481395`, which supplied the CVA6 input. The review diff includes that
factual correction. The published P3 ratios retain their own measurement
conditions. [Input provenance](../measurements/2026-11-27/p4-coremark-profile.json)

## Roadmap and candidate speed gates

1. **P4 pillar 1, suspending calls and a first set of waitful bodies.**
   Ship the existing 1a–1c inter-tier frame acceptance cases first. Add
   waitful `initial` and ordinary `always` with supported integral values,
   fixed delays or fixed scalar event waits, explicit SIR continuations
   and no unsupported effects. The kernel owns suspension and wake order.
   Preserve locals, step counts, wait arguments and enable cancellation
   through the existing frame tree. Ship one delay-loop and one event-wait
   workload with measured generated resumes. Confirm actual C++ resumes,
   a reduction in targeted VM instructions and warm runtime on `proc_wait`
   and the timed loop in `comb_chain`. Use `fork_join` and `func_call` as controls.
   Branch cancellation and generated nested calls expand only after their
   own acceptance cases pass.
2. **P4 pillar 2, observable intermediate transitions.** Keep 2a–2b. Make
   the observation rules explicit for static and runtime registration,
   guards, force and VPI. Check the old value, each intermediate value
   and the observer's region, including a same-slot observer installed
   after startup.
3. **P4 pillar 3, fused static-island evaluation.** Implement F1–F2 for
   a proved acyclic subset, then evaluate F3 independently. Feedback and
   uncertain observations use the existing path. Set an RTL throughput
   gate in P4. This establishes the observation, write and NBA contracts
   before P5 SVA sampling, coverage, VPI and DPI use them. Continue tracking
   RTL through P5–P6. Focus P7 on testbench and UVM costs and final tuning.

The table records the choices considered for the amendment. The roadmap
adopts the recommended column. Divide Verilator's elapsed time by kirimasim's
elapsed time. Use a pinned, uninstrumented, one-thread Verilator P3-settings
reference for the gate and keep the official speed-settings run as a
separate sensitivity check.

| Candidate metric | Starting point | Conservative | Recommended | Stretch or later |
| --- | --- | --- | --- | --- |
| Ibex 2-state native / Verilator speed | 1.033% | 1.20% (1.16× baseline speed) | **1.40%** (1.36×) | 1.60% (1.55×) |
| Ibex 4-state / 2-state native speed, `T2/T4` | 0.991× | ≥0.85× | Report the ratio and gate 4-state runtime at ≤1.05× its paired baseline | ≥0.95× if desired |
| CVA6 2-state synthesis / Verilator speed | 0.250% | 0.25% as a same-host floor | Track **0.30%** (1.20×) | 0.40% (1.60×) after broader eligibility |
| Fixed 25-workload native / VM geometric mean | 1.17× at high load | At least the new low-load baseline | Track **1.05×** the new low-load baseline | 1.10× the low-load baseline |

Adopt one primary speed gate for Ibex and the independent 4-state
non-regression check. Track CVA6 and daily during P4 until the input
preparation path, reference pin and repeatability are accepted. Always report
native and VM absolute times for the daily cases. A slower VM does not count
as a native improvement. Treat a repeatable per-case slowdown over 10% as a
triage item even if the geometric mean rises. Keep 4-state comparisons in a
fixed 4-state mode. Thus a 2-state gain alone cannot hide a 4-state slowdown.

### P4 acceptance matrix

| Axis | Cases to cover |
| --- | --- |
| Wait | Positive delay (including a time advance), `#0`, scalar level/edge event, false/true `iff`, multiple wakes, and a diagnostic fallback for an unsupported wait |
| Calls and concurrency | VM→VM, VM→native, native→VM and native→native where eligible, top-level and nested depth ≥2, plain and forked branches, same-time and reverse-order resume, `disable` and `disable fork` |
| Observer | `@` including synchronous `iff`, `$monitor`, waveform including glitches, VPI read/write/callback, procedural force/release, clocking and module paths |
| Location and lifetime | Input, internal node, output, overlapping bit-range and alias, startup presence, attach/detach before an island, attach/reentry during a boundary callback |
| Fusion and NBA | Sparse/branch fanout, temporarily equal outputs, repeated writes or NBAs, partial overlaps and X/Z, external NBA interleaving on the same edge, delayed/Re-NBA fallback, time zero, SCC convergence/limit and interruption at a defined checkpoint |

Generate cross-products where the mechanisms interact, with small single-axis
controls for the other combinations. Cover the existing five acceptance
cases and add **3a** for fused/unfused sparse chain and branch parity,
**3b** for runtime observation and dynamic fallback, and **3c** for contiguous
NBA grouping. Each case checks final values plus ordered effects and original
logical counters. Eligible cases prove execution via the opt-in report or
the shadow check. Ineligible controls prove each claimed fallback. Compare
native, VM with islands and VM without islands under the existing
`TEST_SUITE` contracts. Run 2- and 4-state tests within their own modes.

### P5 milestones and VM work

| Order | Scope | Completion evidence and measurement |
| --- | --- | --- |
| P5.1 | Pin uvm-core with the reviewed license and elaborate it. Run a minimal directed test using build/connect/run phases, objection and a FIFO or analysis port. | Check phase order and one stable transaction trace. Record elaboration, peak RSS and time per transaction on a fixed seed. |
| P5.2 | DPI-C imports, exports and value lifetimes, followed by tasks that can suspend. | Round-trip scalar and array/X/Z values, callback order, teardown and cancellation. Record calls/s and enabled/disabled overhead. |
| P5.3 | Constrained randomization. Decide solver choice separately. | Check `rand_mode`, constraints, seeds, repeatability and failure paths against the approved oracle. Record solves/s, time and RSS. |
| P5.4 | Concurrent SVA and sampled-value functions. | Check region sampling, disruption by reset, pending attempts, and the covered CVA6 constructs. Inventory the 59 prior diagnostics by mechanism. Record the count cleared and runtime with assertions off/on. |
| P5.5 | Coverage. | Check bins, crossing, sampling and reporting with fixed stimulus. Record sample throughput and RSS with coverage off/on. |
| P5.6 | Remaining VPI. | Check callback regions, handles, dynamic force and observer invalidation against the fused boundary. Measure callbacks/s and effect on ordinary RTL. |
| P5 exit | Existing human specification review. | Preserve the 145-item review and record its outcome separately from performance. |

After P5.1, consider moving the specific blocking SVA subset before DPI or
randomization if the CVA6 diagnostics classify into a self-contained set and
the observation boundary is ready. Keep the existing assertion expectations.
P6 keeps the switch-level, embedding and parallel-execution scope. P7 uses
real UVM profiles and saved test inputs for final tuning. Keep RTL performance
budgets active from P4.

Prototype the architecture's register-based bytecode on a small SIR subset
before P5 and keep the general rollout after P5 feature completion by default.
The timed integer-loop control used about 130 host instructions per SIR
instruction, while Ibex has only 54.82 SIR instructions per cycle. Measure
bytecode warm time, load/build time, RSS and parity on the directed UVM
workload, `func_call`, `fork_join`, `proc_wait` and a normal SIR load. Reopen
the rollout decision at P5.1 if UVM's interpreter self share exceeds 25% and
the prototype repeatedly improves end-to-end time by at least 10% at equal
semantics and within the agreed startup budget. These two thresholds are
proposal criteria. [VM architecture](../ARCHITECTURE.md#three-execution-tiers-one-kernel)

## F1–F8 design and specification deltas

### F1. Fused combinational evaluation

The kernel selects the same ready batch and retains control of time-zero
initialization, region changes, clock edges, interruption and cancellation.
Start with acyclic, fixed-range integral members, single-writer storage,
deterministic levels and no waits inside a body. Give a region one generated
function with a bitset of dirty members. Seed the set with the actual queued
members with live wait generations and visit in the current level/SIR-ID
order. Admit only a fully armed batch whose propagation would stay in the
same Active queue entry. A queued external event or a phase split can
prevent that. Compare each visited member's
canonical value against the slot and propagate dirty bits to exactly the
sensitivities the write changes. Preserve partial ranges and alias proof.
Never collapse separate batches or change an evaluation count. If a consumer
would need an already-passed level, or ordering is uncertain, use the
conventional path from the next safe boundary. Reject that possibility
before the first direct write in the initial implementation.

Use F2 at visibility boundaries. Leave all slots and logical wait generations
in place. Start with simpler whole-slot writes and ordinary isolated output
notifications, then add guarded/partial cases only with a proof. SCCs continue
to execute in the current path with the same convergence and `--max-scc-iter`
rules. Keep `bindCombGroup` as a measured baseline and fallback. Use only
one fast path for a batch. Consider retiring the old path after parity and
performance are confirmed on both modes and representative workloads.

### F2. Observation contract

The runtime owns slot access metadata and its generation. Track range
overlaps, physical aliases and changes to observer or force state. Include
subscriptions leaving the region, expression/edge guards, monitors, waveform
glitches, VPI, procedural overrides, clocking, module paths and registered
preservation reasons. An observed write takes the existing notification path
at its original position while the prior value is still available. Publish
all earlier direct writes before a synchronous observer is evaluated. During
an admitted direct store, no side-effecting callback can interrupt the access.
The kernel retains buffer lifetime and the original visibility points.

In the first release, a live synchronous guard, dynamic observer mutation,
VPI, race exploration, or a runtime ownership doubt selects conventional
execution **before** the first direct write. Ordinary externally observed
edges may use boundary writes only when the proof covers their order. A
later reentrant case must drop all borrowed views before the callback,
check the observation generation again, and resume on the original path from
the exact next operation. Never replay visible effects. Preserved names
remain discoverable. Use 2a/2b for observer and attachment cases and a
verification-only twin for `--tier-check`.

### F3. NBA runs at the original positions

Capture RHS, destination and index now. Keep one ordered record per logical
assignment in pre-reserved, Context-owned scratch for that island, including
multiple writes to the same bit. Leave active slot values unchanged until
the NBA region. Represent a contiguous sequence in the queue by a single entry
that has the original region and sequence interval. Test that no other
producer, captured NBA, different region or delay interposes. Honor the
current split of `wakeIslandMember` batches and the wait order of remaining
same-clock processes. Flush the accepted contiguous prefix before giving
control to an observer or other process. A refusal to reserve or prove a
run uses ordinary `schedule_nba` from the first unissued record.

At NBA application, publish each effect at its logical position, including
per-write notification and safepoints. During reentry expose the unconsumed
suffix before invoking a callback, the rule already used in the queue.
Reject delayed, event-captured, cross-region, Re-NBA or ambiguous overlapping
external sequences initially. A local record vector may carry X/Z and
selections. It cannot replace the sequence of writes by the final value.

### F4 and F5. Scalar temporaries and storage

- **F4.** In explicitly approximate two-state mode or a verified known-input
  path, emit width-masked scalar SSA locals using defined C++ operations.
  Use typed parameters across any needed function split. Keep signedness,
  truncation, integer safety and floating-point flags. First reduce C table
  lookups without changing checkpoint positions. Measure removing redundant
  checkpoints separately. Retain all branch/operation limits, pending end
  checks, SIGINT and resource/diagnostic checkpoints at their existing
  semantic positions. A failing proof uses the existing generated body
  shape. Verify the four-state guard against X/Z and synchronous observers.
- **F5.** Consider a separate one-plane runtime layout for `--state=2`
  after F4. Keep physical storage layout distinct from SIR types and from
  the stored format-1 design. Use versioned descriptors and invalidation at
  the ABI so old A/B modules cannot access the new layout. A waveform or
  VPI view reconstructs the canonical approximation with the b plane clear.
  Use the established layout where a service requires its representation
  until that service is tested. Compare waveforms, VPI, saved SIR and
  independence of Contexts. Make the rollout a separate approval.

### F6–F8. Local values, skipped blocks and other costs

- **F6.** After F1–F3, consider localizing only an intermediate whose every
  access is proved within the region and whose observers and retain reasons
  can be excluded. Preserve its slot ID and materialization at all
  observation boundaries, including deoptimization, exits, errors and
  final inspections. `--public`, `--preserve`, Waveform, Foreign, Force and
  Explicit each bar localization unless a later approval defines a
  materialization contract. Run the logical-write checker and compile
  an ordinary fallback for any unknown or dynamic observation. This would
  amend ISL-A9. [Separate F6 review diff](p4-local-slots-review.patch)
- **F7.** Coarsen contiguous topological ranges so an empty slice of the
  dirty bitset skips the range. Within an active range, retain member-level
  decisions. Split on observations, clock/region boundaries, SCCs and
  proof limits. Size from fanout, likely co-activation and generated-code
  budget. Test sparse and dense stimuli before deciding when the saved
  tests outweigh partitioning and instruction-cache cost. The acyclic
  partitioning idea comes from [Beamer and Donofrio, DAC 2020](https://scottbeamer.net/pubs/beamer-dac2020.pdf).
  The trade-off between activation tests and extra work is described by
  [Chen et al., GSIM](https://arxiv.org/abs/2508.02236).
- **F8.** Measure contiguous allocation of compatible island slots, one
  physical subscription for eligible same-clock members with identical
  scope, and reduction of empty-region main-loop checks in separate
  experiments. Each subscription still introduces the exact original
  logical ordered arms and preserves side-trigger interleaving and resource
  checks. Keep time-slot transitions, all regions and interrupt/drain
  checkpoints. Require a complete alias/layout proof before reordering
  physical storage. Use an independent data layout ID. The hardware-counter
  and layout motivation is described by [Beamer, IEEE Micro 2020](https://www.scottbeamer.net/pubs/beamer-imicro2020.pdf).
  The discussion of ordered register arcs informed the split between pure
  evaluation and commit, with kirimasim's event regions retained.
  [Erhart et al., Arcilator 2023](https://llvm.org/devmtg/2023-10/slides/techtalks/Erhart-Arcilator-FastAndCycleAccurateHardwareSimulationInCIRCT.pdf)

## Common runtime, checking and cost contracts

| Item | Specification changes and witness | Cost terms and validation |
| --- | --- | --- |
| F1 | CODEGEN_SPEC Static islands and ISL-C5, RUNTIME_ABI 4.2, SCHEDULER 4.3 and 9.5, TEST_SUITE 3a | Members × dispatch/heap work and changed ranges × in-island notification work. Compare the existing `bindCombGroup` and exact generated member sequence. |
| F2 | ISL-B5, RUNTIME_ABI 4.2, SCHEDULER 4.3, 9.5, 11, CLI observer reporting, TEST_SUITE 2a/2b/3b | Writes × saved checks minus observer-generation validation, boundary calls and borrow invalidation. Measure observer-off, static and dynamic cases. |
| F3 | ISL-B6, RUNTIME_ABI 4.2, SCHEDULER 4.3, 5, 11, 13, PERFORMANCE 3 and TEST_SUITE 3c | NBAs × issue/apply cost minus ordered-run preparation and kernel publication. Report issue and apply separately on `nba_shift`, Ibex and mixed external NBAs. |
| F4 | CODEGEN_SPEC Two-version switching, RUNTIME_ABI 4.4 and SCHEDULER 16.2 | Executed operations and checkpoints × changed cost. Compare 2-state, known 4-state, and unknown/fallback, including overflow, X/Z and stops. |
| F5 | RUNTIME_ABI 2 and 4.2, CODEGEN_SPEC storage and CLI `--state`/`--report-state-opt`, TEST_SUITE | Accesses × changed storage/conversion cost plus RSS. The measured 2-state/4-state instruction difference is 1.2%, and does not measure the effect of this layout. |
| F6 | Separate ISL-A9 amendment, CODEGEN_SPEC slots, RUNTIME_ABI visibility, CLI preservation, PERFORMANCE 3 | Proven private writes × saved memory work minus materialization and logical counts. Count only writes actually localized and verify each logical value. |
| F7 | CODEGEN_SPEC graph and level order, ISL-C5, PERFORMANCE 3, TEST_SUITE sparse/dense comparison | Empty ranges × eliminated scan work minus block checks and larger code. No change to the members visited. |
| F8 | RUNTIME_ABI 2 and 4.2, SCHEDULER 4.3, 9.5 and 16.2, PERFORMANCE 3, TEST_SUITE | Slot accesses, shared trigger arms and time slots × marginal costs. Measure locality, ordering and fixed loop changes separately. |

**ABI and cache.** Add an exact versioned service for borrowed slot views,
observation generation, ordered NBA publication and region-entry metadata as
needed. Keep the 0.5 host table prefix. A new service ID or version leaves
existing services readable. Change ABI major only for an incompatible
table/descriptor or semantic change under RUNTIME_ABI 2. Validate the layout,
capability and ownership before calling a generated entry. The cache keys
include canonical SIR, deterministic analysis/options and emitter digest,
target, compiler/options, public headers, service versions, layout and
state/check mode. Old or incomplete keys use the ordinary path or rebuild.
Record emission/build time, binary size, cold start, warm-cache startup
and run-only time separately. Preserve same-SIR byte-for-byte generation
given the same options. No build metadata or runtime observer pointers in
emitted deterministic content.

**Tier checks and counters.** Normal fused execution keeps existing
PerfCount logical *granularity*, including full logical writes and changed
values even when the intermediate storage operation is bypassed. Keep the
existing meanings for `slot.writes`, `slot.changes`, `nba.scheduled`,
`wait.subscriptions`, `proc.wakeups`, `island.member_evals`,
`sched.events.*` and limits. Batch arithmetic updates only over a proved
sequence with no observer, break, error or interrupt checkpoint in between.
Update a prefix before every fallback/exit. Measure the cost of counting,
including matched probes with extra diagnostics outside timing. Propose
`--report-fusion=<file>` for physical dispatch, heap, observer slow-path,
direct-store and NBA-run counts in a separate versioned report. Keep
`--stats=json` and its expected values stable. Native call counters describe
actual calls and need an unambiguous fused-body versus logical-member field.
Record the mapping explicitly. In verification, a generated shadow
evaluation and VM island share the same starting state and compare each
member visit and its ordered blocking writes, value transitions, wakes,
wait re-arms, NBA effects and stop state. Compare observed values at every
scheduler boundary. The kernel publishes effects once. Use isolated trace
capture and no external side effects in the shadow. Any unsupported case
uses the existing comparison path. These rules address PERFORMANCE §6's
open counter question.

**P6.** A fused region can become one parallel task only after certifying
disjoint slot writers, read dependencies, side effects and runtime observer
state. Its outputs and ordered NBA interval need a deterministically ordered
kernel commit between dependent work. Parallel execution has a separately
reviewed scheduler contract and uses Context-owned scratch and wait state.
Test independence across two Contexts in P4.

**Estimates.** Estimate saved host instructions per cycle as the sum of
eligible event counts multiplied by *measured* net marginal instruction
costs, minus region checks, trace-disabled bookkeeping, publication and
cache overhead. Derive marginal costs by paired windows with one mechanism
enabled at a time, identical effects and CRC/cycles, dividing retired
instruction deltas by the number of operations that actually changed.
Stage A did not measure those marginal instruction costs. Ibex has about
301 member evaluations, 984 writes, 42 NBAs and 1,158 logical wait
subscriptions per cycle. These counts overlap by path. At the recommended
1.40% gate the elapsed budget is about 7.84 s, down 26% from 10.625 s.
Under equal instruction throughput that would require about **132,000**
fewer host instructions per cycle. With every one of 301 members eligible,
the same budget requires about **440** saved instructions per member.
If the 63.5% sampled union were representative of time, halving it gives
1.47× overall and eliminating it gives 2.74×. These are scenarios.
Do not add the separate sample percentages as independent savings or
apply isolated C++ prototype ratios to CoreMark.

The following sensitivities use hypothetical instruction costs. Measure
eligibility and marginal costs before using them to predict a speedup.

| Change | Stage A denominator and sensitivity |
| --- | --- |
| F1 | At 301 logical member evaluations/cycle, saving 100 instructions at every evaluation would save about 30,100/cycle. Measure eligible dispatch and heap operations first. |
| F2 | If one quarter of 984 logical writes/cycle were eligible and saved 100 instructions each, the saving would be about 24,600/cycle. The count includes writes outside fused regions, so audit the eligible fraction. |
| F3 | Saving 100 instructions on each of 42 NBAs in Ibex would save about 4,200/cycle. The same sensitivity in `nba_shift` is 8,100 per clock. Measure registration and apply independently. |
| F4 | Halving generated-body self time gives about 1.09× end-to-end in Ibex's sampled time model (16.8% share). Scalar code and checkpoint costs were not isolated. Inspect and time each change separately. |
| F5 | For a converted two-plane payload, one plane halves those payload bytes. The share of total memory and the runtime effect are unconfirmed. Use identical SIR, I/O and ABI views with paired instruction, cache and RSS measurements. |
| F6 | Use per-slot eligible write counts and the marginal store cost after F1–F3. The counter-copy control gives a local ceiling. It includes compiler-eliminated copies and does not isolate this term. |
| F7 | Use the number of empty coarse ranges skipped and the number of member tests they avoided. Neither was measured. Collect a dirty-density histogram and compare matched sparse and dense inputs. |
| F8 | There are about 1,158 logical subscriptions and two time slots per Ibex cycle. Saving 100 instructions at each time-slot advance would save about 200/cycle. Measure eligible same-clock subscriptions and placement costs separately. |

## Measurement and implementation order

1. Repeat fixed 25 at low load with the same inputs and state modes.
   Proposed quiet-host rule is 1-minute load ≤ 50% of logical CPUs at the
   first and last sample of a series (9.0 on the 18-core host), no concurrent
   benchmark/CI on the same host, and report all initial and final loads.
   If violated, label the run exploratory and repeat. Keep five or more
   alternating warm runs per condition, plus cold and warm-cache startup.
   For short daily inputs use the difference between the medians of T(N)
   and T(1), and repeat near-ties. Report CRCs, versions, compiler flags and
   exact input hashes.
2. Schedule weekly Ibex Linux x86-64 baseline measurements after the public
   pinned replay is approved. Check native hardware counters on that host
   with `perf stat` (record event, privilege, multiplexing and coverage).
   Start as reporting, then consider an instruction-per-simulated-cycle
   regression budget on a fixed CPU/OS/compiler/commit and baseline window.
   Keep CPU instruction counts separate from deterministic PerfCount.
   Add a Verilator 5.054 column for compatible cases of the pinned daily
   manifest. Mark X/Z and unsupported cases explicitly and compute any
   aggregate only over a fixed common set. [Current reference options](https://verilator.org/guide/latest/simulating.html#benchmarking-optimization)
3. Implement F1–F2 and the normal-entry waitful controls first after
   approval. Assess F3 separately on NBA-heavy workloads. Investigate the
   CVA6 rejecting forms in parallel as a scoped problem. F4 follows
   selective measurements. Reorder the optimization backlog so compiler
   tuning, hot/cold separation and PGO follow structural work. Deploy
   F5, F6, F7 and F8 only after their evidence and scope review. Current
   CODEGEN_SPEC's blank two-version section receives the existing behavior
   description in the main review diff. [Stage A evidence](p4-performance-profile.md)

## Approved scope and separate decisions

The main amendment adopts the three P4 pillars, the recommended speed gate
and tracking metrics, the quiet-host protocol, the logical counter contract
and a separate opt-in fusion report. It adopts the P5 milestones, including
review of an earlier CVA6 SVA subset, and a bounded VM bytecode pilot before P5.
The [roadmap](p3-p7-roadmap-revision.md) records the approved scope.

Separate decisions remain for F5 physical two-state storage, the F6
ISL-A9 overlay, a host-instruction regression threshold, new profiling
tools or runners, external source and adapter preparation, and solver
selection. The current replay uses prepared artifacts.
