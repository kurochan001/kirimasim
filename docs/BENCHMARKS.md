# Performance measurements

[日本語](BENCHMARKS.ja.md) · This English text is the main version.

This record contains two separate tier 1 measurements: macOS on 2026-10-23
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
