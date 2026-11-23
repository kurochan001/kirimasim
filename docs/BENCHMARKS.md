# Performance measurements

[日本語](BENCHMARKS.ja.md) · This English text is the main version.

Elapsed time measures speed under the recorded conditions, while
[PerfCount](GLOSSARY.md#perfcount) measures operation counts without establishing
elapsed-time performance.

<a id="v020-daily25"></a>

## Release 0.2.0 and Icarus on 25 fixed workloads (macOS, 2026-11-23)

An Apple M5 Max running macOS 27.2.1 arm64 ran the 0.2.0 candidate (`f8e7118f7342`, AppleClang 21, Release build) in the default four-state VM with static islands. Icarus Verilog 14.0 devel (`1eba369`) compiled each input with `iverilog -g2012` before timing `vvp`. Both tools ran the same [25 fixed inputs](https://github.com/kurochan001/kirimasim-tests/blob/befffe9069ef17894f2559328d1b8494732917d3/bench/daily.json) used for 0.1.3. Source hashes and the expected output of every run were checked.

After a warmup at N=1 for each tool, N=1 and the listed N each ran five times, serially, rotating tool and input-size order. Δ is median T(N) minus median T(1). Each kirimasim run includes its front end. Speedup is Icarus Δ divided by kirimasim Δ. Values above 1× favor kirimasim. Initial and final one-minute load averages were 9.08 and 8.22.

| Workload | N | kirimasim Δ (s) | Icarus Δ (s) | Speedup |
|---|---:|---:|---:|---:|
| `casez_struct` | 5,000,000 | 1.0805 | 3.2804 | 3.036× |
| `comb_chain` | 3,000,000 | 2.0091 | 8.0834 | 4.023× |
| `dyn_queue` | 4,000,000 | 1.9826 | 5.5889 | 2.819× |
| `fork_join` | 4,000,000 | 2.9439 | 4.3098 | 1.464× |
| `func_call` | 1,500,000 | 3.6421 | 11.1386 | 3.058× |
| `gate_udp` | 400,000 | 5.2306 | 3.8731 | 0.740× |
| `mem_rw` | 4,000,000 | 2.6292 | 4.1364 | 1.573× |
| `nba_burst` | 500,000 | 0.0364 | 0.0911 | 2.505× |
| `nba_shift` | 1,000,000 | 9.4894 | 27.2609 | 2.873× |
| `proc_wait` | 1,000,000 | 4.0852 | 6.3177 | 1.546× |
| `string_fmt` | 500,000 | 1.5851 | 7.3408 | 4.631× |
| `wide_arith` | 1,500,000 | 1.4935 | 2.0947 | 1.403× |
| `fourstate_mix` | 1,000,000 | 0.3577 | 1.2957 | 3.623× |
| `fifo_stream` | 200,000 | 0.1952 | 0.2827 | 1.448× |
| `uart_loopback` | 20,000 | 0.1974 | 0.1782 | 0.903× |
| `bus_fabric` | 150,000 | 0.4824 | 0.4113 | 0.853× |
| `hierarchy_fanout` | 100,000 | 2.8490 | 4.4233 | 1.553× |
| `event_fanout` | 50,000 | 1.3405 | 1.6856 | 1.257× |
| `word_boundary` | 4,000,000 | 2.3948 | 5.6165 | 2.345× |
| `wide_divide` | 80,000 | 0.0680 | 0.4320 | 6.352× |
| `packed_slice` | 1,500,000 | 0.8300 | 2.0834 | 2.510× |
| `memory_scatter` | 5,000,000 | 0.8116 | 4.2304 | 5.212× |
| `clock_domains` | 20,000 | 0.0578 | 0.0631 | 1.091× |
| `fourstate_arith` | 100,000 | 0.0691 | 0.1396 | 2.020× |
| `resolved_bus` | 50,000 | 0.0875 | 0.2469 | 2.821× |

Inputs below 1.15× in the primary comparison were repeated 11 times per size. The repeat speedups were `gate_udp` 0.918×, `uart_loopback` 0.859×, `bus_fabric` 0.854×, `clock_domains` 1.106×.

<a id="p3-coremark"></a>

## P3 development builds on Ibex and CVA6 (2026-11-23)

The [RTL performance targets](spec/PERFORMANCE.md) are at least 10× Icarus speed in four-state mode and 0.5–1× single-threaded Verilator speed in two-state mode. The table gives **reference run time divided by kirimasim run time**. The last column gives the additional speedup kirimasim needs to reach the lower target on that input.

| Design and mode | kirimasim native | Reference | Relative speed | Speedup still needed |
| --- | ---: | ---: | ---: | ---: |
| Ibex, four-state | 134.46 s | Icarus 13.0, 541.97 s | 4.03× | 2.48× |
| Ibex, two-state | 136.18 s | Verilator 5.054, 1.057 s | 0.00776× | 64.4× |
| CVA6 synthesis view, two-state | 1,345.02 s | Verilator 5.054, 1.881 s | 0.00140× | 358× |

**Ibex.** On macOS arm64, `f9edd4bd` ran with `--tier=native` and either `--state=4` or `--state=2`. All four runs used the same CoreMark image with 10 iterations. All five CRCs and the final cycle count (4,099,842) matched. Each engine was timed once, after compilation and cache preparation. These are run-only observations. The five-run median, cold-build and time-to-first-evaluation comparisons remain open. This native mode already enabled islands before the VM default changed at `a4075801`.

**CVA6.** The `cv32a65x` synthesis view honors the upstream RTL's synthesis exclusions. At `8a533ad0`, kirimasim in both state modes and Verilator matched all five CRCs and 1,051,805 cycles for one CoreMark iteration. Icarus 13.0 rejected a struct-typed `localparam` in this configuration, so there is no four-state speed ratio. The full simulation RTL still encounters 59 unsupported concurrent-assertion diagnostics.

<a id="v013-daily25"></a>

## Release 0.1.3 and Icarus on all 25 daily workloads (macOS, 2026-11-21)

The release build of `v0.1.3` (`b5b4cef0`, AppleClang 21) and Icarus Verilog 14.0 devel (`1eba369`) ran on an Apple M5 Max with macOS 27.2.1 arm64. All 25 daily inputs from the [fixed manifest](https://github.com/kurochan001/kirimasim-tests/blob/befffe9069ef17894f2559328d1b8494732917d3/bench/daily.json) were included. Source hashes and expected outputs were checked for both tools. After a warmup at N=1 for each tool, both N=1 and the listed N ran five times, serially, with rotating tool and count order. Δ is the median T(N) minus the median T(1). Icarus times cover `vvp` execution after `iverilog -g2012` compilation. Speedup is Icarus Δ divided by kirimasim Δ. Other work shared the host. The initial and final one-minute load averages were 13.49 and 5.24.

| Workload | N | kirimasim Δ (s) | Icarus Δ (s) | Speedup |
|---|---:|---:|---:|---:|
| `casez_struct` | 5,000,000 | 1.5060 | 4.2073 | 2.794× |
| `comb_chain` | 3,000,000 | 5.6654 | 19.3356 | 3.413× |
| `dyn_queue` | 4,000,000 | 3.3978 | 8.9655 | 2.639× |
| `fork_join` | 4,000,000 | 3.8778 | 6.0864 | 1.570× |
| `func_call` | 1,500,000 | 2.9409 | 9.6267 | 3.273× |
| `gate_udp` | 400,000 | 1.5581 | 3.0940 | 1.986× |
| `mem_rw` | 4,000,000 | 1.6003 | 3.2921 | 2.057× |
| `nba_burst` | 500,000 | 0.0390 | 0.1002 | 2.569× |
| `nba_shift` | 1,000,000 | 6.6163 | 23.8698 | 3.608× |
| `proc_wait` | 1,000,000 | 2.6631 | 5.3295 | 2.001× |
| `string_fmt` | 500,000 | 1.4626 | 7.1688 | 4.902× |
| `wide_arith` | 1,500,000 | 1.4498 | 2.0440 | 1.410× |
| `fourstate_mix` | 1,000,000 | 0.3089 | 1.2826 | 4.152× |
| `fifo_stream` | 200,000 | 0.1592 | 0.2821 | 1.772× |
| `uart_loopback` | 20,000 | 0.1685 | 0.1764 | 1.047× |
| `bus_fabric` | 150,000 | 0.3775 | 0.4036 | 1.069× |
| `hierarchy_fanout` | 100,000 | 2.1429 | 4.4194 | 2.062× |
| `event_fanout` | 50,000 | 1.0219 | 1.7167 | 1.680× |
| `word_boundary` | 4,000,000 | 2.3880 | 5.6455 | 2.364× |
| `wide_divide` | 80,000 | 0.0766 | 0.4364 | 5.696× |
| `packed_slice` | 1,500,000 | 0.9302 | 2.1072 | 2.265× |
| `memory_scatter` | 5,000,000 | 0.7517 | 4.2345 | 5.633× |
| `clock_domains` | 20,000 | 0.0495 | 0.0625 | 1.263× |
| `fourstate_arith` | 100,000 | 0.0679 | 0.1413 | 2.081× |
| `resolved_bus` | 50,000 | 0.0874 | 0.2434 | 2.785× |

Inputs below 1.15× in the primary comparison were repeated 11 times per size. The repeat speedups were `uart_loopback` 1.015×, `bus_fabric` 1.027×.

## Simulation speed in 0.1.3 on macOS (2026-11-21)

On an Apple M5 Max running macOS arm64, the v0.1.2 tag, the 0.1.3 runtime candidate and Icarus Verilog 14.0 devel (`1eba369`) ran in rotating order. Each input size ran 11 times. The simulation-time delta Δ is the median T(N) minus the median T(1). Icarus timings cover `vvp` on the compiled input. Every run produced the expected output.

| Design | N | 0.1.2 Δ (ms) | 0.1.3 candidate Δ (ms) | Icarus Δ (ms) |
|---|---:|---:|---:|---:|
| `uart_loopback` | 20,000 | 225.6 | 166.5 | 172.7 |
| `bus_fabric` | 150,000 | 592.1 | 374.7 | 395.7 |
| `clock_domains` | 20,000 | 63.4 | 46.8 | 60.8 |

With N quadrupled and seven repetitions per size, the candidate deltas were 2.9%, 5.2% and 24.1% lower than Icarus, respectively.

## Simulation speed in 0.1.2 on macOS (2026-11-20)

Measured with Apple M5 Max, 18 cores and 128 GiB on macOS 27.2.1 arm64. The initial 1-, 5- and 15-minute load averages were 65.61/93.50/74.00. Local CI shared the host during the campaign. The release builds use AppleClang 21.0.0.21000401 with `-O3 -DNDEBUG -ffp-contract=off` and default simulation options. The candidate contains the runtime changes at `5eede4659338` and reports version 0.1.1 before the release metadata is applied. The baseline is `cb97fc93bf6b`, the `v0.1.1` tag is `26f359bcd504` and the starting `main` is `3159cb87b1d7`. Icarus Verilog is a pinned 14.0 devel build at `1eba369` using `iverilog -g2012` and `vvp`.

Each version ran each of the 25 daily inputs once at `+n=1` to warm up and five times at each count. The runs used one process at a time and alternated the orders of the counts and tools. Every run had the expected output line. Δ = median T(N) − median T(1). For Icarus, Δ uses only `vvp` runs, with compilation recorded separately. The geometric mean candidate ratio is **0.760** against `cb97fc93`, **0.525** against v0.1.1, **0.800** against the starting `main` and **0.484** against Icarus. The commit with timing results in its message is `af65d4ba4093` and has the same source tree.

| Design | +n | v0.1.1 Δ s | cb97fc93 Δ s | Starting main Δ s | 0.1.2 candidate Δ s | Icarus Δ s | Baseline / Icarus | Candidate / Icarus |
|---|---:|---:|---:|---:|---:|---:|---:|---:|
| `casez_struct` | 5,000,000 | 4.2237 | 2.1663 | 1.9517 | 1.7579 | 4.7776 | 0.453 | 0.368 |
| `comb_chain` | 3,000,000 | 10.7142 | 6.2330 | 6.0916 | 5.2849 | 12.4355 | 0.501 | 0.425 |
| `dyn_queue` | 4,000,000 | 7.6180 | 6.4992 | 4.5463 | 3.4582 | 11.5117 | 0.565 | 0.300 |
| `fork_join` | 4,000,000 | 15.5864 | 13.1726 | 11.6872 | 11.5496 | 17.3838 | 0.758 | 0.664 |
| `func_call` | 1,500,000 | 6.0014 | 4.7980 | 4.6721 | 3.6845 | 12.8827 | 0.372 | 0.286 |
| `gate_udp` | 400,000 | 5.1422 | 2.8383 | 2.8178 | 2.3790 | 3.7911 | 0.749 | 0.628 |
| `mem_rw` | 4,000,000 | 3.6730 | 2.4908 | 2.2793 | 1.9210 | 3.4271 | 0.727 | 0.561 |
| `nba_burst` | 500,000 | 0.1063 | 0.0732 | 0.0720 | 0.0560 | 0.1296 | 0.565 | 0.432 |
| `nba_shift` | 1,000,000 | 16.1566 | 9.6068 | 9.1581 | 6.4924 | 19.4045 | 0.495 | 0.335 |
| `proc_wait` | 1,000,000 | 6.5223 | 5.3395 | 5.0716 | 3.8601 | 5.8187 | 0.918 | 0.663 |
| `string_fmt` | 500,000 | 3.5945 | 2.8747 | 2.9491 | 1.8914 | 8.6957 | 0.331 | 0.218 |
| `wide_arith` | 1,500,000 | 2.0704 | 2.0038 | 2.0218 | 1.8483 | 2.4977 | 0.802 | 0.740 |
| `fourstate_mix` | 1,000,000 | 1.0040 | 0.7236 | 0.6988 | 0.4678 | 1.6068 | 0.450 | 0.291 |
| `fifo_stream` | 200,000 | 0.4791 | 0.3585 | 0.3418 | 0.2744 | 0.3552 | 1.009 | 0.773 |
| `uart_loopback` | 20,000 | 0.4457 | 0.3627 | 0.3430 | 0.2944 | 0.2177 | 1.666 | 1.352 |
| `bus_fabric` | 150,000 | 1.1791 | 0.9056 | 0.9653 | 0.7042 | 0.5128 | 1.766 | 1.373 |
| `hierarchy_fanout` | 100,000 | 7.9817 | 5.5130 | 5.0902 | 4.2016 | 5.4833 | 1.005 | 0.766 |
| `event_fanout` | 50,000 | 2.4575 | 2.1631 | 1.9997 | 1.6535 | 2.3116 | 0.936 | 0.715 |
| `word_boundary` | 4,000,000 | 4.8180 | 3.7858 | 3.7581 | 3.2903 | 7.3076 | 0.518 | 0.450 |
| `wide_divide` | 80,000 | 0.6868 | 0.0974 | 0.0945 | 0.0818 | 0.4862 | 0.200 | 0.168 |
| `packed_slice` | 1,500,000 | 1.6706 | 1.5584 | 1.5391 | 1.1148 | 2.4394 | 0.639 | 0.457 |
| `memory_scatter` | 5,000,000 | 2.9989 | 1.4088 | 1.3676 | 0.9548 | 5.1010 | 0.276 | 0.187 |
| `clock_domains` | 20,000 | 0.1256 | 0.1086 | 0.0946 | 0.0807 | 0.0771 | 1.409 | 1.047 |
| `fourstate_arith` | 100,000 | 0.1057 | 0.0940 | 0.0961 | 0.0811 | 0.1706 | 0.551 | 0.475 |
| `resolved_bus` | 50,000 | 0.2161 | 0.1579 | 0.1512 | 0.1117 | 0.2973 | 0.531 | 0.376 |

Icarus had a smaller Δ on 3 of 25 inputs (`bus_fabric` 1.37×, `uart_loopback` 1.35×, `clock_domains` 1.05×). The two Icarus ratio columns show how the ten inputs slower on the earlier Linux run compare on this Mac. Only this Mac campaign covers the candidate. Linux timing, an expanded workload and Callgrind instruction counts remain unmeasured.

`ScalarCode` grew from 48 to 56 bytes. `InstrInfo` is 88 bytes in both builds. The median across inputs of the `+n=1` medians is 10.75 / 11.06 ms for baseline / candidate. The 0.1.1 verified-SIR probe gives median load-and-empty-run time below. Peak RSS also includes design construction and verification.

| Instructions | Width | Baseline load ms | Candidate load ms | Baseline peak MiB | Candidate peak MiB |
|---:|---:|---:|---:|---:|---:|
| 101,001 | 32 | 6.94 | 7.75 | 41.8 | 42.8 |
| 101,001 | 64 | 6.68 | 6.99 | 41.8 | 42.8 |
| 101,001 | 65 | 4.68 | 5.32 | 36.6 | 36.6 |
| 404,001 | 32 | 27.39 | 30.64 | 155.2 | 159.4 |
| 404,001 | 64 | 27.01 | 29.46 | 155.2 | 159.4 |
| 404,001 | 65 | 19.35 | 20.74 | 134.4 | 134.7 |

The measured revision records the five-repetition deltas for `nba_shift` and `bus_fabric` in its [commit message](https://github.com/kurochan001/kirimasim/commit/af65d4ba4093).

## macOS speed improvements for 0.1.1 (2026-11-17)

Version 0.1.1 adopts word-based wide shifts and aligned selections, immutable operand-type caching, and integral/void instruction routing. The [detailed report](https://github.com/kurochan001/kirimasim-tests/blob/main/docs/simulation-speed-20261117.md) distinguishes the earlier full thirteen-input measurement from the final three-input comparison.

At `7f5a9134`, the thirteen-input time ratio has geometric mean 0.823 against Icarus and 0.774 against the unoptimized source; ten inputs have smaller time. At the final routing source `1745b8fc`, paired timing improves gate_udp 4.2%, mem_rw 10.0% and proc_wait 4.8% relative to 7f5a9134. Directly remeasured against Icarus:

| Design | 1745b8fc Δ s | Icarus Δ s | 1745/Icarus |
|---|---:|---:|---:|
| `gate_udp` | 3.815 | 2.600 | 1.467 |
| `mem_rw` | 3.039 | 2.781 | 1.093 |
| `proc_wait` | 4.975 | 5.093 | 0.977 |

Only proc_wait has a smaller median Δ among these three. The final source has no new thirteen-input timing score. These Mac measurements are provisional with concurrent local CI, five repetitions per count/tool, Δ = median T(N) − median T(1). Linux performance/RSS remeasurement of the adopted source remains deferred. No performance acceptance criterion changes.

On the measured Mac, the first type cache grows InstrInfo from 72 to 88 bytes (about +6 MiB RSS on a 404,001-instruction probe); the routing bit stays within 88 bytes. The additional routing load probe is about 11 ms and 134 MiB for both sources. The detailed report retains raw samples, spread, hashes and separate campaign conditions. Historical measurements below retain their original scope.

The historical record below contains two separate tier 1 measurements: macOS on 2026-10-23
at `dec94ff21979`, and [Linux (WSL2) on 2026-10-25](#development-versions-on-linux-wsl2-2026-10-25)
at `b66834254c65`, on eleven designs with the newest development versions of
Icarus Verilog, Verilator and Lyra. It supports the
[README summary](../README.md) without changing performance acceptance criteria.

The execution-control options in [P1 features](P1_FEATURES.md), container
integration, repeated event controls, aggregate capture/timing and wide gate-mask changes were added
after the macOS runs. The Linux run uses a later simulator revision;
the macOS and short test-suite times below have not been refreshed. The latest
[functional validation](notes/p1-icarus-progress.md) and
[Icarus 13.0 feature comparison](notes/icarus-13-comparison.md) use newer
sources; neither is itself a timing or memory measurement.

## macOS conditions (2026-10-23)

- kirimasim 0.0.0, commit `dec94ff21979`, release build, `-O3 -DNDEBUG`,
  `-ffp-contract=off`, AppleClang 21.0.0.21000334. Default runtime options,
  including `--bounds-check=off`. Uncommitted documentation and measurement
  tooling changes do not affect the simulator binary.
- Icarus Verilog 14.0 devel, `s20260301-541-gb25510638`.
- Verilator 5.053 devel, `v5.052-349-g2cb54975a`.
- Apple M5 Max, 18 cores, 128 GiB, macOS 27.0.1 arm64. Pre-benchmark load
  averages (1/5/15 minutes): 2.37/2.54/3.26. Benchmarks, sv-tests and the
  own suite were run sequentially, without overlapping each other. OS
  background processes were not stopped.
- These are locally available comparison builds; whether they are the
  projects' newest versions is not confirmed. The tables below up to
  [Development versions on Linux](#development-versions-on-linux-wsl2-2026-10-25) are
  these macOS runs only; that section is a separate measurement on another
  machine with other tool versions, and its numbers are not mixed with these.

## Simulation time

[tools/bench.py](../tools/bench.py) warmed each design/tool once, then
alternated five runs with `+n=1` and five at the stated count. Times below
are the large-run median less the small-run median, in seconds. Expected
results were unchanged. All 60 measured large runs had their output and
exit status checked; all matched.

| Design | `+n=` | kirimasim | Icarus Verilog | Verilator |
| --- | --- | --- | --- | --- |
| `comb_chain` | 3,000,000 | 5.043 s | 7.968 s | 0.142 s |
| `nba_shift` | 1,000,000 | 11.273 s | 18.739 s | 0.120 s |
| `proc_wait` | 1,000,000 | 3.966 s | 5.204 s | 1.817 s |
| `fork_join` | 4,000,000 | 2.763 s | 4.084 s | 0.843 s |

For these four designs kirimasim was about 1.31–1.66 times faster than
Icarus; Verilator was about 2.2–93.9 times faster than kirimasim. Four-state
interpretation and two-state C++ generation have different execution models.
These results alone do not establish general real-design performance or
an improvement over earlier versions, and do not resolve issue #28.

### Small-run startup-inclusive time (ms)

Medians of five `+n=1` runs, including kirimasim's reading and elaboration.

| Design | kirimasim | Icarus Verilog | Verilator |
| --- | --- | --- | --- |
| `comb_chain` | 8.067 | 6.426 | 3.164 |
| `nba_shift` | 7.714 | 6.266 | 3.107 |
| `proc_wait` | 8.353 | 7.272 | 3.445 |
| `fork_join` | 7.402 | 6.187 | 3.168 |

### Design build time (seconds, once per design)

Icarus uses `iverilog -g2012`; Verilator uses `--binary --timing`, its
default C++ settings and `--build-jobs 0`. The script and raw data give the
warning options too. kirimasim has no separate design build; preparation
occurs within a run. A clean build of kirimasim itself was not timed.

| Design | kirimasim | Icarus Verilog | Verilator |
| --- | --- | --- | --- |
| `comb_chain` | — | 0.023 | 1.581 |
| `nba_shift` | — | 0.014 | 1.528 |
| `proc_wait` | — | 0.015 | 1.641 |
| `fork_join` | — | 0.014 | 1.542 |

### Peak RSS (MiB)

The median of the per-process peak RSS across five large runs, not the
maximum over all samples.

| Design | kirimasim | Icarus Verilog | Verilator |
| --- | --- | --- | --- |
| `comb_chain` | 9.19 | 5.34 | 2.00 |
| `nba_shift` | 9.19 | 5.50 | 2.00 |
| `proc_wait` | 10.09 | 5.86 | 2.16 |
| `fork_join` | 9.19 | 5.33 | 2.06 |

## Development versions on Linux (WSL2), 2026-10-25

Measured on 2026-10-25, 10:55–11:42 UTC, on eleven designs: the four above
and seven written for other mechanisms, with the same method, by
[tools/bench.py](../tools/bench.py), each tool at its newest development
commit of that day. It is a separate measurement from the macOS one above:
another machine, and other tool versions.

- kirimasim 0.0.0, commit `b66834254c65` (main), release build
  (`-O3 -DNDEBUG`), clang 20.1.8.
- Icarus Verilog 14.0 devel, `2b8d42dc088f` (master), built with the steps of
  `tools/build_icarus.sh`.
- Verilator 5.053 devel, `1838b6ed035c` (master), built with configure's
  defaults; the generated C++ is compiled with `ccache g++ -Os` (g++ 15.2.0)
  and linked with mold.
- Lyra at `63922502ff0a` (main), built with `bazel build -c opt`. Its default
  backend is the C++ backend (a program built without `--backend` is the same
  file as one built with `--backend cpp`); the LLVM backend, which Lyra's
  README calls the main one, is a column of its own. Both at Lyra's default
  `-O 0`, each design built once into an empty cache; designs are compiled
  with clang 20.1.8.
- AMD Ryzen 9 7950X (32 logical CPUs under WSL2), 110 GiB, Ubuntu 24.04.4 LTS
  on WSL2, kernel 6.18.33.1, glibc 2.39. The runs started once the 1-minute
  load average had stayed under 1.2 with no other build or test running;
  0.22–1.15 during them. The benchmark runs were sequential; these load
  observations do not establish the absence of background work.

The seven new designs, under [bench/](../bench/), each print one checksum
line that kirimasim, Icarus Verilog and Verilator printed alike at its count
when it was added; none depends on X or Z, so two-state Verilator agrees:

| Design | What it does | `+n=` |
| --- | --- | --- |
| `wide_arith` | 256-bit additions, a 128-bit product, shifts and compares in one loop | 1,500,000 |
| `mem_rw` | A 1024-word memory written and read at moving addresses on every clock edge | 4,000,000 |
| `func_call` | A task with `inout` and `output` arguments calling a function that recurses four deep | 1,500,000 |
| `string_fmt` | `$sformatf`, `substr`, concatenation and a hash of every character | 500,000 |
| `dyn_queue` | A queue fed and drained around a dynamic array | 4,000,000 |
| `gate_udp` | 32 xor gates and 32 instances of a combinational UDP | 400,000 |
| `casez_struct` | `casez` decoding into a packed struct and a register file | 5,000,000 |

Seconds (large-run median less the `+n=1` median). All 265 large runs that
were built had exit status 0 and the expected line, and every `+n=1` run
printed `n=1`. Lyra does not support gates or UDPs yet, so it built no
program for `gate_udp`.

| Design | `+n=` | kirimasim | Icarus Verilog | Verilator | Lyra (C++ backend, default) | Lyra (LLVM backend) |
| --- | --- | --- | --- | --- | --- | --- |
| `comb_chain` | 3,000,000 | 9.249 s | 9.796 s | 0.223 s | 31.915 s | 2.878 s |
| `nba_shift` | 1,000,000 | 25.486 s | 26.585 s | 0.196 s | 140.711 s | 5.900 s |
| `proc_wait` | 1,000,000 | 6.786 s | 5.688 s | 2.283 s | 73.445 s | 6.164 s |
| `fork_join` | 4,000,000 | 4.837 s | 5.023 s | 1.190 s | 15.826 s | 4.860 s |
| `wide_arith` | 1,500,000 | 4.944 s | 2.036 s | 0.086 s | 6.236 s | 0.380 s |
| `mem_rw` | 4,000,000 | 4.937 s | 3.645 s | 0.696 s | 23.079 s | 2.705 s |
| `func_call` | 1,500,000 | 4.917 s | 7.511 s | 0.005 s | 7.887 s | 0.395 s |
| `string_fmt` | 500,000 | 5.003 s | 8.080 s | 0.135 s | 18.770 s | 0.279 s |
| `dyn_queue` | 4,000,000 | 4.112 s | 6.686 s | 0.083 s | 16.275 s | 0.412 s |
| `gate_udp` | 400,000 | 5.378 s | 3.517 s | 0.046 s | not supported | not supported |
| `casez_struct` | 5,000,000 | 3.976 s | 4.117 s | 0.018 s | 22.159 s | 0.466 s |

- The spread of the five large runs was at most 7.8% of their median, but
  14.7% for Verilator on `comb_chain`.
- kirimasim was slower than Icarus Verilog on `proc_wait` (1.19 times),
  `wide_arith` (2.4 times), `mem_rw` (1.35 times) and `gate_udp` (1.53
  times); faster on `func_call` (1.53 times), `string_fmt` (1.61 times) and
  `dyn_queue` (1.63 times); and within about 6% on the other four. Verilator
  was faster than kirimasim on every design. Lyra's default backend was
  slower than kirimasim on every design it built; its LLVM backend was faster
  on all of them but `fork_join`, where the two were within 1%.
- kirimasim at `b66834254c65` was slower than at `8a8607690aa6`, measured the
  same morning (`comb_chain` 9.249 s against 8.288 s), while Icarus Verilog
  and Verilator kept their times. Run alternately three times each, the later
  commit took 1.08 times as long on `comb_chain` and `proc_wait` and 1.14
  times on `fork_join`. A bisection on `fork_join` puts the change at
  `b17234e420b6` (4.26 s at its parent, 4.96 s with it).
- On `dyn_queue`, kirimasim's peak memory grows with the count: GNU time gave
  44 MiB at 100,000, 164 MiB at 1,000,000 and 541 MiB at 4,000,000, about
  130 bytes per iteration, while the queue never holds more than 33 elements.
  Why is not confirmed.
- `+n=1` runs took 1–6 ms for every tool. Builds, timed apart: Icarus
  0.005–0.024 s; Verilator 0.34–1.06 s with ccache; Lyra 5.4–6.3 s (default)
  and 0.44–0.50 s (LLVM backend) into an empty cache.
- Sampled RSS high-water values (median of the five large runs, MiB):
  kirimasim 21.6–25.3 and 547.5 on `dyn_queue`, Icarus 7.7–8.2, Verilator
  1.4–3.9, Lyra 4.3–4.8 (default) and 5.2–5.7 (LLVM). On Linux,
  tools/bench.py samples each program's `VmHWM` every 10 ms. A peak near
  process exit can fall after the last sample, so these are observed
  high-water values, not guaranteed final peak RSS; Verilator's 1.4 MiB on
  `func_call`, a run of a few milliseconds, is such a sample. `ru_maxrss` of a
  child also counts the Python process it was started from, so earlier Linux
  numbers, including the nightly job's before the sampling, cannot be below
  about 19 MiB. The sampling breaks direct comparison with that earlier Linux
  RSS history; macOS still uses `ru_maxrss`.
- An earlier run the same morning, on the four first designs with kirimasim
  `8a8607690aa6`, is kept as raw data in
  [measurements/2026-10-25-wsl2/bench.json](measurements/2026-10-25-wsl2/bench.json).
- Raw data: [bench.json](measurements/2026-10-25-wsl2-designs/bench.json).

### Reproducing the Linux comparison

Use the executable versions and build settings listed above. Set `KIRIMASIM`,
`IVERILOG`, `VERILATOR` and `LYRA` to their executable paths; `vvp` must be
next to `iverilog`. `LYRA_COMMIT` is the full commit of the Lyra build
(`63922502ff0a0cdc8d162009f73abc11b2401415` for this record). If
`VERILATOR_ROOT` is set for another Verilator, unset it. Run this separately
from other measurements:

```sh
env -u VERILATOR_ROOT python3 tools/bench.py --kirimasim "$KIRIMASIM" \
  --iverilog "$IVERILOG" --verilator "$VERILATOR" \
  --lyra "$LYRA" --lyra-llvm "$LYRA" --lyra-version "$LYRA_COMMIT" \
  --runs 5 --out build/bench-linux
```

`--lyra` uses the executable's default backend; `--lyra-llvm` explicitly
selects LLVM. `--lyra-version` records the supplied commit because this Lyra
executable has no `--version`. Building Lyra itself with `-c opt` does not
change the design's default `-O 0` used by these commands.

The tool writes `results.json` and `summary.md` even when a design fails and
exits 1 if any result fails; a failed design keeps its `stage` and
`diagnostics` and is not a zero-second run. Successful records keep the
warmup, five small and five large samples, output lines, exit status and
timeout flags. On Linux, `max_rss_mib` is a sampled `VmHWM` value; an
unobserved value is `null`, not zero. `bench_report.py` still reads this same
key and does not separate historical RSS acquisition methods automatically;
interpret nightly RSS comparisons across the method change accordingly.

The published JSON preserves the original samples; rerunning creates new
measurements. Use the recorded simulator revisions to reproduce this record;
using a newer revision measures that revision instead. Do not use `--n` for
the published comparison: it overrides the work counts and disables the
full expected-checksum check.

## Short test-suite times

The [sv-tests runner](../tools/svtests.py) and
[own-population timing tool](../tools/measure_population_times.py) ran each
test once, with up to six concurrent tests. Totals sum per-test durations;
they are not elapsed suite times. Startup and preparation dominate these
short tests, so these are separate from the simulation-speed comparison.

- sv-tests: 344 cases at pinned `15490e95fd978ada1c1dc59b7c4b53a7edb23b78`.
  In these runs kirimasim passed 213 and Icarus 210.
- Own P1 population: 1,466 cases from frozen kirimasim-tests `179c55de9c83`.
  Runners were copied and fixed at `bc1609dc0055`. kirimasim passed 1,451;
  Icarus passed 1,274. Existing runners judged the results; no expectations
  changed.
- kirimasim preparation is `--dump-sir`; running is whole duration less
  preparation, clamped to zero. Icarus preparation is compilation; running
  starts its program. The own-suite runner invokes `vvp` with the program.
  Normalization and Python judging time are excluded.
- kirimasim's difference includes noise and SIR-printing cost. Verilator's
  per-test rebuild time was not measured. The historical acceptance-count
  comparison tables were not refreshed by this timing run.

| Test set | Simulator | Build step | Run step | Total |
| --- | --- | --- | --- | --- |
| sv-tests, 344 | kirimasim | 7.06 s | 0.18 s | 7.24 s |
|  | Icarus Verilog | 4.79 s | 33.52 s | 38.31 s |
| Own P1 population, 1,466 | kirimasim | 11.65 s | 1.15 s | 12.80 s |
|  | Icarus Verilog | 21.54 s | 9.15 s | 30.70 s |

For tests **both measured tools passed**, the subset is below. It differs
from the earlier three-way intersection. The 184 sv-tests cases include
43 expected rejections. Totals are computed before rounding, so displayed
build plus run may differ from the total by 0.01 s.

| Test set | Simulator | Build step | Run step | Total |
| --- | --- | --- | --- | --- |
| sv-tests, 184 | kirimasim | 1.70 s | 0.09 s | 1.79 s |
|  | Icarus Verilog | 2.51 s | 27.22 s | 29.73 s |
| Own P1 population, 1,269 | kirimasim | 10.06 s | 1.00 s | 11.06 s |
|  | Icarus Verilog | 18.75 s | 9.04 s | 27.79 s |

## Raw data and reproduction

- [Linux bench.json](measurements/2026-10-25-wsl2-designs/bench.json): the eleven
  designs and five tool/backend columns of 2026-10-25, every run and the build
  commands. Reproduction is described in the Linux section above. The earlier
  Linux runs, of the four first designs, are kept in
  [measurements/2026-10-25-wsl2/bench.json](measurements/2026-10-25-wsl2/bench.json)
  (the same morning, kirimasim `8a8607690aa6`) and
  [measurements/2026-10-24-wsl2/bench.json](measurements/2026-10-24-wsl2/bench.json)
  (older tool versions).
- [bench.json](measurements/2026-10-23/bench.json): all five timing samples,
  medians, RSS, versions, load, options and SHA-256 hashes of the designs,
  script and kirimasim binary.
- [svtests.json](measurements/2026-10-23/svtests.json): per-test verdicts and times.
- [population.json](measurements/2026-10-23/population.json): per-test verdicts,
  times and SHA-256 hashes of the four runner files.

The commands below reproduce the older macOS and short test-suite measurements.
Set the variables to the executable/checkouts at those recorded versions.
`TESTS_REPO` is a kirimasim-tests clone containing `179c55de9c83`;
`RUNNERS_REPO` is kirimasim-tests at `bc1609dc0055`. `SV_TESTS` is the pinned
sv-tests checkout including its UVM submodule. Run the measurements
sequentially.

```sh
cmake --preset release
cmake --build --preset release
python3 tools/check_release_flags.py
python3 tools/bench.py --kirimasim build/release/src/driver/kirimasim \
  --iverilog "$IVERILOG" --verilator "$VERILATOR" --runs 5 --out build/bench
python3 tools/svtests.py --sv-tests "$SV_TESTS" \
  --kirimasim build/release/src/driver/kirimasim --iverilog "$IVERILOG" \
  --jobs 6 --out build/suite-times/svtests
mkdir -p build/population
git -C "$TESTS_REPO" archive 179c55de9c83 > build/population.tar
tar -xf build/population.tar -C build/population
python3 tools/measure_population_times.py --tests-repo build/population \
  --runner-repo "$RUNNERS_REPO" --kirimasim build/release/src/driver/kirimasim \
  --iverilog "$IVERILOG" --jobs 6 --out build/suite-times/population.json
```
