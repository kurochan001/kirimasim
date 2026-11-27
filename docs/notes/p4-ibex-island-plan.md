# Ibex: where the island work goes and how to measure a change

[日本語](p4-ibex-island-plan.ja.md) · 2026-11-28. Measurements refer to the builds identified below. The implementation plan and the [single-driver Wire rules](../spec/CODEGEN_SPEC.md#continuous-assignment-to-an-ordinary-single-driver-wire-in-a-fused-region) were **approved on 2026-11-28**. The [reviewed diff](p4-single-driver-wire-review.patch) is retained. A new version of service 19 requires a separate review before implementation.

## Measured starting point

The five warm two-state runs of the implementation in `4fa11d58` on Apple M5 Max gave medians of 9.706963 s for native and 0.102895 s for single-thread Verilator 5.054, or **1.060%**. Firmware exit was 0 at 433,198 cycles with CRCs e9f5/e714/1fd7/8e3a/e714. The native median was 220,627,406,087 retired host instructions, **509,299 per simulated cycle**. At this reference time the **1.40%** gate requires at most **7.350 s**, a **24.28%** time reduction. Four-state time must also stay within **1.05× the fixed P4 entry build**. See the [samples, hashes, flags and loads](../measurements/2026-11-28/ibex-two-islands.json) and [gate definition](../spec/PERFORMANCE.md).

These runs used macOS 27.2.1 arm64, Apple Clang 21.0.0, Release `-O3 -DNDEBUG -ffp-contract=off`, generated C++20 `-O2 -fPIC -shared -fno-exceptions -fno-rtti -ffp-contract=off -fno-fast-math`, saved SIR, `--tier=native --state=2 --max-scc-iter=65536`, one warm-up and alternating order. One-minute load was 4.60 → 4.44. The comparison uses the P3 reference options and the same firmware. The [replay procedure](p4-performance-profile.md) identifies the inputs.

All 130,473,903 island member evaluations called individual generated bodies. None took the fused path. The following counts classify writers in the saved SIR. Eligibility for a direct store needs the separate net proof below.

| Island | Members, by operation and destination | Member evaluations |
| --- | --- | ---: |
| 4 | 304 continuous assignments with one SIR writer to a Wire | 59,492,260 |
| 4 | 3 continuous assignments with one writer to a variable | 1,204,676 |
| 4 | 108 continuous assignments to disjoint intervals of shared destinations | 13,123,116 |
| 4 | 2 continuous assignments with potentially overlapping writers | 765,881 |
| 0 | 73 clocked FfPlain processes | 31,623,254 |
| 0 | 5 continuous assignments with one Wire writer | 2,599,186 |

Island 4 totals **417 members / 74,585,933 evaluations**, island 0 **78 / 34,222,440**. Together they account for **83.39% of evaluations**. Elapsed time has a different denominator.

### Cost estimates and their limits

In the P4 entry profile, island scheduling and wait handling accounted for **32.7% of CPU self samples** across Ibex. A separate instruction probe used the pre-trace build (base `8491abdf` with fusion changes, same host and compiler flags). It sampled every 503rd time slot at three offsets, subtracted empty-read cost and reconciled boundary overhead to 508,165 instructions/cycle. Probe load was 47.88 → 40.33. Treat the following as **estimated exclusive instructions per simulated cycle**. Writes and NBA issue are charged to the source and wakes to the recipient. Necessary writes at external boundaries are included. [CPU sample definitions](p4-performance-profile.md) · [Instruction model and binary identity](../measurements/2026-11-28/ibex-two-islands.json)

| Scope | Scheduling and wait re-arm | Writes and notification | Wake and enqueue | NBA issue |
| --- | ---: | ---: | ---: | ---: |
| Island 4 | 68,364 | 59,946 | 20,281 | 0 |
| Island 0 | 35,369 | 30,140 | 8,562 | 6,005 |

For both islands together, saving 45%, 25%, 30%, 15% respectively in the four columns would save about **78,755 instructions/cycle**. At 70%, 55%, 55%, 40% the saving would be **140,426**. The original model projected **8.98 s / 1.22%** and **7.69 s / 1.43%** using the P4 entry pair (10.6253 / 0.109715 s). With the later low-load pair, the equivalent instruction budget is **about 123,700 instructions/cycle**, assuming unchanged time per instruction. Wall time decides the gate. Even *all* 59,946 write/notification instructions on island 4 cover less than half that budget. The scheduling experiment belongs in the first comparison.

## Why 242 islands can produce 156 wakes per cycle

The [static analysis](../../src/analysis/islands.cpp) joins eligible producers and readers when an **immediate write** overlaps a sensitivity range on the same physical slot. It also joins eligible clocked processes with a matching physical trigger range and edge. Shared inputs alone do not connect combinational readers. An NBA takes effect in a later region. A process excluded from analysis can feed an island without joining its component. For example 77 CombAt processes in this SIR run outside the islands. The partition follows these rules across hierarchy.

Of 833 members, **187 form singletons**. All 187 are continuous assignments, including 65 port-connection processes. Of the 187, **101 have an empty trigger list and ran once each**. The other 55 islands have sizes 2 (33 islands), 3 (9), 4 (3), 5 (6), 8 (2), 78 and 417. All singletons together contributed 7,013,091 evaluations (16.19/cycle). Static island count alone overstates the repeated work.

The [scheduler](../../src/kernel/sim.cpp) increments `proc.wakeups` when it executes an island batch or resumes an ordinary process (including applicable final/postponed entries). In two-state Ibex:

| Count over 433,198 cycles | Total | Per cycle |
| --- | ---: | ---: |
| Logical wakes | 67,577,682 | 156.00 |
| Island batch executions (`island.evals2 + island.evals4`) | 41,593,458 | 96.01 |
| Remaining wakes (difference) | 25,984,224 | 59.98 |
| Member evaluations inside batches | 130,473,903 | 301.19 |

Several wakes can share one batch. A different island, a different wake phase or an intervening ordinary Active event can split a batch; subsequent value changes can wake the same island again. Ordinary generated CombAt bodies alone had 24,684,622 calls (56.98/cycle). Merging islands cannot remove those ordinary dispatches by itself.

**Opportunity to test.** Five singletons have the same declared unguarded trigger on SIR slot 778 and each ran 440,217 times. Perfectly coincident notifications could save at most **1,760,868 physical dispatches (4.06/cycle) for these five**. Effective bit sensitivities, actual adjacency and savings are unconfirmed. First trace matching range/edge, time, region, phase, wait generation/order and adjacent queue entries, including why a batch ends. Only coalesce a contiguous subsequence whose reference order is reproduced, including notifications appended during execution. Never pull work past another Active event, a pending observer, a clocked waiter or the NBA boundary. Preserve time zero, stale-wait cancellation, SCC rounds, interrupt prefixes and logical counters. Count physical pops and batches separately. Trace and compare a shared-input fanout with an ordinary waiter deliberately inserted between subscribers, plus disjoint slices and distinct clocks. Cross-island coalescing is a separate follow-up experiment.

## Remove scaling costs before widening eligibility

1. **Validate a region once per borrow.** Today `evaluateCombGroup` scans every member's observation generation, monitor and module-path state, then its live wait. At bind time build a reverse index from each physical slot and relevant wait/alias to all affected regions. Maintain a region epoch or invalid flag on subscription changes, dynamic order, shared waits, monitor and module-path registration/removal, force, waveform and foreign observation. Audit every mutation; retain global guards for race, VPI, pending NBA, clocking and queue state. One equality check can cover observation only if all such mutations invalidate before visibility. On stale/saturated epoch take the ordinary path; revalidate off the hot path before reuse. Track wait readiness independently (unarmed/pending/resource counts and generations for dirty entries). A valid observer epoch alone does not prove a valid wait. Test late install/remove, saturation, re-entry and two independent contexts.
2. **Index the dirty members.** Build process ID → (region, rank) at bind, use generation-checked O(1) lookup in place of `find_if` for each dirty entry. Preserve level/SIR order for combinational members, clocked notification order and kernel batch boundaries. Exercise stale, duplicate, missing and out-of-order entries.
3. **Bound checking storage by the region.** `startFusionCheck` copies both complete storage domains; completion compares both again. Precompute the deduplicated physical read/write closure (including aliases, partial reads, guards and all a/b words). Map descriptor offsets into two compact shadow arrays, copy the closure for each invocation and compare its final contents. Restrict both generated shadow accesses and reference writes to the proven footprint; catch an unexpected destination even if its final value is restored. Test aliases, unrelated live state, changed/unchanged writes and injected out-of-footprint writes. If isolation is unproven, select ordinary checking before running the shadow. After starting, overflow or mismatch fails the check. Only the VM publishes effects. Execute builds contain no recording.
4. **Select regions per island and use multiple words.** Today `emitFusion` and `prepareCombGroup` examine the whole design's list, the VM checks unrelated processes, the scheduler holds one group, and service 19 v1 has one 64-bit ready, visited and consumer mask. Bind deterministic per-region descriptors and index them by island/batch. Scope proof to the region while accounting for all outside readers and writers. Propose an exact new service version for the descriptor table, bounded multiword ready/visited sets and sparse successor lists; retain v1 loading, version the emitter/cache and distinguish execute/check. Iterate nonempty words in rank order, size the wait-order overflow guard to the actual bound, preserve pointer lifetime and interrupt order. Review the ABI diff separately before implementation. Exercise 63/64/65, 127/128/129, multiple regions, mixed eligibility, sparse/dense propagation, startup and SCC fallback.

First validate these changes on the existing small region and synthetic scale controls. Record member scans, dirty lookups, shadow words copied/compared, heap operations and physical wait re-arms in diagnostic runs. With one dirty member and many idle ones, the steady execute entry should do constant observation work and one rank lookup; adding unrelated storage must add zero shadow words. Recheck `comb_chain` and `hierarchy_fanout`, including ordinary fallback, before admitting the large Ibex region.

## First experiment on island 4

Keep 00/W/S/WS in four reproducible experiment-only Release builds, selected at compile time outside the distributed simulator. No runtime option, environment variable or hidden selector may ship on main. Any proposal to retain a runtime control must first go through CLI specification review. All use the same generic acyclic Cont proof, coverage, body code, storage layout and observer boundaries. Admit only proven subregions of the 417-member island; the 108 split-range writers and two possible overlaps stay on the ordinary path. Preserve the queue position of every ineligible peer. If a boundary cannot be separated, fall back for that batch and count the reason. Keep island 0 and NBA on their original path. Island IDs select *report rows*, never production behavior. Record any other qualifying regions and charge their changes separately. First compare the shared scaffold with both optimizations off against `4fa11d58` to expose its own cost.

| Variant | Physical write and notification | Scheduling and waits |
| --- | --- | --- |
| 00 control | Ordinary kernel write | Ordinary dispatch, heap, re-arm |
| W only | Proven direct store/change detection; deliver the same matching waits at the same point | Same dispatch, heap and re-arm as 00 |
| S only | Ordinary kernel write and observer delivery at the original point | Indexed ready set, ordered dispatch and proven internal re-arm |
| WS | W | S |

Keep the reference notification/queue contract between the two changes. W-only can use a host-side write helper on the existing per-member path. S-only routes the ordinary write's actual internal wake into the new dispatcher; synchronous/external observers retain ordinary delivery. Do not call back into the host with borrowed pointers. Split or fall back **before** a borrow if an external event may interleave. Compare identical eligibility in all four variants and disclose actual coverage/fallback differences. This separates both main effects and their interaction.

- Pin SIR/firmware, OS, compiler and flags above, reference binary, cache identity and load limit (one-minute load ≤9 on this 18-core host). Warm each cache, then alternate/balance the four variants and reverse order across blocks, at least five valid runs each. Record loads before/after, raw time and retired instructions, binary hashes and dispersion. On macOS use `/usr/bin/time -lp` on the simulator process, as in [the replay tool](../../tools/coremark_profile.py). Keep compilation and diagnostic runs separate.
- For each arm publish **median retired host instructions / 433,198** and median absolute seconds. Report savings 00−W, 00−S, 00−WS and the interaction (00−WS)−(00−W)−(00−S). In a separate attribution run report island 4's exclusive buckets, queue operations, fallback reasons and fraction of writes meeting the Wire proof. None of the three experimental results has been measured yet.
- Require all five CRCs and 433,198 cycles, ordered effects against the VM in both state modes, unchanged logical PerfCount values and the observation/interrupt checks. Compare the low-load four-state absolute time with the pinned entry build. A change with fewer instructions but no repeatable time gain does not establish a speed gain.
- Use the measured WS gap to budget island 0. Prototype ordered clocked dispatch and then contiguous zero-delay NBA issue separately. Capture RHS and indices in Active; commit in issue order in NBA, preserving outside issuers, partial overlaps and observers. Island 0's entire issue-control budget is about 6,005 instructions/cycle. Re-measure the complete Ibex run after each step. If both islands leave a gap, profile remaining boundary writes and ordinary CombAt bodies before choosing another change.

## Reproducing the topology and wake counts

A diagnostic replay at `264ad2ce` matched the saved standard output, counters, static island report, every tier row and the empty fusion report byte for byte. With the pinned SIR and firmware paths from the [profile](p4-performance-profile.md), run:

```sh
mkdir -p build/ibex-plan-check
build/release/src/driver/kirimasim --load-sir="$IBEX_SIR" --tier=native --state=2 \
  --cxx=/usr/bin/c++ --cache-dir=build/ibex-plan-check/cache --max-scc-iter=65536 \
  --stats=json --report-islands=build/ibex-plan-check/ibex.islands \
  --report-tier=build/ibex-plan-check/ibex.tier.jsonl \
  --report-fusion=build/ibex-plan-check/ibex.fusion.json "+image=$COREMARK_HEX" \
  +cycles=443198 > build/ibex-plan-check/ibex.stdout
```

Group tier rows by non-null `island`, count rows for island size and sum `native_calls` for evaluations on this all-native input. Join `process_id` to the SIR process row for kind, triggers and written ranges. Read the final JSON line of stdout for the wake decomposition above. The five-run timing series was collected separately.
