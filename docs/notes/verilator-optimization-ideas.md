# Verilator-Inspired Optimization Ideas and Experiment Guide for kirimasim

[日本語](verilator-optimization-ideas.ja.md) · This English text is the main version; the Japanese version follows it, and where they differ, this text holds.

**Clean-room notice (`docs/spec/AI_POLICY.md`):**
This note was compiled without reading or quoting any Verilator source code. It summarizes only design-level ideas from official documentation, release notes, public engineering blogs, and academic papers. Any implementation in `kirimasim` must be designed and validated independently against kirimasim's own SIR, analysis tables, `CODEGEN_SPEC`, `RUNTIME_ABI`, `valsem`, and `--tier-check`.

The backlog below uses the November 23 development measurements. Release 0.3.0
met both Ibex targets. [P3 exit measurements](../BENCHMARKS.md#p3-performance-targets)

## 1. Current bottlenecks and prioritization

[Stage A](p4-performance-profile.md) found that island evaluation and
NBA application are prominent in Ibex, whereas ineligible VM bodies
dominate CVA6. Establish fused acyclic evaluation and its observation
boundary first, then measure ordered NBA runs independently. Track
CVA6 eligibility and the 14 of 28 daily cases with no generated-body
calls separately. Measure generated wait/resume and the limited VM
bytecode pilot alongside the RTL work.

The table retains earlier experiment candidates. A3 follows proven
fusion and observation under a separate approval. Measure A1 flag and
hot/cold changes and A2 PGO after reducing structural costs, fixing
host and generated compiler settings in each paired comparison.
The estimates below are candidates for experiments.
[Design and order](p4-performance-proposal.md)

## 2. Prioritized experiment backlog

| ID | Idea | Target phase | Primary goal | Impact area | Effort |
| :--- | :--- | :--- | :--- | :--- | :--- |
| **A1** | Hot/Cold code splitting and `-Os` for generated C++ (I-cache) | P3 | Reduce instruction-cache misses and build time on large RTL | Runtime + build time | Low |
| **A2** | Compiler PGO (`-fprofile-generate` / `-fprofile-use`) | P3 / P7 | Branch prediction and code layout tuning | Runtime (~5–15% typical) | Low |
| **A3** | Localizing unobserved intermediate signals | P3–P4 | Demote internal slots to stack/register locals | 2-state/4-state RTL runtime | Medium |
| **A4** | Combinational DFG optimization, deduplication and peepholes | P3 | Eliminate redundant logic, replications and unaligned extracts | 2-state/4-state RTL runtime | Medium–High |
| **A5** | Packed/Unpacked aggregate decomposition (scalarization) | P3–P4 | Break false dependencies across struct fields and small arrays | Island eligibility + runtime | Medium |
| **A6** | Constant pool for constant aggregates and bulk/wide zero resets | P3 | Shrink init/reset code and speed up wide clears | Runtime + code size | Low–Medium |
| **B1** | Selective dynamic scheduling (strict RTL/testbench separation) | P4 | Keep wait-free RTL on the fast path when adding waiting tasks | Regression prevention | Medium |
| **B2** | Custom allocator for suspended call frames | P4–P5 | Cut heap allocation cost for suspended `automatic` tasks | Testbench / UVM runtime | Medium |
| **B3** | Optional C++ pretty-printing and 128 KB output buffering | P3–P4 | Speed up code emission (time to start running) | Startup / build time | Low |
| **B4** | Compact value/const node representation and pass fusion | P4–P7 | Reduce peak RSS and improve cache locality on large designs | Peak RSS + codegen time | Medium |
| **C1** | Dead class-method removal and devirtualization | P5 / P7 | Reduce virtual-call overhead in UVM class hierarchies | UVM runtime | Medium |
| **C2** | Hierarchical compilation and incremental caching | P6–P7 | Cut rebuild time and memory on large SoCs (CVA6 / OpenTitan) | Build / total time | High |
| **C3** | Coarsened static task partitioning and Thread PGO | P6–P7 | Limit synchronization overhead in multithreaded execution | Parallel scaling | High |

## 3. Details and experiment steps

### Tier A: Immediately testable in P3 (Ibex 1% and CVA6 0.2%)

#### A1. Hot/Cold code splitting and `-Os` (I-cache pressure)
- **Public source idea:** The Verilator guide ([Simulating — Benchmarking & Optimization](https://verilator.org/guide/latest/simulating.html), [FAQ](https://veripool.org/guide/latest/faq.html)) notes that instruction-cache size often limits large RTL models; compiling per-cycle hot paths with `-Os` and splitting rarely executed cold routines into separate units compiled at `-O0` improves both runtime and compile time. The MIT CSAIL paper ([MICRO 2023](https://people.csail.mit.edu/sanchez/papers/2023.ash.micro.pdf)) likewise highlights large instruction footprints as a primary RTL simulation bottleneck.
- **Application in kirimasim:** Split `src/codegen/` output into hot per-cycle evaluation units and cold initialization/registration units; benchmark `-O2` vs `-Os` vs `-O1 -fstrict-aliasing` (keeping `-ffp-contract=off` per `CODEGEN_SPEC.md`) on Ibex and CVA6 using 5+ run medians.

#### A2. Compiler PGO (`-fprofile-generate` / `-fprofile-use`)
- **Public source idea:** The Verilator guide ([Compiler Profile-Guided Optimization](https://verilator.org/guide/latest/simulating.html)) reports 5–15% speedups from compiler PGO on both single-threaded and multithreaded models.
- **Application in kirimasim:** Profile generated model builds on a short Ibex CoreMark run (e.g., 1 iteration), rebuild with `-fprofile-use`, and compare 10-iteration medians while verifying CRC, cycle count and `--tier-check`.

#### A3. Localizing unobserved intermediate signals
- **Public source idea:** Verilator's CLI guide ([verilator Arguments](https://veripool.org/guide/latest/exe_verilator.html), `--localize-max-size`) demotes non-observable signals from model state into function-local C++ variables.
- **Application in kirimasim:** Within P3 static islands and P4 combinational regions, demote slots that have no external readers, waveform/VPI hooks, static observers, or `force` targets into stack/register locals. Preserve full fallback to `Context` slots whenever an observer exists or equivalence is not statically proven.

#### A4. Combinational DFG optimization, deduplication and peepholes
- **Public source idea:** Verilator's reference ([verilator Arguments](https://veripool.org/guide/latest/exe_verilator.html)) and [Revision History](https://verilator.org/guide/latest/changes.html) describe DFG-based combinational optimization, duplicate combinational logic elimination, aligned word extracts, concatenation-of-replications folding and identical-operand simplification.
- **Application in kirimasim:** Add a lightweight DFG pass between `src/analysis/` and `src/codegen/` for eligible combinational islands. Start in `--state=2`, then enable only 4-state rules verified against `valsem` (preserving IEEE 1800-2023 4-state semantics such as `X ^ X == X`).

#### A5. Aggregate decomposition (packed/unpacked structs and arrays)
- **Public source idea:** Verilator's reference ([verilator Arguments](https://veripool.org/guide/latest/exe_verilator.html)) decomposes packed and unpacked aggregates into separate variables to eliminate false dependencies.
- **Application in kirimasim:** Decompose statically indexed, unobserved packed structs and small arrays in `src/analysis/` / `src/codegen/` to expand static-island eligibility and avoid unnecessary downstream re-evaluation on Ibex and CVA6.

#### A6. Constant pool for constant aggregates and bulk/wide zero resets
- **Public source idea:** Verilator's [Revision History](https://verilator.org/guide/latest/changes.html) highlights placing constant aggregates into a constant pool, bulk fills for integral unpacked arrays, optimized wide zero resets and table-driven VPI symbol registration.
- **Application in kirimasim:** Emit `static constexpr` pools for constant aggregates and use bulk zeroing/word fills for wide clears; measure binary size and runtime on `wide_arith`, `mem_rw` and Ibex.

### Tier B: P3 late to P4 (mixed execution, startup time and memory)

#### B1. Selective dynamic scheduling (strict separation of wait-free RTL)
- **Public source idea:** Antmicro's blog ([Using coroutines for low-overhead dynamic scheduling in Verilator](https://antmicro.com/blog/2021/12/coroutines-for-dynamic-scheduling-in-verilator)) shows that detecting which parts of a design require dynamic scheduling and keeping synthesizable RTL on the static path preserved vanilla performance on SweRV EH1.
- **Application in kirimasim:** Keep P4's two-level call-frame suspension strictly on tasks/processes that can wait, ensuring zero regression on wait-free RTL (daily 25 workloads and Ibex).

#### B2. Custom allocator for suspended call frames
- **Public source idea:** The same Antmicro blog notes that coroutine/frame allocation overhead can be mitigated with a custom allocator.
- **Application in kirimasim:** Use a pooled/free-list allocator for P4 suspended `automatic` task call frames and benchmark against `fork`/`join` and P4 acceptance tests 1b/1c under ASan.

#### B3. Optional pretty-printing and 128 KB output buffering
- **Public source idea:** Antmicro's scaling post ([Scaling Verilator for very large designs](https://antmicro.com/blog/2022/11/scaling-verilator-for-very-large-designs)) reports up to 6% codegen time spent on human-readable formatting and ~1.5% reduction by using 128 KB output buffers.
- **Application in kirimasim:** Emit compact C++ with 128 KB buffered I/O by default in `src/codegen/` (keeping pretty output optional for debugging) and measure time to start running on Ibex and CVA6.

#### B4. Compact value/const nodes and pass fusion
- **Public source idea:** The same Antmicro post reduced `V3Number` from 96B to 56B via unions and removing inferable metadata (saving 3 GB on a large design) and fused expanding/reducing passes to lower peak RSS.
- **Application in kirimasim:** Audit `sizeof` of SIR value/constant nodes and peak RSS (`VmHWM` / `ru_maxrss`) on CVA6.

### Tier C: P5–P7 (UVM, hierarchical builds and parallelism)

#### C1. Dead class-method removal and devirtualization
- **Public source idea:** Verilator's reference ([verilator Arguments](https://veripool.org/guide/latest/exe_verilator.html), `-fno-dead-members`) removes uncalled class methods and devirtualizes non-overridden virtual methods.
- **Application in kirimasim:** Apply closed-world class hierarchy analysis in P7 once P5 UVM workloads run, measuring per-transaction time.

#### C2. Hierarchical compilation
- **Public source idea:** Antmicro's post ([Improving Verilator's hierarchical mode](https://antmicro.com/blog/2025/05/improving-verilator-hierarchical-mode)) compiles hierarchical blocks independently to cut rebuild time and peak memory on large SoCs.
- **Application in kirimasim:** Cache SIR-to-C++ codegen units at module/subsystem boundaries in P6–P7 for OpenTitan and CVA6.

#### C3. Coarsened static task partitioning and Thread PGO
- **Public source idea:** Verilator's [ORConf 2018 slides](https://veripool.org/papers/Verilator_v4_Multithreaded_OrConf2018.pdf), [Thread PGO docs](https://verilator.org/guide/latest/simulating.html), [Revision History](https://verilator.org/guide/latest/changes.html) (per-thread progress words, AArch64 spin waits, avoiding worker wakeups for tiny work), and the [MIT CSAIL MICRO 2023 paper](https://people.csail.mit.edu/sanchez/papers/2023.ash.micro.pdf) show that fine-grained RTL tasks are dominated by synchronization overhead and benefit from coarsening plus measured-cost Thread PGO.
- **Application in kirimasim:** Design P6 parallel scheduling around coarse-grained task merging, lightweight synchronization and P7 Thread PGO feedback.

## 4. Shared experiment protocol for kirimasim

1. **Correctness gates:** `python3 tools/local_ci.py`, Tier 1/Tier 2 agreement, static-island on/off agreement, `--tier-check`, and `valsem` cross-checks for any 4-state rule.
2. **Count regressions (`PerfCount`):** inspect `--stats=json` to confirm reduced evaluations/writes.
3. **Wall-clock medians (5+ runs):** verify matching CRCs and final cycle counts first on the same host for Ibex (4-state/2-state) and CVA6 (2-state) after compile/cache prep, plus daily 25 workloads; record tool versions, commits, compilers and effective options.
4. **Attribution (`AI_POLICY.md`):** cite the public reference URL in `docs/spec/` or `docs/notes/` whenever an idea is adopted, with zero source code quoted.

## 5. Public references (no source code)

- Verilator 5.054 Official Guide — Simulating: https://verilator.org/guide/latest/simulating.html
- Verilator 5.054 Official Guide — Arguments Reference: https://veripool.org/guide/latest/exe_verilator.html
- Verilator 5.054 Official Guide — FAQ: https://veripool.org/guide/latest/faq.html
- Verilator 5.054 Official Guide — Revision History and Change Log: https://verilator.org/guide/latest/changes.html
- Antmicro Blog — Using coroutines for low-overhead dynamic scheduling in Verilator (2021-12-30): https://antmicro.com/blog/2021/12/coroutines-for-dynamic-scheduling-in-verilator
- Antmicro Blog — Scaling Verilator for very large designs (2022-11-22): https://antmicro.com/blog/2022/11/scaling-verilator-for-very-large-designs
- Antmicro Blog — Improving Verilator's hierarchical mode for better performance and scalability (2025-05-26): https://antmicro.com/blog/2025/05/improving-verilator-hierarchical-mode
- Wilson Snyder — Verilator 4.0: Open Simulation Goes Multithreaded (ORConf 2018): https://veripool.org/papers/Verilator_v4_Multithreaded_OrConf2018.pdf
- MIT CSAIL — Accelerating RTL Simulation with Hardware-Software Co-Design (MICRO 2023): https://people.csail.mit.edu/sanchez/papers/2023.ash.micro.pdf
