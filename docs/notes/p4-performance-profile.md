# P4 performance profile (phase A)

[日本語](p4-performance-profile.ja.md)

Measured on November 27, 2026 at `b1ad0cb96a40` (0.3.0 on `main`). This record
covers profiling and controls for the roadmap review.

## CoreMark results

Both engines used one firmware iteration and the same RTL view for each design.
Ibex completed **433,198 cycles** and the CVA6 `cv32a65x` synthesis view completed
**1,051,805 cycles**. Every completed engine run matched the five CRCs
`e9f5`, `e714`, `1fd7`, `8e3a`, `e714` and firmware exit code 0. CVA6 used
the RTL with the upstream synthesis exclusions applied. Its ordinary simulation
view was outside these measurements.

| Workload and engine | Warm median (s) | µs per simulated cycle | Host instructions per simulated cycle |
| --- | ---: | ---: | ---: |
| Ibex, 2-state native | 10.625 | 24.53 | 503,199 |
| Ibex, 4-state native | 10.726 | 24.76 | 509,467 |
| Ibex, Verilator 5.054, P3 settings | 0.1097 | 0.253 | 6,492 |
| Ibex, Verilator 5.054, guide settings | 0.1105 | 0.255 | 6,492 |
| CVA6 synthesis, 2-state native | 344.794 | 327.81 | 6,115,450 |
| CVA6 synthesis, Verilator 5.054, P3 settings | 0.8635 | 0.821 | 21,927 |
| CVA6 synthesis, Verilator 5.054, guide settings | 0.8383 | 0.797 | 21,443 |

Ibex reached **1.033%** of baseline Verilator speed (about **97×** the elapsed
time) and retired **77.5×** as many instructions. The host retired just **1.2%**
fewer instructions in 2-state native than in 4-state native. CVA6 reached
**0.250%** of baseline Verilator speed on this host. Its result applies
to the synthesis view and the pinned firmware.

The P3 Verilator settings are `--binary --timing --threads 1 --top-module top
-Wno-fatal`. The [official performance settings](https://verilator.org/guide/latest/simulating.html#benchmarking-optimization)
add `-O3 --x-assign fast --x-initial fast --no-assert`. A separate fresh paired
build of both options used the same Verilator 5.054 and host compiler. Its
default/guide medians were **0.1448/0.1475 s** for Ibex and **0.8820/0.8792 s**
for CVA6. Retired instructions were effectively equal for Ibex and fell **2.2%**
for CVA6. At this run length the Ibex wall times moved by more between sessions
than between the two options within a session. The guide run changes the
settings for unknowns and assertions. The equality check here covers the
recorded firmware outputs and cycle counts.

### Conditions

The host was an Apple M5 Max (18 cores, 128 GiB), macOS 27.2.1 arm64 (26B131).
The simulator used Apple Clang 21.0.0 (clang-2100.4.1.1), Release
`-O3 -DNDEBUG -ffp-contract=off`. Generated C++ used the same compiler with
`-std=c++20 -O2 -fPIC -shared -fno-exceptions -fno-rtti -ffp-contract=off
-fno-fast-math`. The fresh Verilator build used that compiler and its default
generated hot-code flag `-Os`. All reported run times exclude the compiler and
use the warm native cache. Five timed runs per mode and reversed execution
order separate run time from setup. The CVA6 counts and CPU samples use separate
runs. The data records every sample and its initial and final 1/5/15-minute
load average, with one-minute values from 2.71 to 7.31 for the timed CoreMark
series.

The run reused the saved P3 SIR, firmware and reference binary. All **37**
Ibex RTL compilation inputs match the Git blob IDs of the OpenROAD snapshot
`404977691e84` under `flow/designs/src/ibex`. The `a9adcf481395` Ibex
attribution in the P3 record needs correcting. All **165** original CVA6
compilation inputs match OpenROAD `a9adcf481395` under `flow/designs/src/cva6`
before synthesis exclusions, with CVA6 provenance `3a389af15180`.
The firmware uses Ibex source `041098fe2380`. [P3 record](../BENCHMARKS.md#p3-performance-targets)

The firmware SHA-256 begins `6af83d1f86eec882`. The saved SIR
hashes begin `dc4dfdc469330447` for Ibex and `66ca4d5a645f500a` for CVA6.
The CVA6 input uses the upstream `translate_off/on` exclusions,
`HPDCACHE_ASSERT_OFF`, and a `1us/1us` time scale. The saved SIR was generated
with `--single-unit -DVERILATOR` in addition to those options.
Native used `--max-scc-iter=65536` with `+cycles=443198` and
`+cycles=3000000` respectively. These plusargs bound execution. The divisor
is the final simulated cycle count. [Raw measurements](../measurements/2026-11-27/p4-coremark-profile.json)

## Where the cycles go

The table divides diagnostic counts by CoreMark's final simulated cycle count.
The island column shows native body calls **within** an island as a fraction
of `island.member_evals`. For these runs the island calls plus the island VM
entries exactly matched that denominator. Ordinary native calls are separate.
`wait.subscriptions` includes logical registrations via reused bindings, and
the NBA event count includes logical updates in coalesced queue entries.

| Counts per simulated cycle | Ibex native 2 | Ibex native 4 | CVA6 synthesis native 2 |
| --- | ---: | ---: | ---: |
| Island member evaluations | 301.19 | 300.87 | 779.87 |
| Island member evaluations entering generated code | 100% | 100% | 4.16% |
| Slot writes and changes | 983.96 / 463.54 | 972.75 / 457.99 | 1,656.36 / 565.51 |
| NBA registrations | 41.98 | 41.98 | 286.16 |
| Wait subscriptions | 1,157.61 | 1,154.98 | 2,275.85 |
| Process wakeups | 156.00 | 155.43 | 52.41 |
| Active events and NBA events | 156.00 / 41.98 | 155.43 / 41.98 | 52.41 / 286.16 |
| Time slots | 2.00 | 2.00 | 2.00 |
| Executed SIR instructions | 54.82 | 54.82 | 19,503.75 |

**CPU samples.** Ibex used three five-second `sample` runs per state mode.
Sampling used a 1 ms interval, a warm cache and no tier report.
Only stacks below `Scheduler::run` enter the denominator. For CVA6, the table
uses four five-second windows starting at 12, 110, 210
and 305 seconds after launch during a complete firmware run. The separate
25,000-cycle prefix profile matched Verilator's final bus read/write counts. The `time -lp` retired-instruction totals measure the entire
invocation, including startup. The CPU sample fractions below are a different
measurement. For the NBA attribution, the simulator's machine code matched a
relinked copy with debug line tables byte for byte. Stack offsets that resolve
to NBA application in `Scheduler::execute` identify that path. Coalesced
frames with elided or mixed offsets are left with their ordinary attribution.

| Exclusive stack-sample category | Ibex native 2 (11,471) | Ibex native 4 (11,484) | CVA6 native 2 (15,186) |
| --- | ---: | ---: | ---: |
| Island scheduling and wait handling | 32.7% | 32.0% | 14.7% |
| Slot writes and notifications outside attributed NBA apply | 25.7% | 23.8% | 8.7% |
| Generated C++ | 16.8% | 16.5% | 0.4% |
| NBA registration | 1.2% | 1.2% | 0.6% |
| Attributed NBA application | 2.2% | 2.5% | 1.1% |
| VM interpreter | 4.5% | 6.3% | 59.2% |
| VM resume and dispatch | 2.8% | 3.1% | 3.3% |
| Time slots and region dispatch | 1.5% | 1.5% | 0.3% |
| Other | 12.7% | 13.1% | 11.6% |

For 2-state Ibex, **61.3%** of all sample stacks include `evaluateIsland` and
**2.2%** are in the identifiable NBA application path. The union is **63.5%**.
The 155,158,525 C++ calls include 24,684,622 ordinary native calls outside
islands. Every executed SIR instruction was attributed to the four testbench
processes. `top.always1` (RAM and firmware I/O) accounts for 19,417,512
instructions and `top.always0` (clock) for 4,331,987. Exclusive VM interpreter
samples outside island evaluation account for **2.6%**. This figure covers
interpreter self time on that path. It does not apportion kernel work initiated
by the testbench to individual processes.

CVA6 spends most of its sampled time in the VM **inside** island evaluation.
Across the full firmware run, 198 rejecting processes executed **11.26 billion**
SIR instructions and 10,648 peer-blocked processes executed **7.07 billion**.
The highest individual counts were
`top.dut.issue_stage_i.i_issue_read_operands.gen_asic_regfile.i_ariane_regfile.always_ff0`
(1.335 billion),
`top.dut.issue_stage_i.i_issue_read_operands.gen_asic_regfile.i_ariane_regfile.always_comb0`
(1.285 billion), and `top.dut.issue_stage_i.i_scoreboard.always_comb2` (1.195
billion). Each reported `process-frame`. The largest island includes blockers
`top.dut.$port.cvxif_resp_i` (`instruction store`) and
`top.dut.i_frontend.always_comb1` (`process-frame`). Blocker weights overlap
when several blockers prevent the same process from using C++. Instructions
from independent callbacks can appear only in the global VM counter.

The island report found no SCCs for Ibex or the six daily controls. CVA6 had
four SCCs, with 21, 29, 45 and 2,136 members. Temporary instrumentation tallied
the last round of each component history on reset or finish. The full CVA6 run recorded **10,440,440** histories. The median was **4** rounds, the 95th percentile was **12**, and the maximum was **25**.
The histogram describes the scheduler's iteration histories. The temporary
instrumentation lives in an isolated build, and its time does not enter the
speed comparison. [SCC and reason counts](../measurements/2026-11-27/p4-coremark-profile.json)

## Daily controls and Icarus

The fixed workloads use the manifest from `befffe9069ef` and verified RTL
hashes. For each mode, five alternating runs at N and 1 use the same saved SIR
after cache preparation. Normalized counts and host instructions use
`(N result − 1 result) / (N − 1)`. A workload's N can count transactions,
frames or stimulus iterations. The data includes time slots per increment
and per-region counters. The one-minute load average ranged from 3.43 to 7.31 during the daily timings.

| Workload | N | 4-state VM / native Δ (s) | Native host instructions per increment | Native member evaluations / writes / NBAs per increment |
| --- | ---: | ---: | ---: | ---: |
| `nba_shift` | 1,000,000 | 6.119 / 2.509 | 72,257 | 17 / 83 / 81 |
| `comb_chain` | 3,000,000 | 2.138 / 1.022 | 10,374 | 8 / 13 / 0 |
| `hierarchy_fanout` | 100,000 | 2.991 / 2.735 | 631,072 | 384 / 513 / 0 |
| `bus_fabric` | 150,000 | 0.481 / 0.355 | 60,741 | 52.1 / 57.1 / 1 |
| `uart_loopback` | 20,000 | 0.203 / 0.160 | 204,042 | 83.5 / 154.5 / 66 |
| `gate_udp` | 400,000 | 2.752 / 2.229 | 89,321 | 51.8 / 53.8 / 0 |

Icarus 14.0 devel (`1eba369`, the 0.2.2 benchmark binary) ran the same inputs.
The reference/VM and reference/native ratios were **0.88× / 1.19×** for
`bus_fabric`, **0.90× / 1.14×** for `uart_loopback` and **1.04× / 1.28×**
for `gate_udp`. The elapsed measurements are specific to the fixed inputs and
this host. The VM retired **31%** more host instructions than Icarus on
`bus_fabric` and **22%** more on `uart_loopback`. Native cut both instruction
counts to within **1.1%** of Icarus on the same inputs. The native profiles of
the first two spent, respectively, **61%**
and **33%** in island/wait handling plus slot writes and notification.
The UART also executed 240 SIR instructions and processed 36 time slots per
frame. The gate/UDP case spent **82%** in island/wait handling plus writes and
notification, despite its lead over Icarus in native mode.

For `nba_shift`, NBA registration and identifiable application together
accounted for **44%** of native CPU samples, against **8%** in generated code.
For `comb_chain`, generated code accounted for **4%** and the interpreter for
**32%**. Its `initial0` drives the loop and has 32 SIR instructions per step.
`hierarchy_fanout.initial0` retains 1,811 SIR instructions per step even with
native. A separate run of all 28 workloads on `kirimasim-tests`
`06f00049d0ee` at `+n=200` matched VM stdout in all cases. Fourteen workloads
made zero native calls and recorded no generation rejection. Their hot
procedures are outside the current generator's input, including timed
`initial` and ordinary `always` processes. [Per-run data](../measurements/2026-11-27/p4-daily-profile.json)

A separate `initial` loop with 32-bit xorshift and accumulation used **130 host
instructions per executed SIR instruction** in both state modes. It had one
time slot at N=1 and N=2,000,000. The result uses the differences in host and
SIR instructions between these sizes. It measures that instruction mix,
including VM value and storage handling. [Interpreter control](../measurements/2026-11-27/p4-fused-controls.json)

## What the fused controls can establish

Each standalone C++ control produced the same final checksum as 2-state
native at N and 1. The shift uses a shadow state for register and accumulator
updates and swaps it at each simulated edge. The combinational chain runs
the eight stages in dependency order, with the same stimulus and checksum.
The Ibex control isolates the three 64-bit copy edges from `mcycle_counter_i`
(SIR process IDs 831, 832, 828 in that order) in a three-member island, with
synthetic inputs. Compilation used Apple Clang 21, `-O2 -ffp-contract=off
-fno-fast-math`. Five warm runs and startup subtraction give these local
measurements. The three controls do not model a general event queue or
observation at the intermediate signal boundaries.

| Control | 2-state native / C++ Δ (s) | Host instructions per increment, native / C++ |
| --- | ---: | ---: |
| `nba_shift`, N=1,000,000 | 2.396 / 0.00639 | 72,081 / 114 |
| `comb_chain`, N=3,000,000 | 0.956 / 0.00545 | 10,289 / 27 |
| Ibex counter copy chain, N=2,000,000 | 0.506 / 0.00365 | 7,639 / 6 |

These are local ceilings for the measured arithmetic and state transfers.
For example, the Ibex C++ compiler can eliminate the intermediate copies in
the isolated chain. The whole-Ibex speedup would be **at most about 2.74×**
in a simple additive model that removes *all* time in the sampled island/NBA
union while leaving every other cost fixed. A 2× reduction in that union
would give about **1.47×**. These estimates use fractions of CPU samples and
assume they represent elapsed simulation time. They neither predict the
speed of an implementation nor imply the synthetic controls' instruction
ratios apply to CoreMark. [Control data](../measurements/2026-11-27/p4-fused-controls.json)

## Reproduce and review

For prepared assets, the one-command replay below checks the pinned firmware
hash, every CRC and the final cycle count. Use `cva6` and its synthesis assets
for the second design. Replace `--reference` with `--verilator-args=PATH` to
build both 5.054 settings from a local JSON array of ordered source arguments.
The input list is interpreted relative to that file. The recorded source and
firmware hashes also allow checking existing P3 assets.

```sh
python3 tools/coremark_profile.py ibex --kirimasim build/release/src/driver/kirimasim \
  --sir "$IBEX_SIR" --firmware "$COREMARK_HEX" --reference "$IBEX_VTOP" \
  --out build/p4-ibex-replay --diagnostic
python3 tools/p4_profile.py --stdout build/p4-ibex-replay/diag-native-2.stdout \
  --tier build/p4-ibex-replay/native-2.jsonl --islands build/p4-ibex-replay/native-2.islands
```

For the stack profile, start a separate warm run with the same SIR, firmware and
state mode. Attach `sample <PID> 5 1 -mayDie -file stacks.sample` after startup and
pass that file to `p4_profile.py --sample`. The optional `--nba-sites` override
takes the verified offset list for the specific binary, recorded as
`nba_mapping.nba_offsets` in the CoreMark JSON for this build.

**Questions for the review.** The event-path hypothesis fits Ibex. Its own
testbench interpreter work accounts for a small share of CPU samples.
For `comb_chain` and `hierarchy_fanout` the timed testbench still matters.
CVA6 shows a third case, with most interpreter work in RTL that cannot reach
generated code.
Prioritizing solely by the number of generated functions would hide this
distinction.

**Not confirmed.** Instruction counts per function were not captured. The
per-function results are CPU stack samples, and the CVA6 stack profile spans four windows of a complete run. A native Linux x86-64 run with `perf` counters and profiles across
firmware phases would test these distributions on a second host. The separate
cost of a C ABI call, detached from the scheduler and value work it performs,
would require a controlled replacement that preserves the effects. The
standalone C++ control intentionally changes several of those costs at once.

**Human decision pending.** The existing acquisition/firmware/RTL preparation
recipe and adapters are local. The replay script is verified with prepared
inputs, and both reference builds were exercised. Confirm which parts of the
local recipe may be rewritten and shared before adding the public one-command
path from source checkout onward. No speed threshold or specification
amendment is part of this measurement round.
