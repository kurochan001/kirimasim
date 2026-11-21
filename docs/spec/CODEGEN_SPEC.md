# CODEGEN_SPEC: C++ generation

[日本語](CODEGEN_SPEC.ja.md) · This English text is the main version; the Japanese version follows it, and where they differ, this text holds.

**Reader context:** Tier 2 generates C++ ahead of execution, while the current tier 1
interprets SIR; the [architecture
overview](../ARCHITECTURE.md#three-execution-tiers-one-kernel) explains the planned
boundary.

**Revision (2026-10-24):** old P2→P3, old P3→P4 and old P4→P5; new P2 (SIR v1) is inserted, with tier numbers unchanged ([mapping](../notes/phase-renumbering.md)).

Priority: P3 (moved from P1 on 2026-10-15; C++ generation is P3 work since the renumbering of 2026-10-24). This document has headings only so far, except the section "Floating point" and the first rules of "Rules of C++ generation", added on 2026-10-19 (approved), and "Static islands", whose first rule was added on 2026-10-19 and the rest on 2026-11-14 (approved).

## Rules of C++ generation

The rest is written in the phase the priority table gives. These rules were approved on 2026-10-19.

- The same SIR gives the same C++, byte for byte (NFR-6). CI generates it twice and compares the two.
- The short-circuiting of `&&`, `||`, `->` and `?:` is kept. SIR expresses it with branches (SIR_SPEC.md 5.2), and generated code turns a logical operation into a bitwise one only where the right side is proved free of side effects and of null references.
- An expression with side effects is evaluated as many times as SIR says. Code is duplicated only where it is proved free of side effects.
- Where the left-to-right order of VALUE_SEMANTICS.md 3.1 meets a C++ construct whose order of evaluation is not set, such as the arguments of a call, the values go through temporaries in that order.

## Static islands

The first rule was approved on 2026-10-19, the rest on 2026-11-14 ([proposal](../notes/static-islands-proposal.md), with its stages). The work is P3's; nothing here is implemented yet.

- Inside a static island, each process is evaluated once, in the level order that `analysis` decides. Tier 1 uses the same order for the processes of an island, so the values a time slot passes through, glitches included, are the same on both tiers.
- Whether a process enters an island is decided in `analysis` only, from SIR, and recorded in the analysis tables (SIR_SPEC.md 1; the SIR v1 proposal, D6). Island execution changes only when and in what order members are evaluated, never the meaning of an instruction.

### Candidates and reasons

- Candidates are the processes SIR_SPEC.md 7.1 names: of the kinds `cont`, `comb`, `latch`, `ff` and `ff-plain`, with no delay, no event control other than at the start, and no call with side effects.
- The first version leaves a process out of islands for any of the reasons below, and the analysis table records, for each process, its island or its reason codes. The codes are stable names. Widening the range later removes a reason; narrowing it would change expected values.

| Code | The process is left out because |
| --- | --- |
| `kind` | its kind is not one of the five above |
| `delay` | it has a delay |
| `mid-event-control` | it has an event control other than at its start |
| `side-effect-call` | it calls a subroutine with side effects |
| `net-resolver` | it is a `cont` that carries `netResolver` (SIR_SPEC.md 7.1) |
| `heap-value` | it reads or writes a value on the heap: a string, a dynamic array, a queue or an object |
| `system-task` | it calls a system task |
| `dpi` | it calls a DPI subroutine |
| `report-queue` | it queues reports of unique or priority violations, or deferred immediate assertions (SCHEDULER_SPEC.md 15) |
| `override-target` | it writes a slot that a procedural `assign` or `force` may target |
| `program` | it belongs to a program |
| `scc` | it is in a strongly connected component of the dependency graph (until the iteration is implemented) |
| `runtime-index` | a range it writes, or reads as sensitivity, has an index known only at run time (until the iteration of UNSPECIFIED_BEHAVIOR.md A7 is implemented) |
| `unknown-entry` | its entry in the analysis table is missing or stale |
| `codegen-unsupported` | on tier 2 only: its island holds an instruction tier 2 cannot generate, so the whole island runs on tier 1's island execution |

### Dependency graph, islands and level order

- The nodes of the dependency graph are the candidates not left out. There is an edge from P to Q when, in the same time slot, Q reads as sensitivity a slot range that P writes. Ranges are compared bit by bit (UNSPECIFIED_BEHAVIOR.md A7). Writes by NBA make no edge.
- An island is made for each connected component of the graph. `ff` and `ff-plain` processes woken by the same trigger (a slot, its bits and an edge) go into one island.
- The level order is a topological order of the graph; ties are broken by SIR process number. The same SIR and the same options give the same table, byte for byte.
- A strongly connected component is left out (`scc`) in the first version. When its iteration is implemented, it is iterated until it settles, on tier 1's island execution and on tier 2 alike, up to `--max-scc-iter` (UNSPECIFIED_BEHAVIOR.md A6; CLI.md 2.6).
- No slot is removed or merged: every slot keeps its storage.
- A missing or stale entry of the analysis table means unknown (the SIR v1 proposal, D6), and such a process stays out of islands (`unknown-entry`).

### Island execution on tier 1

- An island is registered with the execution kernel as one process; its members are not registered on their own (SCHEDULER_SPEC.md 11).
- It wakes when a slot range that a member reads as sensitivity, and that is written from outside the island, changes, or when the trigger of an `ff` or `ff-plain` member happens (SCHEDULER_SPEC.md 4.3).
- One wake-up evaluates, once each and in level order, the members whose inputs changed and the downstream members whose inputs those writes changed.
- A member's body runs on tier 1's interpreter as it does outside islands.
- In the first version, writes inside an island go through the kernel's ordinary writes, so processes waiting outside the island, waveforms and VPI callbacks see changes on the same path as without islands.
- NBAs of `ff` and `ff-plain` members are queued in the kernel as usual. Members woken by the same trigger are evaluated in the order they started to wait (SCHEDULER_SPEC.md 4.3), so the order NBAs are issued in does not depend on islands.
- The steps at time 0 do not change (SCHEDULER_SPEC.md 6.1).
- Nothing takes a process out of an island while the design runs; forms that would need it are left out by the reasons above.
- Race exploration mode (`--race-check`) does not perturb the order inside an island (SCHEDULER_SPEC.md 11).
- Islands are made with `--enable-pass=islands` until a person approves the completion of the stage that puts `ff` and `ff-plain` into islands (stage 3 of the proposal), and by default after that, when `--disable-pass=islands` turns them off (CLI.md 2.3).

### Tier 2, first version

- Only island members are turned into C++. Processes outside islands run on tier 1, mixed with them on the same execution kernel.
- Generated code follows "Rules of C++ generation" and "Floating point" above.
- An island that holds an instruction tier 2 cannot generate runs, as a whole island, on tier 1's island execution, and records `codegen-unsupported`. It is not an error.
- Only the 4-state version is made first. The 2-state version, X-reachability analysis and two-version switching come later.
- The order of evaluation inside an island and the number of evaluations of each member are the same as on tier 1's island execution.
- `--tier-check` compares every island evaluation of tier 2 with tier 1's island execution (CLI.md 2.3). The operations of generated code are cross-checked with `valsem` (CLAUDE.md).
- Building, loading and caching generated code (`--cxx`, `--cache-dir`) are not decided here.

The optional P2 analysis tables have schema version 1. Their key contains
the SIR format version, SHA-256 of the exact canonical SIR including its final
LF, schema version, target/layout ID, versioned producer ID and SHA-256 of the
producer's deterministic pass/options bytes. A consumer derives the expected
key from its current run and recomputes the design digest. Unknown versions,
a mismatched key, invalid IDs, widths or dependency order discard the table.

Value rows are strictly ordered by site kind, body kind, row ID, block ID,
instruction ordinal and value position. A site identifies a slot, block
parameter, instruction result or an SSA/constant operand. A row records its
width, no-X evidence and preservation reason. Island rows are ordered by
process ID. Each records its island, level and sorted predecessor IDs. Every
predecessor belongs to the same island at a lower level. Missing tables and
rows mean unknown. Analysis results stay outside SIR. P3 supplies the analyses
and decides eligibility under the static-island rules. The checked in-memory
interface is [tables.h](../../src/analysis/include/kirimasim/analysis/tables.h).

## Two-version switching

Not written yet (to be written in the phase the priority table gives).

## Floating point

Approved 2026-10-19.

- **Rule:** the execution kernel, the VM, the rest of kirimasim's own code (`src/`, `tests/`) and the C++ that tier 2 generates are built with `-ffp-contract=off`, and without `-ffast-math`, `-Ofast` or `-funsafe-math-optimizations` (CLAUDE.md). Dependencies are not held to this. Math functions (`$sin`, `$exp` and the like) are left to the system's libm, so their last bits may differ between systems; that is the one exception to NFR-2 for reals.
- **Why:** an expression on `real`, an IEEE 754 double (6.12 of the standard), must give the same bits on every tier and OS (NFR-2). A compiler that fuses a multiply and an add into one FMA instruction does not round the product before the addition, so the result can differ in its last bit from the two operations done apart. Whether it fuses depends on the compiler, the target and the flags.
- **Checked 2026-10-19:** `double f(double a, double b, double c) { return a * b + c; }` was compiled with `-std=c++23 -S`, at `-O2` and at `-O3` (the two gave the same), and the output searched for FMA instructions (`fmadd` on arm64, `vfmadd` on x86-64). The machine was an arm64 Mac; the other targets were compiled with `--target`, not run.

| Compiler | Target | Without `-ffp-contract=off` | With it |
| --- | --- | --- | --- |
| Apple clang 21.0.0 | arm64-apple-macos | FMA (`fmadd`) | No FMA (`fmul`, `fadd`) |
| clang 22.1.8 (Homebrew LLVM) | aarch64-linux-gnu | FMA | No FMA |
| clang 22.1.8 (Homebrew LLVM) | x86_64-linux-gnu | No FMA (`mulsd`, `addsd`) | No FMA |
| clang 22.1.8 (Homebrew LLVM) | x86_64-linux-gnu, `-march=haswell` | FMA (`vfmadd213sd`) | No FMA |
| GCC 16 (Homebrew) | arm64-apple-darwin | FMA | No FMA |

- So the presets, which gave neither `-ffp-contract` nor `-march` until 2026-10-28, built for arm64 (macOS, or Linux on arm64) code that fuses where a build for x86-64 does not, and a result on `real` could differ between the two. Apple clang fuses by default (it defines `__FP_FAST_FMA`). Since 2026-10-28, `CMakeLists.txt` gives kirimasim's own code `-ffp-contract=off`, and `tools/check_release_flags.py` checks that every unit of `src/` and `tests/` in the release build has it, and none of `-Ofast`, `-ffast-math` and `-funsafe-math-optimizations`.
- With the flag, none of the 8,170 tests of the dev build changed its result on macOS arm64 with Apple clang 21.0.0 (checked 2026-10-28).
- **Not confirmed:** GCC and clang on x86-64 Linux, the compilers of the Linux CI jobs, with and without `-march` (to check: the same compile in a CI job); whether the macOS runner of CI (`macos-26`) is arm64 (to check: `uname -m` in a CI job).
