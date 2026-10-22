# Performance measurements

[日本語](BENCHMARKS.ja.md) · This English text is the main version.

Remeasured on 2026-10-23 with the current tier 1 build and the comparison
simulators below. This supports the [README summary](../README.md); it does
not change performance acceptance criteria.

## Conditions

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
  projects' newest versions is not confirmed. No Linux or other-CPU
  measurements are included here.

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

- [bench.json](measurements/2026-10-23/bench.json): all five timing samples,
  medians, RSS, versions, load, options and SHA-256 hashes of the designs,
  script and kirimasim binary.
- [svtests.json](measurements/2026-10-23/svtests.json): per-test verdicts and times.
- [population.json](measurements/2026-10-23/population.json): per-test verdicts,
  times and SHA-256 hashes of the four runner files.

Set the variables below to the executable/checkouts at the versions above.
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
