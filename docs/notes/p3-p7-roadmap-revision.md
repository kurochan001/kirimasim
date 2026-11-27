# P3–P7 Roadmap Revision

[日本語](p3-p7-roadmap-revision.ja.md) · This English text is the main version; the Japanese version follows it, and where they differ, this text holds.

Date: 2026-11-26. Follows the earlier [phase renumbering (2026-10-24)](phase-renumbering.md) without altering historical approval records, release-time notes or execution tier numbers (Tier 1 = VM, Tier 2 = generated C++, Tier 3 = optional JIT).

The P4 performance amendment was approved on 2026-11-27. [Approval scope and design](p4-performance-proposal.md)

## Phase mapping

| Previous | New | Scope and exit criteria |
| --- | --- | --- |
| P0–P2 | P0–P2 | Unchanged; inherit existing completion judgments. |
| P3 | P3 | Establish Tier 2 C++ compiled execution and meet practical RTL speed targets on Ibex/CoreMark. |
| — | P4 | Deliver suspending calls, intermediate-signal observation and fused static-island evaluation, with correctness checks and an Ibex speed gate. |
| P4 | P5 | Deliver the testbench milestones below and complete the 145-item human review. |
| P5 | P6 | Inherit broader switch-level, parallel execution and embedding (`lib-create`) scope. |
| — | P7 | Optimize practical UVM and finish cross-workload RTL and testbench tuning. |

## P3 speed targets and measurement protocol

Speed ratios are defined uniformly as `(reference simulator runtime) / (kirimasim runtime)` (larger is faster).

- **P3 exit criterion 1:** Ibex/CoreMark 4-state native >= **3.0x** Icarus Verilog (`vvp` runtime).
- **P3 exit criterion 2:** Ibex/CoreMark 2-state native >= **1%** (0.01x) single-threaded Verilator (runtime within 100x Verilator).
- **CVA6 synthesis view:** tracked continuously with **0.2%** of single-threaded Verilator as the next improvement target (not a P3 pass/fail gate).
- **P3 measurement protocol.** Match CRC and final cycle count on the same host with identical RTL, testbench, firmware and iterations. Compare at least five warm runs per engine after compilation and cache preparation, and record versions, revisions, compilers and effective options.

Release 0.3.0 met the two P3 thresholds. The [exit measurements](../BENCHMARKS.md#p3-performance-targets) give the results and the [validation record](../VALIDATION.md) links the release CI.

## New P4 scope and acceptance tests

Starting from SIR v1, the kernel ABI, P3 static islands and `--tier-check`, deliver three pillars through the normal execution path and verify the eight acceptance cases below. Full AOT coverage of SIR and full UVM execution remain later work.

1. **Suspending VM/native calls:**
   - **1a (Basic suspend/resume + NBA):** an `automatic` task preserving local values across `#delay` or an event wait and issuing an NBA upon resume, including at least one path called from generated C++ into a VM waiting task, checked against VM reference execution (resume time, preserved values, caller return point, NBA order/result) and confirming native path execution.
   - **1b (Concurrent multi-frame wait via `fork`):** two concurrent invocations of the same `automatic` task suspending with distinct local values and resuming in reverse order to verify call-frame isolation.
   - **1c (Mid-wait `disable` / `disable fork`):** aborting a suspended call chain spanning generated C++ and VM frames to verify clean unwinding, wait cancellation and zero leaks.
2. **Combinational intermediate-signal observability:**
   - **2a (Static observer present vs. absent):** the same combinational circuit tested with and without an observer on an intermediate signal; verified for 4-state transitions and event order against VM reference when observed, batched evaluation when unobserved, and fallback to the conventional path whenever equivalence cannot be statically proven (usable by VPI and future SVA).
   - **2b (Dynamic `force`/`release` or VPI intervention):** runtime `force`/`release` or existing VPI access on an intermediate signal inside a candidate combinational region, verifying safe fallback and exact agreement with VM reference execution.

3. **Fused static-island evaluation and direct slot access**
   - **3a** exercises an acyclic region that takes the fused path. Compare member and write traces against VM for an external input in the middle, fanout, unchanged values, X/Z and time zero.
   - **3b** checks synchronous observation and runtime attachment and termination. A fallback must retain the already-published effect prefix.
   - **3c** compares grouped NBA issue and apply order and observation points, including overlapping slices and NBAs issued by a process outside the island.

Check existing counters and `--tier-check` in each case. Also exercise
SCC iteration, independent Contexts and native cache reuse.

Pillar 1 also demonstrates generated continuations for both a delay
loop and a simple event wait in ordinary `initial` or `always` bodies.
Verify fewer VM SIR instructions in the selected processes. Report
actual resumptions, SIR instructions and runtime for fixed `proc_wait`
and `comb_chain` inputs, with `fork_join` and `func_call` as controls.
Unsupported frames, waits and calls retain the ordinary path.

P4 requires Ibex/CoreMark 2-state native to reach **1.40%** of
single-threaded Verilator 5.054 with P3 options on the same host, and
4-state native runtime
to stay within **1.05×** the pinned P4 entry build. Track the CVA6
synthesis view toward **0.30%** and the native/VM geometric mean of
the fixed 25 toward **1.05×** a new quiet-host baseline.

## P5–P7 scope and performance tracking

Set P5 milestones in order for minimal UVM, DPI-C, constrained
randomization, concurrent SVA, coverage and remaining VPI. Record
correctness, enabled/disabled time, and per-transaction time or the
matching throughput at each milestone. The 145-item review is still
required at P5 exit. Consider an earlier, narrow SVA subset for
features blocking practical RTL.

Improve RTL performance from P4 onward. P5 sampling, coverage, VPI
and DPI-C use the P4 observation and commit boundaries. Pilot VM
bytecode before P5 and decide on replacing the interpreter after
profiling P5. P6 measures parallel scaling. P7 focuses on practical
UVM and testbench optimization and final tuning. The earlier RTL
targets (10× Icarus in 4-state, 0.5×–1× Verilator in 2-state) and
UVM target (100× the CIRCT-based reference) remain long-term candidates.

[Design and milestone checks](p4-performance-proposal.md)
