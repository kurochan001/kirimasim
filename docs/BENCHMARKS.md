# Performance measurements

[日本語](BENCHMARKS.ja.md) · This English text is the main version.

This record contains two separate tier 1 measurements: macOS on 2026-10-23
at `dec94ff21979`, and [Linux (WSL2) on 2026-10-25](#development-versions-on-linux-wsl2-2026-10-25)
at `8a8607690aa6`, with the newest development versions of Icarus Verilog,
Verilator and Lyra. It supports the
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
  including bounds checking off. Uncommitted documentation and measurement
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

Measured on 2026-10-25, 08:09–08:41 UTC, with the same four designs, counts
and expected results, by [tools/bench.py](../tools/bench.py), each tool at
its newest development commit of that day. It is a separate measurement from
the macOS one above: another machine, and other tool versions.

- kirimasim 0.0.0, commit `8a8607690aa6` (main), release build
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
  0.33–1.21 during them. The benchmark runs were sequential; these load
  observations do not establish the absence of background work.

Seconds (large-run median less the `+n=1` median). All 100 large runs had
exit status 0 and the expected line, and every `+n=1` run printed `n=1`.

| Design | `+n=` | kirimasim | Icarus Verilog | Verilator | Lyra (C++ backend, default) | Lyra (LLVM backend) |
| --- | --- | --- | --- | --- | --- | --- |
| `comb_chain` | 3,000,000 | 8.288 s | 9.838 s | 0.218 s | 31.399 s | 2.821 s |
| `nba_shift` | 1,000,000 | 22.459 s | 25.899 s | 0.195 s | 139.227 s | 5.706 s |
| `proc_wait` | 1,000,000 | 6.313 s | 5.571 s | 2.190 s | 72.489 s | 5.990 s |
| `fork_join` | 4,000,000 | 4.162 s | 4.973 s | 1.177 s | 15.492 s | 4.776 s |

- The spread of the five large runs was 0.4–7.6% of their median, but 10.5%
  for kirimasim and 15.5% for Lyra's LLVM backend on `nba_shift`.
- Here kirimasim was about 1.15–1.19 times faster than Icarus Verilog on three
  designs and about 1.13 times slower on `proc_wait`. Verilator was about
  2.9–115 times faster than kirimasim. Lyra's default backend was about
  3.7–11.5 times slower than kirimasim; its LLVM backend was about 2.9 times
  faster on `comb_chain`, about 3.9 times on `nba_shift` (where both spreads
  are wide) and about 1.05 times on `proc_wait`, and about 1.15 times slower
  on `fork_join`.
- `+n=1` runs took 2–6 ms for every tool. Builds, timed apart: Icarus
  0.004–0.025 s; Verilator 0.32–0.37 s with a warm ccache, and 2.82–3.01 s
  once each with ccache turned off; Lyra 5.9–6.4 s (default) and 0.45–0.51 s
  (LLVM backend) into an empty cache.
- Sampled RSS high-water values (median of the five large runs, MiB):
  kirimasim 21.5–25.4, Icarus 7.8–8.4, Verilator 3.8–4.0, Lyra 4.5–4.9
  (default) and 5.1–5.8 (LLVM). On Linux, tools/bench.py samples each
  program's `VmHWM` every 10 ms. A peak near process exit can fall after the
  last sample, so these are observed high-water values, not guaranteed final
  peak RSS. `ru_maxrss` of a child also counts the Python process it was
  started from, so earlier Linux numbers, including the nightly job's
  before the sampling, cannot be below about 19 MiB. The sampling breaks
  direct comparison with that earlier Linux RSS history; macOS still uses
  `ru_maxrss`.
- The tool versions and kirimasim's commit differ from the 2026-10-24 Linux
  run, so its numbers are not a change of any one tool.
- Raw data: [bench.json](measurements/2026-10-25-wsl2/bench.json).

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

- [Linux bench.json](measurements/2026-10-25-wsl2/bench.json): the five tool/backend
  columns of 2026-10-25, every run and the build commands. Reproduction is
  described in the Linux section above. The earlier Linux run of 2026-10-24,
  with older tool versions, is kept in
  [measurements/2026-10-24-wsl2/bench.json](measurements/2026-10-24-wsl2/bench.json).
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
