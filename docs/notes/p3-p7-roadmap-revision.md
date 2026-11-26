# P3–P7 Roadmap Revision

[日本語](p3-p7-roadmap-revision.ja.md) · This English text is the main version; the Japanese version follows it, and where they differ, this text holds.

Date: 2026-11-26. Follows the earlier [phase renumbering (2026-10-24)](phase-renumbering.md) without altering historical approval records, release-time notes or execution tier numbers (Tier 1 = VM, Tier 2 = generated C++, Tier 3 = optional JIT).

## Phase mapping

| Previous | New | Scope and exit criteria |
| --- | --- | --- |
| P0–P2 | P0–P2 | Unchanged; inherit existing completion judgments. |
| P3 | P3 | Establish Tier 2 C++ compiled execution and meet practical RTL speed targets on Ibex/CoreMark. |
| — | P4 | Implement mixed VM/native execution foundations prone to later rework, verified by two representative mixed-execution workflows. |
| P4 | P5 | Inherit broader testbench features (UVM, constrained randomization, concurrent SVA, coverage, DPI-C, remaining VPI) plus the 145-item human review. |
| P5 | P6 | Inherit broader switch-level, parallel execution and embedding (`lib-create`) scope. |
| — | P7 | Final performance tuning once practical RTL and P5 UVM testbenches are in place. |

## P3 speed targets and measurement protocol

Speed ratios are defined uniformly as `(reference simulator runtime) / (kirimasim runtime)` (larger is faster).

- **P3 exit criterion 1:** Ibex/CoreMark 4-state native >= **3.0x** Icarus Verilog (`vvp` runtime).
- **P3 exit criterion 2:** Ibex/CoreMark 2-state native >= **1%** (0.01x) single-threaded Verilator (runtime within 100x Verilator).
- **CVA6 synthesis view:** tracked continuously with **0.2%** of single-threaded Verilator as the next improvement target (not a P3 pass/fail gate).
- **Protocol:** verify matching CRCs and final cycle counts first on the same host with identical RTL, testbench, firmware and iteration count; compare the median of 5+ runs after compilation and cache preparation, recording tool versions, commits, compilers and effective options. Existing CoreMark figures in `docs/BENCHMARKS.md` remain as historical development references; P3 completion requires re-measurement on current `main`.

Release 0.3.0 met the two P3 thresholds. The [exit measurements](../BENCHMARKS.md#p3-performance-targets) give the results and the [validation record](../VALIDATION.md) links the release CI.

## New P4 scope and acceptance tests

Starting from P2 SIR v1, the kernel ABI (`docs/spec/RUNTIME_ABI.md`), P3 static islands and `--tier-check`, P4 delivers the minimal implementation and CI regression tests across two pillars (five acceptance cases total; full AOT coverage of all SIR instructions and full UVM execution remain outside P4):

1. **Suspending VM/native calls:**
   - **1a (Basic suspend/resume + NBA):** an `automatic` task preserving local values across `#delay` or an event wait and issuing an NBA upon resume, including at least one path called from generated C++ into a VM waiting task, checked against VM reference execution (resume time, preserved values, caller return point, NBA order/result) and confirming native path execution.
   - **1b (Concurrent multi-frame wait via `fork`):** two concurrent invocations of the same `automatic` task suspending with distinct local values and resuming in reverse order to verify call-frame isolation.
   - **1c (Mid-wait `disable` / `disable fork`):** aborting a suspended call chain spanning generated C++ and VM frames to verify clean unwinding, wait cancellation and zero leaks.
2. **Combinational intermediate-signal observability:**
   - **2a (Static observer present vs. absent):** the same combinational circuit tested with and without an observer on an intermediate signal; verified for 4-state transitions and event order against VM reference when observed, batched evaluation when unobserved, and fallback to the conventional path whenever equivalence cannot be statically proven (usable by VPI and future SVA).
   - **2b (Dynamic `force`/`release` or VPI intervention):** runtime `force`/`release` or existing VPI access on an intermediate signal inside a candidate combinational region, verifying safe fallback and exact agreement with VM reference execution.

## P5–P7 performance tracking

- Across P4–P6, continue measuring the fixed daily 25 workloads and Ibex to catch regressions on existing RTL.
- Once SVA and UVM run in P5, record enabled/disabled overhead under identical stimulus and per-transaction time.
- In P7, optimize from real profiles of Ibex, CVA6 and P5 UVM environments. Previous P3 RTL targets (4-state >= 10x Icarus, 2-state 0.5x–1x Verilator) and the UVM target (>= 100x CIRCT-based reference) remain as long-term candidates; formal P7 pass ratios are decided after P5 and P6 measurements.
