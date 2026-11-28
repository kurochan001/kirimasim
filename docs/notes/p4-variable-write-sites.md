# P4 proposal for direct access at individual generated variable stores

[日本語](p4-variable-write-sites.ja.md) · Experiment order approved on 2026-11-28: measure fixed-range W in ordinary generated bodies first, then island 4 in the 00/W/S/WS configurations. Service 20 v1 and its fixed-store contract were approved with explicit prefix versioning and lifetime invalidation. [English and Japanese review diff](p4-variable-sites-review.patch) · [Implementation results, baseline replay and invalidation counts](p4-fixed-variable-w.md)

## How often a write actually changes the value

The prior exclusive write/notification estimates for island 4, island 0 and ordinary CombAt sum to 177,287 host instructions per simulated cycle, about 35% of Ibex. They include necessary work. Of the 77 ordinary CombAt rows, 76 generated bodies ran a total of 24,684,622 times. Process 823 was rejected for cyclic control flow and had zero VM entries on this input. [Earlier cost model](p4-ibex-island-plan.md) · [Counters, conditions and calculations](../measurements/2026-11-28/ibex-variable-sites.json)

| Ordinary variable store | Equal/cycle | Changed/cycle | Host instructions/equal call | Host instructions/changed call |
|---|---:|---:|---:|---:|
| Whole | 257.64 | 87.93 | 148.0 | 419.6 |
| Part select | 177.15 | 23.37 | 149.9 | 441.6 |
| Total | 434.80 | 111.30 | | |

All 236,569,230 ordinary ABI write calls targeted variables of at most 64 bits. Zero met the diagnostic veto conditions on this run. Dynamic partial ranges account for 145.14 equal and 15.35 changed attempts per cycle. Fixed partials account for 32.02 equal and 8.02 changed attempts. Whole and fixed partial stores together give **385.61 calls/cycle** eligible for the first version. Three processes also used the existing counted-loop service, which already handles 165.54 logical writes and 165.54 changes per cycle. Including that service gives **711.64 logical writes and 276.84 changes per cycle**. The savings below exclude the already grouped updates.

**Ordinary change-only subscribers matter to the result.** Of the equal attempts, 367.88/cycle had subscriptions and 66.92 had none. Of the changes, 94.77/cycle had subscriptions. The first version sends every change to the canonical write while the old value is still in the store. Edges, synchronous guards and reentrant callbacks see the same transitions at the same point. Restricting admission to slots without subscribers would lose about 85% of the equal-write opportunity.

## Contract and differential check

On f28d6fe8 the narrow equal return precedes all observation logic. The strength check and held-value route are in [Scheduler::write](../../src/kernel/sim.cpp#L2582); initialization, table catch-up and the two-plane comparison are in [writeRaw](../../src/kernel/sim.cpp#L2811). A direct call against the Release kernel, with an armed guarded change waiter, returned false in four cases (whole/slice × four-state/approximate two-state). Storage, wait count and all counters agreed before and after except slot.writes +1. The following real change ran the guard and resumed its waiter once in every case. The patch specifies broader acceptance coverage for waves, VPI, race and other observers.

- Decide per physical store site, starting with fixed elements and whole or fixed in-bounds partial writes. Evaluate the RHS and address once. Multi-output and cyclic processes keep their existing order and wait behavior. Nets, synthetic driver storage, NBAs and dynamic ranges take the existing path.
- Bind the verified sites once per Context/design. After initialization and table catch-up, compare both planes for four-state and normalize X/Z before comparing in approximate two-state. Count an equal write once and keep the original ending call. Marked held and strength-owned slots use canonical write. Pass a change or stale binding to the canonical service before storing or counting. The only generation is a Context-wide storage/eligibility guard. Ordinary registration, monitoring, VPI, glitch capture and race observation do not invalidate it. Revisit per-observer eligibility when proposing direct *changed* writes.
- The review patch proposes optional service 20 v1 with its C layout, separate from fusion service 19 v1. Run the comparison in a generated shadow and a VM shadow, recording every attempt including equality, element, mask, normalized values, observation, counters and stop point. Replay synchronous observer effects that can affect a subsequent read or termination, or decline the site where the check cannot reproduce them. Publish once. Overflow fails the check. Execute modules contain no recording code.
- Cover full, partial and overlapping writes, A→B→A, X/Z equality and edges, same-visible-value writes during force followed by release, late registration, SCCs, separate Contexts, reentrancy, synchronous termination and SIGINT ordering. Verify actual admission and fallback.

## Generated-body instruction attribution

| Operation | Calls/cycle | Caller + service instructions/cycle |
|---|---:|---:|
| Function-table slot_words | 473.33 | 2,807.45 + 5,679.93 = **8,487.38** |
| Post-write ending | 546.10 | 2,184.40 + 1,638.30 = **3,822.70** |
| Instructions attributed to the two W4 arithmetic headers | | **4,849.18** |
| Replacing arrays of W4 temporaries with individual W4 locals | | **0 difference** |

The read row measures obtaining the address. The ending row measures dispatch and fetching the flag. Value arithmetic and the subsequent termination decision are outside those rows. Across 79 compiled bodies and specializations, the optimized IR had zero addressable temporary t arrays. Replacing 76 source arrays with individually named locals of the same type produced an identical 261,028-byte machine-code section. The 4,849 header instructions include necessary value operations. Unattributed register spills cannot all be charged to W4.

The previous **65,248 instructions/cycle** was a sampled generated-body scope including services and excluding kernel writes. We counted about 27,224 instructions/cycle in module code itself. The address lookup and ending totals are about 13.0% and 5.9% of the previous scope. Other work includes branches, value operations, checkpoints and other services, with uncertainty in the previous boundary attribution. These measurements cover this two-state workload and host.

## Experiment order and distance to the gate

The quiet timing reference is 9.706963 s native and 0.102895 s Verilator. The 1.40% gate requires **7.34964 s or less**. The current five-run stock median is 509,389 instructions/cycle. At constant time per instruction the required reduction is **123,704 instructions/cycle**.

| Order | First work package | Conditional Ibex estimate |
|---|---|---|
| **Recommended: variable W, then island 4 S/WS** | Bind sites, compare equal values, invalidate safely and check shadow traces. Ordinary bodies can use this without graph or scheduling changes. | Equal calls at fixed sites cost about 42,937/cycle. Allowing 16–32 instructions on every one of the 385.61 fixed attempts saves **30,597–36,767 (6.01–7.22%)**. Projects to **1.13–1.14%** of Verilator with **86,937–93,107** still needed. |
| Island 4 00/W/S/WS, then variable W | Scalable dirty set, multiple regions, generation validation, wait ordering, Wire proof and four controlled builds. | The existing plan's medium/optimistic fractions give **51,835–91,980** for island 4 WS, or **1.18–1.29%**. The remaining gap is **31,725–71,870**. All write/notification work on island 4 totals 59,946. |

**Dispatch and setup costs.** The table gives store-site savings before binding costs. The earlier 16–32 instructions/attempt excluded per-resume binding and validation. V1 removes those calls. With 56.98 CombAt resumes/cycle, allowing **1–3 extra instructions per resume** to pass a preselected binding is a sensitivity assumption pending disassembly. This subtracts 57–171 instructions/cycle, leaving **30,426–36,710** before amortizing setup. Subtract measured one-time validation, binding and teardown instructions divided by 433,198. Report invalidation costs and remeasure saved attempts if a binding becomes stale. Count binds and resume-time service calls separately, and compare complete warm runs to capture costs outside the sampled write sites.

A separate direct-read experiment would save **4,701–6,594 instructions (0.92–1.29%)** if validating and obtaining the address costs 4–8 instructions per read. Both this allowance and the 16–32 per write are assumptions. With disjoint savings fixed-site W plus reads projects to **1.14–1.16%**. Extending W to dynamic partials at the same unit cost, subject to proving clipping, bounds and invalid-selection behavior, would raise the W-only estimate to **47,212–55,949 (9.27–10.98%)**, or **1.18–1.21%** with reads. This sensitivity case goes beyond the first version. The W estimate retains all changed-write notification and takes no additional credit for ending. Measure declines, invalidation, check costs and time per instruction after approval.

The first decision for ordinary W needs two builds (control and fixed sites) and leaves the graph and wait queue intact. Starting with island 4 needs two independently working changes before measuring the four arms, with proofs for Wire, wake order, generation and shadow storage. Both orders eventually need the island comparison before a gate decision. Engineering days have not been measured.

Measure ordinary W against stock, isolate direct reads, then keep ordinary W fixed in all four island 4 builds to separate Wire, S and their interaction. If needed, proceed to island 0 scheduling and ordered NBA issue. Re-profile overlapping wake costs. Use at least five alternating warm runs on identical SIR, with elapsed time deciding both 1.40% and the four-state 1.05× ceiling. Keep switches in experimental builds.

## Measurement report after the fixed-store W run

Use identical saved Ibex SIR, firmware, CRCs, cycle count, state, compiler,
flags and warm-cache policy. Keep wall timing separate from instrumented
runs; record at least five interleaved controls, cold build time separately,
loads and retired instructions per simulated cycle. On the four-state run
also check the 1.05× limit against the fixed P4 entry build.

1. Hold ordinary fixed-store W constant. Compare that baseline, fixed-range
   direct RHS reads alone, and cheaper post-write ending tests alone. A fourth
   run with both measures interaction. Count executed direct reads, ending
   calls/tests and actual fallbacks. Verify synchronous termination and SIGINT
   ordering in each variant.
2. Hold reads, ending, islands and the other controls constant. Compare fixed
   W with fixed plus dynamic partial W. The entry workload issued 160.49
   dynamic partial writes/cycle. Report the admitted/declined counts, split by
   equal/changed, and test unknown, out-of-range and clipped selects in both
   state modes. Dynamic admission requires its own proof and review.
3. Profile only changed writes, separately for full and partial stores. The
   baseline inclusive costs are 419.6 and 441.6 instructions/call. In the
   W build split the same sample into exclusive ABI/validation, mask/plane
   merge and storage, observer dispatch and matching, guard/monitor work,
   wake/queue, and residual. Reconcile their sum with the inclusive window
   using matched empty-probe correction. Name costs that can be removed and
   costs required by active observers. Provide visit and wake counts.
4. In one table report ordinary W, island 4 WS incremental over W, island 0
   incremental over W+WS, the combined reduction versus stock, and the gap to
   **about 123,700 instructions/cycle**. Keep ordinary W fixed across island
   4's 00/W/S/WS. Report the paired whole-workload lower/upper observed
   reductions across repetitions, plus conditional low/high projections for
   any unimplemented arm with assumptions and overlap exposed. Take the
   combined range from paired end-to-end deltas on the same workload. Elapsed
   time decides the gate; include median seconds and the Verilator percentage.

## Measurement and replay

Source 133995a745d7, Apple M5 Max, macOS 27.2.1, Apple Clang 21. Host Release -O3 and emitted -O2, FP contraction off for both, no fast math in the module. Each run validated 433,198 cycles, exit 0 and CRCs e9f5/e714/1fd7/8e3a/e714. Use the saved SIR and firmware from the [Ibex reproduction procedure](p4-performance-profile.md). The [JSON](../measurements/2026-11-28/ibex-variable-sites.json) includes input hashes, before/after loads, three sampling series, five stock controls and both code-section hashes.

```sh
/usr/bin/time -lp "$K" --tier=native --state=2 --cxx=/usr/bin/c++ \
  --load-sir="$SIR" --cache-dir="$CACHE" --max-scc-iter=65536 \
  +image="$RAM" +cycles=443198
```

In a disposable module cache, wrap the function table only on ordinary CombAt entries. Classify a write by the canonical return value. For about one in 1,024 calls, subtract a matched empty call from the thread_selfcounts window. Report the median of the three seed-specific means per cell. The changed-call windows include synchronous delivery and wakes, so do not add them to the older exclusive 87,201. Independent diagnostics classified live guard and subscription state and counted execution at the five emitted dynamic write sites. Three destinations received dynamic writes. Dynamic-element reads occurred zero times on this input. For module attribution, weight original optimized AArch64 instructions by executed block counts, including shared helpers only inside CombAt. Multiply the exact dynamic service calls by the straight-line Release callee lengths (12 for reads, 3 for ending). Instrumented time and instrumentation instructions are excluded from the gate comparison.
