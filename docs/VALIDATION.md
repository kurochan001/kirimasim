# Validation and Reproduction Guide

**P1 policy update (2026-11-01):** Icarus's `wire real` extension is optional.
Only the reviewed `real_net` source is out of scope: the current 176-probe inventory
has 175 P1-evaluated cases, one exclusion, two required-scope gaps and three raw gaps.
Unsupported results and independent faults remain recorded. Standard `real`
features retain their existing scope. See the [decision and checks](notes/p1-wire-real-exception.md).


[日本語](VALIDATION.ja.md) · The English version is authoritative when the versions differ.

**Approved combined checkpoint (2026-11-01):** class observers and partial inout ports now have their three approved expectation migrations applied. Full dev passes 11,299/11,299 without exclusions; the frozen 176-probe Icarus 13.0 comparison retains three gaps and 25 separate reference mismatches. See the [integration record](notes/p1-approved-class-inout-integration.md). Earlier checkpoint measurements below retain their dates and populations.


This guide explains which question each validation tool answers, how to
repeat a run and how to interpret its evidence. Commands and local smoke
examples were checked at source `2020305e` on **2026-10-30**. P1 remains in
progress. The agreed conditions are in [Test Suite](spec/TEST_SUITE.md) and
[P1 Scope](spec/P1_SCOPE.md); this guide adds no new acceptance criteria.

For investigating one failure, use [Debugging](DEBUGGING.md). For writing
an implementation and its tests, use [Development](DEVELOPMENT.md).

**Earlier recorded checkpoint (2026-11-01):** main `4fb5f6b1` passed all 21
local-CI steps (dev/ASan/UBSan 10,205 tests each, release 10,206). The
[145-probe Icarus 13 inventory](measurements/2026-11-01/icarus13-array-element-ports.json)
contains five passing-reference gaps, including two newly inventoried supply
forms; the previous 131 sources/oracles/native statuses are unchanged.
The [port note](notes/array-element-port-execution.md) separates four older
reference mismatches from eleven new optional-collapse readout differences.
These are recorded results, not a new run of sv-tests, population, 14.0 or
seven-night fuzzing. The command examples above/below retain their stated scope.


The subsequent [effective-kind measurement](measurements/2026-11-01/icarus13-port-effective-kinds.json)
uses implementation `afebe49f` and the same 145 frozen sources/oracles. Both
supply-inner gaps now pass, leaving three passing-reference gaps. All fifteen
reference mismatches remain separately recorded. Validation of the implementation
and the separately approved historical expectation migration is in the
[execution note](notes/port-kind-execution.md). The measured local implementation
`afebe49f` was rebased as `67a93af0` onto upstream `d0865eb6`; the port producer
and its tests are unchanged, and the upstream SIR text change preserves output.


The latest [156-probe comparison](measurements/2026-11-01/icarus13-converting-ports.json)
adds eleven converting-port cases to the unchanged 145-case inventory.
The frozen baseline has eight passing-reference gaps and the implementation
has three; six additional optional-collapse readout differences are retained
separately. See [conversion validation](notes/converting-port-execution.md).


The latest [171-case run](measurements/2026-11-01/icarus13-partial-inout-ports.json)
adds fifteen partial-inout probes. The baseline's fourteen passing-reference
gaps fall to three; four new wired readout differences are recorded separately
with their reference cause unconfirmed. Earlier sources/oracles and native
statuses remain unchanged. See [partial-inout validation](notes/partial-inout-port-execution.md).

## Keep the kinds of evidence separate

- **CTest / local CI:** checks the repository's unit, source, CLI, Context,
  counter and tooling tests, plus build and static policies. Success does
  not establish complete language coverage or the external comparison gates.
- **`valsem` and scheduler comparisons:** compare fast behavior with independent
  reference models for the tested values/scripts. Fixed-seed local tests
  and the required consecutive nightly runs are different evidence.
- **sv-tests simulation comparison:** compares pass counts on the fixed
  handwritten simulation population, separately on Linux and macOS.
- **Fixed P1 population:** requires at least 90% of the approved frozen list
  to pass the own-suite harness. Unsupported entries stay in the denominator.
- **Feature probes:** give a finite mechanism-by-mechanism comparison against
  written expectations. A clean probe report cannot establish full parity.
- **Mutation tests:** check whether tests detect deliberately compiled mistakes.
  A successful applicability check alone does not test that detection.
- **PerfCount:** detects operation-count regressions. For elapsed time and
  memory measurements, use [Benchmarks](BENCHMARKS.md).

A CTest entry may run a matrix of many designs. Counts may differ between
build presets. Do not interpret summed test counts as a unique language
feature count. The wider Icarus-required coverage target still applies even
when a frozen numerical population excludes a mechanism.

## Record enough information to repeat the run

Run from the repository root after a [development build](USER_MANUAL.md#build-and-version).
Use a fresh output directory when retaining several attempts:

```sh
mkdir -p build/validation-guide
git rev-parse HEAD > build/validation-guide/source-revision.txt
git status --short > build/validation-guide/worktree.txt
build/dev/src/driver/kirimasim --version > build/validation-guide/version.txt
```

The binary can be older than the checkout, so retain both identifiers.
Also record the preset, compiler and OS, ordered inputs, working directory,
options, seeds, test selection and the compared tool/corpus versions.
Keep stdout, stderr and the runner's exit code. A pipeline ending in `tee`
can obscure an earlier failure unless the shell preserves pipeline failures.
Review paths and input contents before sharing logs publicly.

## Run local tests and CI

Confirm that a focused selection actually contains tests, then run it:

```sh
ctest --preset dev -N \
  -R '^(W4Ops[.]CountBits|suite/values/aggregate_bit_query_record_fork_two_65)$'
ctest --preset dev --output-on-failure \
  -R '^(W4Ops[.]CountBits|suite/values/aggregate_bit_query_record_fork_two_65)$'
```

This selection contains two tests at the checked revision. For the full
pre-push checks required by the repository:

```sh
CTEST_PARALLEL_LEVEL=4 python3 tools/local_ci.py
```

Choose parallelism to fit the machine. The default run configures/builds/tests
dev, release, ASan and UBSan, then checks release flags, project policies,
unpushed commits, README counts, clang-format and clang-tidy. Read the summary
and the logs in `build/local-ci/`; the dev JUnit file is `ctest-dev.xml`.
Report any deliberately selected presets or omitted checks. Minimum-compiler,
nightly and external-comparison jobs need their own results.

### Read pass, failure, unsupported and skip correctly

For own-suite tests, [run_suite_test.py](../tools/run_suite_test.py) is the
judge. A source expected to be rejected can pass the test with design exit 2
and the required diagnostic. Conversely, design exit 0 alone may not satisfy
the expected stdout, stderr or other metadata.

Nightly's `slang-main` job explicitly configures
`-DKIRIMASIM_SUITE_DIAGNOSTICS=slang-main`. This passes
`--diagnostic-profile=slang-main` to the suite judge and selects the upstream
diagnostic for `proc/random_seed_not_a_variable` (slang
[PR #2076](https://github.com/MikePopoloski/slang/pull/2076)). It still requires
exit 2 and the exact file, line, column and diagnostic text. Every other test
uses its header unchanged. The default `pinned` profile retains all existing
expectations; this upstream compatibility check does not replace the fixed-version
P1 gates.

An unsupported construct is not a successful execution. In `tests/suite/`,
unsupported exit 2 is a permitted skip only when the test is listed in the
approved [skip list](../tests/suite/skips.txt); otherwise it fails. The
harness uses exit 77 for a skip. A listed test that starts passing must be
reviewed and removed from that list. External own-suite tests can be reported
as unsupported under the harness's separate population rules.

Inspect the actual assertions and metadata too: a test without an expected
output or a meaningful self-check provides less evidence about values.
Do not add skips, change a test body or weaken expected output to obtain a
pass. Follow [expectation-change rules](../CONTRIBUTING.md#changing-expected-results).

## Reproduce reference-model and ordering tests

The unit-test controls in [fuzz.h](../tests/unit/fuzz.h) are separate from
the simulator's `--seed`. A small reproducible example is:

```sh
KIRIMA_FUZZ_SCALE=1 KIRIMA_FUZZ_SEED=20261030 \
  build/dev/tests/unit/kirimasim_unit_tests --gtest_filter=W4Ops.CountBits
```

Record the original environment value as well as the per-test seed printed
on failure. The implementation mixes the supplied seed with a test-specific
constant; feeding the printed mixed seed back as the environment value
generates different cases. `KIRIMA_FUZZ_SCALE=0` does not disable testing;
the helper uses at least scale 1.

The [nightly workflow](../.github/workflows/nightly.yml) sets scale 100 for
the reference fuzzers using a UTC-date seed. To reproduce a selected date's
two reference groups after a release build:

```sh
KIRIMA_FUZZ_SCALE=100 KIRIMA_FUZZ_SEED=20261030 \
  build/release/tests/unit/kirimasim_unit_tests \
  --gtest_filter='W4Ops.*:Valsem.*:Scheduler.*:Heap.*'
KIRIMA_FUZZ_SCALE=100 KIRIMA_FUZZ_SEED=20261030 \
  build/release/tests/unit/kirimasim_unit_tests --gtest_filter='Wide.*'
```

Use the recorded date/seed and revision for the run being investigated.
One local success does not establish seven consecutive successful nightly
runs. Retain each dated result and its actual selection at the P1 judgment.
Unit-test shuffle/repeat jobs check state retained across tests in one
process. After the dev build, a three-repeat ordering recipe is:

```sh
build/dev/tests/unit/kirimasim_unit_tests \
  --gtest_shuffle --gtest_repeat=3 --gtest_random_seed=30
```

Replace 30 and the selection with the job's recorded values when reproducing
a failure. The GoogleTest random seed controls test order, not value-fuzzer
inputs or SystemVerilog random streams. See the workflow's sanitizer jobs.

## Preserve libFuzzer inputs across builds

The failure trace from `fuzzSetting()` now labels `mixed_seed` separately from
the original `KIRIMA_FUZZ_SEED` string, reports parsed scale, and prints a
shell-quoted replay command with the current unit-test binary and exact test
filter. An unset variable is replayed as unset; zero and permissively parsed
strings retain their original meaning. The seed mixing, fixed CI stream,
date seed / scale-100 Nightly and seven-night P1 condition are unchanged.
Record the binary hash and checkout revision alongside the trace.

Nightly restores **input files**, not verdicts, into `build/fuzz-new`, across
revisions of the existing `kirimasim_fuzz_compile` target. It first executes
each restored file once with the freshly built binary, then retains the
existing 30-minute mutation run. Replay findings remain report-only, as does
that mutation run; fixed-suite CTest still runs every input in batches of 64 to avoid OS argument-length limits. Each batch's exact argv is in
`fuzz-corpus-replay.log`. The before/after snapshots record every corpus hash,
current binary hash, revision and source-dirty flag. `fuzz-corpus` and the
separate original `fuzz-findings` artifact preserve observations even if a
subsequent build passes. The cache is a best-effort transport and can be
evicted; keep/download artifacts needed for an investigation.

For a downloaded corpus on Linux, after building the current fuzz preset:

```sh
mkdir -p build/fuzz-findings
python3 tools/fuzz_corpus_snapshot.py \
  --binary build/fuzz/tests/fuzz/kirimasim_fuzz_compile \
  --corpus build/fuzz-new --output build/corpus-replay-before.json -- \
  find build/fuzz-new -type f -print0
find build/fuzz-new -type f -print0 | \
  xargs -0 -r -n 128 -t build/fuzz/tests/fuzz/kirimasim_fuzz_compile \
    -timeout=30 -rss_limit_mb=4096 -artifact_prefix=build/fuzz-findings/ \
    > build/corpus-replay.log 2>&1
```

The `-r` option is GNU xargs; on macOS, invoke the binary with explicitly
selected file arguments (and avoid an empty list). A directory argument starts
mutation; positional file arguments replay once each. On code changes, rerun
using the current binary rather than accepting old results. Keep an original
crash input/log unchanged; triage specification, frontend/reference, runner and
implementation faults before adding a confirmed mechanism regression to unit
tests or the existing matrix. Do not weaken an expectation to obtain a pass.

External SystemVerilog input exploration and same-stimulus cross-simulator
checks belong to [kirimasim-tests](https://github.com/kurochan001/kirimasim-tests),
using its existing bounded random runner and manifests. Its random evidence is
separate from the fixed P1 population and qualified interaction coverage. This
repository retains internal valsem/scheduler comparisons and libFuzzer; no
external job-admission cache is applied to these gates.

## Run external-corpus comparisons

The following recipes require already prepared checkouts and executables.
Example `checkouts/...` paths are locations to replace with your own prepared
paths. Check the adopted revisions before running; ask before installing
tools or adding dependencies. Do not inspect Icarus/Verilator implementation
sources or import generated external test populations.

### sv-tests

Use the corpus and Icarus revision adopted by
[Test Suite](spec/TEST_SUITE.md#simulators-compared-against-fixed-versions).
The corpus pin is also `PINNED` in [svtests.py](../tools/svtests.py).

```sh
SVTESTS_CHECKOUT=checkouts/sv-tests
SVTESTS_IVERILOG=checkouts/icarus/bin/iverilog
git -C "$SVTESTS_CHECKOUT" rev-parse HEAD
"$SVTESTS_IVERILOG" -V
python3 tools/svtests.py --sv-tests "$SVTESTS_CHECKOUT" \
  --kirimasim build/release/src/driver/kirimasim \
  --iverilog "$SVTESTS_IVERILOG" --jobs 4 \
  --out build/validation-guide/svtests --gate
```

This creates `results.json` and `summary.md`. Check that both engines ran
on the whole intended population: `--gate` compares counts only when both
kirimasim and Icarus were supplied. A pin mismatch currently produces a
warning rather than enforcing the pin. A filtered or mismatched-corpus run
is an investigation, not the formal gate. Repeat the required measurement
on each supported CI OS; a result on one OS does not prove the other.

sv-tests uses its own expected-failure and `:assert:` rules, not the own-suite
stdout oracle. In particular, unsupported rejection does not satisfy a
kirimasim test expected to fail. Generated external populations are excluded.
`--parse-only --baseline tests/gates/svtests_parse.txt` measures another
population and warns on a drop; it is not the simulation gate and that
baseline warning does not make the runner fail.

### Frozen own-suite P1 population

Use the corpus revision recorded in the first line of
[p1_population.txt](../tests/gates/p1_population.txt), including its approved
expectations. The runner does not itself enforce the checkout revision.

```sh
P1_TESTS_CHECKOUT=checkouts/kirimasim-tests
git -C "$P1_TESTS_CHECKOUT" rev-parse HEAD
python3 tools/p1_population.py run --tests-repo "$P1_TESTS_CHECKOUT" \
  --kirimasim build/release/src/driver/kirimasim \
  --list tests/gates/p1_population.txt --jobs 4 \
  > build/validation-guide/p1-population.out
```

Read passed, failed and unsupported counts separately. The rate is passed
divided by the complete frozen list; unsupported entries are not removed.
The runner returns 1 below 90%, so exit 0 does not mean every entry passed.
The `select` command generates a proposed population; do not use it to
overwrite the approved list during validation. Changes to the list or its
expectations require the agreed review procedure.

## Compare a finite feature probe

With already installed Icarus compiler/runtime executables, this small run
selects one original probe and records both versions and judged output:

```sh
python3 tools/icarus_feature_probe.py \
  --kirimasim build/dev/src/driver/kirimasim --iverilog iverilog --vvp vvp \
  --only wide_integral --require-parity \
  --output build/validation-guide/wide-integral.json
```

Check raw `probes`, `gaps` and `reference_mismatches` alongside `p1_scope`,
`p1_out_of_scope`, `p1_required_gaps` and `independent_failures`. `gaps` preserves
all raw passing-reference gaps. The P1 gate uses required-scope gaps and
independent kirimasim faults: `--require-parity` returns 1 for either. Without
that flag, the tool can return 0 with gaps. `reference_failures` records reference
crashes/limits separately; these are not kirimasim defects or proof of parity.

Only the reviewed name **and source hash** can select the `wire real` exception.
Report total, evaluated and excluded counts, names and reasons separately. An
unsupported optional case stays failed/unsupported, never PASS. For its exit 2,
a separate compile-only `--dump-sir` check must confirm an ordinary positioned
compile error with no stdout; runtime errors, crashes, timeouts, output limits
and internal errors remain failures. Standard real variables, arithmetic, legal
single continuous assignments and `nettype real` keep their existing scope.

`--extended` adds finite mechanisms, not all combinations. The gate does not
establish that Icarus matches every written expectation or that P1 is complete.
See [the exception audit](notes/p1-wire-real-exception.md).

## Check negative controls and operation counts

The read-only mutation applicability check is:

```sh
python3 tools/mutants.py --check
```

It checks replacement anchors without compiling or injecting a mistake.
The current `--check` path checks the full catalogue even with `--only`.
Actual mutation runs rebuild and temporarily rewrite `src/`; run them in
a dedicated clean checkout, following CONTRIBUTING and the nightly recipe.
After configuring its dev preset, a selected mutation recipe there is:

```sh
python3 tools/mutants.py --preset dev --only countbits
```

`--only` is a case-sensitive name substring. `--shard 1/32` selects just one
catalogue partition; all partitions need results for whole-catalogue evidence.
Nightly uses 32 partitions after all eight previous jobs reached the six-hour
limit with 1,126 mutations (at most 36 per new partition). This changes scheduling,
not the catalogue or detection rules; hosted completion must still be checked.
The runner requires a passing unchanged baseline and restores each source.
A compiled mutation is caught by a failing test or a newly unsupported skip.
`DOES NOT APPLY`, `DOES NOT BUILD` and `SURVIVED` are problems, not successful
negative controls. Ensure the selected set is nonempty.

For operation counts:

```sh
ctest --preset dev -L PerfCount --output-on-failure
```

The checker rejects counts at least 10% above an existing budget, missing
or unexpected counter keys, and differing text counters. Read
[PerfCount maintenance](../tests/perf_counts/README.md) before proposing a
budget change. Do not raise expectations to conceal a regression. Counter
changes, timing measurements and correctness results are separate findings.

## Publish a scoped validation result

State the question, source/binary and corpus identifiers, commands, environment,
selection, seeds, pass/fail/unsupported/skip totals, exit status and artifact
locations. Separate reproduced facts, inferred causes and unconfirmed work.
Attach the local CI summary when pushing; do not describe checks that were
not run as successful.

This guide's local smoke selections, mutation applicability check and single
feature probe are command checks. It does not report new full external-gate,
mutation-execution or seven-nightly measurements. Use the dated
[P1 audit](P1_CRITERIA_GAPS_2026-10-28.md) and underlying records with their
original scopes; a current completion judgment needs the required fresh evidence.
For generated README counts and [dated validation records](VALIDATION_COUNTS.md), follow CONTRIBUTING and [readme_counts.py](../tools/readme_counts.py)
after reviewing the complete, unfiltered measurement artifacts.
