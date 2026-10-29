# Debugging Guide

[日本語](DEBUGGING.ja.md) · The English version is authoritative when the versions differ.

Use this guide to reduce a failing design, locate the failing stage, inspect
SIR and collect a useful report. The examples were checked at source
`2020305e` on **2026-10-30**. They illustrate the current implementation,
including deliberately unsupported and bounded-loop examples.
For option definitions use the [Reference Manual](REFERENCE_MANUAL.md);
for judging a test run use [Validation](VALIDATION.md).

## Capture the original failure first

Save the exact command, ordered sources, top, includes, macros, data files,
working directory, preset and executable version. Record `--seed`,
`--init-values`, time precision, warning modes and resource limits too.
A checkout revision alone does not identify an older executable.

From the repository root after a [development build](USER_MANUAL.md#build-and-version):

```sh
mkdir -p build/debugging-guide
git rev-parse HEAD > build/debugging-guide/source-revision.txt
git status --short > build/debugging-guide/worktree.txt
build/dev/src/driver/kirimasim --version > build/debugging-guide/version.txt
```

Use fresh output locations when retaining multiple attempts. Save stdout,
stderr and the actual exit status before making changes. Banners and
diagnostics are normally on stderr, and a successful exit does not prove
that a requested waveform opened.

## Locate the failing stage

The existing [bit-query example](examples/development/bit_queries.sv) is a
small successful case for inspecting the pipeline:

```sh
build/dev/src/driver/kirimasim parse --top=bit_queries \
  docs/examples/development/bit_queries.sv \
  > build/debugging-guide/parse.out 2> build/debugging-guide/parse.err
build/dev/src/driver/kirimasim --dump-sir --top=bit_queries \
  docs/examples/development/bit_queries.sv \
  > build/debugging-guide/model.sir 2> build/debugging-guide/lower.err
build/dev/src/driver/kirimasim --top=bit_queries --seed=0 --init-values=x \
  --max-time=10ns --max-wall-time=5 --stats=json \
  --wave=build/debugging-guide/values.vcd \
  docs/examples/development/bit_queries.sv \
  > build/debugging-guide/run.out 2> build/debugging-guide/run.err
```

Each returns 0; design output is `ones=3 unknown=2 wide=2`, followed by the
JSON line in the last run. Check stages individually for the failing design:

- **Arguments or files fail:** inspect the actual argument spelling and
  working directory. Include paths, module libraries and runtime data paths
  have different roles. Start with [Porting](PORTING.md#translate-inputs-explicitly).
- **`parse` fails:** investigate preprocessing, types, elaboration, configs
  and frontend validation. This mode includes kirimasim-owned checks such
  as query validation; a parse failure alone does not identify a slang bug.
- **`parse` succeeds, SIR generation fails:** check the first positioned
  unsupported/lowering diagnostic and [P1 Support](P1_SUPPORT.md). Frontend
  acceptance does not establish executable support.
- **The verifier fails:** retain source, command and diagnostics as an
  internal representation problem. A failed `--dump-sir` invocation may
  leave an empty file rather than a valid dump.
- **SIR generation succeeds, execution fails or differs:** investigate the
  typed execution path, scheduling, lifetimes, inputs and termination reason.

Exit 2 can mean input, unsupported-source or design-runtime errors; exit 3
indicates an internal failure. `$fatal` and `--stop=error` produce exit 1.
Limits have their own codes. Use the complete
[exit reference](REFERENCE_MANUAL.md#termination-and-exit-codes), not a code
alone, to interpret the positioned message and the stage.

### A legal frontend input can still be unsupported

The self-written [object example](examples/debugging/unsupported_object.sv)
is intentionally not executable in the checked subset. Run these separately:

```sh
build/dev/src/driver/kirimasim parse --top=unsupported_object \
  docs/examples/debugging/unsupported_object.sv
```

Parsing returns 0. The following command is expected to return **2**:

```sh
build/dev/src/driver/kirimasim --dump-sir --top=unsupported_object \
  docs/examples/debugging/unsupported_object.sv \
  > build/debugging-guide/unsupported.sir 2> build/debugging-guide/unsupported.err
```

The diagnostics name unsupported `sample_box` storage and `NewClass`.
The file's existence is not proof of successful lowering. These are
documentation examples, not automatically registered suite tests; their
intentional failures must not be counted as successful simulation.

## Inspect the SIR rather than guessing the lowering

Search the successful dump:

```sh
rg -n '^(precision|source|slot|process)|syscall.*\$countbits' \
  build/debugging-guide/model.sir
```

The checked format-0 example contains entries of this shape:

```text
slot %s1 "bit_queries.wide" : i4<65> var inst=0
%t5 = syscall s2<32> "$countbits", %t4, const i4<1> 1'h1
```

Inspect the declared widths/state, variable slots, temporary values, control
flow and source comments. Here `$countones` was lowered to `$countbits`
with control 1; a changed source spelling need not appear in the dump.
The source table resolves the file index in location comments. Slot,
temporary and process numbers are local diagnostic IDs and can change.
`precision` is the global scheduling precision; `sim.end_time` uses its ticks.

The dump is not a stable interchange format and cannot be executed as a
compiled-model input. For a concrete implementation trace, see the
[Development Guide](DEVELOPMENT.md#trace-one-existing-function-through-the-implementation).

Read the final statistics line without treating all stdout as JSON:

```sh
python3 - <<'PY'
import json
from pathlib import Path
lines = Path('build/debugging-guide/run.out').read_text().splitlines()
stats = json.loads(lines[-1])
print(stats['sim.end_reason'], stats['sim.end_time'])
PY
```

For this example the result is `finish 1000` (1 ns at 1 ps precision).
Compilation failures need not produce runtime statistics or final cleanup.

## Investigate values and conditions

Check typed widths, signedness, conversions, initialization and driver
resolution before blaming arithmetic. Ordinary uninitialized four-state
variables start as X in the default mode; undriven ordinary nets can be Z.
Print `%b` / `%h` when decimal output hides the bit pattern.

The [X-condition example](examples/debugging/x_condition.sv) makes this visible:

```sh
build/dev/src/driver/kirimasim --top=x_condition --init-values=x --seed=0 \
  --cond-x=warn docs/examples/debugging/x_condition.sv
```

It returns 0 and prints `ready=x` then `branch=false`. The warning identifies
the `if` position. This is expected handling of an unknown condition, not
proof of a kernel defect. `--cond-x=warn` preserves values and branch choice;
`error` deliberately turns a reached normal-simulation report into a design runtime error.
Other initialization modes change the experiment. Record them instead of
using a mode that merely hides missing reset behavior.

Use `--bounds-check=warn` when investigating supported packed/fixed-array
accesses, and read the source-position counts. The instrumentation does not
cover every container operation. Invalid queue writes also have mandatory
warnings independent of this option. For aggregates and refs, inspect when
values/selectors were captured and which storage remains live across a wait.
Reduction should preserve the same width, X/Z and lifetime properties.

## Investigate timing, missing transitions and limits

Align source timescales and global precision. Decide whether the expected
observation is in Active, after an NBA, or in Postponed; module and program
processes use different region sets. `$display` and `$strobe` observe at
different points. A new self-written probe can help expose that difference;
rewriting an existing test's observation can hide a race and changes its oracle.

VCD normally records settled slot values. `--wave-glitches` exposes eligible
intermediate transitions; scope filtering and unsupported data can still
omit signals. `--report-pulses` adds positioned notes for transitions removed
by supported inertial-delay paths. It does not add unsupported timing features.
See [Waveform](spec/WAVEFORM.md) and the
[pulse example](P1_FEATURES.md#report-removed-pulses).

The [zero-time example](examples/debugging/zero_time.sv) deliberately has no wait.
This command is expected to return **5**:

```sh
build/dev/src/driver/kirimasim --top=zero_time --max-steps=1000 \
  --max-wall-time=5 --stats=json docs/examples/debugging/zero_time.sv \
  > build/debugging-guide/zero-time.out 2> build/debugging-guide/zero-time.err
```

It prints `final reached`; the final JSON has reason `zero_time_limit` and
end time 0. The diagnostic identifies the process that ran without waiting.
`--max-steps` applies to consecutive instructions per process, not the total
`vm.instrs` count, which also includes other and final work. A simulation-time
limit cannot by itself stop a loop that never advances time.

Investigate the reported limit before increasing it. Use simulation-time and
wall-clock bounds for different questions; wall-clock checks are safe-point
checks and do not forcibly interrupt blocked host operations. Resource-limit
exit 6 can indicate live-child or call/stack exhaustion. `--max-time-ok`
changes the code for an intentional time boundary, not the recorded reason
or the correctness of a result.

## Locate the owning component and investigate host failures

Follow the evidence to the relevant boundary:

- [driver](../src/driver/main.cpp): arguments, mode selection, banners, exit mapping.
- [front](../src/front/front.cpp), [query checks](../src/front/query_checks.cpp),
  [expression lowering](../src/front/lower_expr.cpp): frontend setup and SIR production.
- [SIR verifier](../src/sir/verify.cpp) and [text output](../src/sir/text.cpp):
  representation validity and its diagnostic view.
- [VM](../src/vm/vm.cpp): typed instruction execution and frames.
- [scheduler](../src/kernel/sim.cpp), [store](../src/kernel/store.cpp) and
  [heap](../src/kernel/heap.cpp): regions, values and lifetimes.
- [value reference](../src/valsem/value.cpp) and
  [scheduler reference tests](../tests/unit/scheduler_test.cpp): independent comparisons.

These are investigation starting points, not a diagnosis from an exit code.
Distinguish an unsupported form, a design assertion, a resource limit and a
host crash. Preserve sanitizer diagnostics and the first relevant stack trace.
To reproduce the positive example under sanitizers, with prepared build prerequisites:

```sh
cmake --preset asan
cmake --build --preset asan
build/asan/src/driver/kirimasim --version
build/asan/src/driver/kirimasim --top=bit_queries \
  docs/examples/development/bit_queries.sv
cmake --preset ubsan
cmake --build --preset ubsan
build/ubsan/src/driver/kirimasim --version
build/ubsan/src/driver/kirimasim --top=bit_queries \
  docs/examples/development/bit_queries.sv
```

A preset's old executable is not evidence for current source. A host signal
may appear as a negative return code in a Python subprocess and as a shell
signal status; retain how the result was captured. Ask before installing a
new diagnostic tool. External frontend issues need an independently reduced
case and evidence separating slang behavior from kirimasim's own checks.

## Reduce and report without changing the question

Remove unrelated logic incrementally and verify the failure after each change.
Preserve clocks, precision, input order, options, seeds, side effects, ref/automatic
lifetimes and observation region when they trigger the failure. Work on a new
self-written reproducer. Existing test bodies and expectations are protected
by [CONTRIBUTING](../CONTRIBUTING.md#changing-expected-results).

Report through the [bug form](../.github/ISSUE_TEMPLATE/bug_report.md), with
the minimal original sources and relative layout, exact command, source/binary
versions, build environment, stdout/stderr, exit status, expected behavior
and its basis. Use `$fatal` for a self-check that must fail; `$error` alone
does not force a failing kirimasim exit status.

Label observations, inferred causes and unconfirmed work separately.
The dated [failure analysis](kirimasim_failure_analysis_2026-10-28.md) and
[triage ledger](kirimasim_failure_triage_2026-10-28.md) show earlier cases;
reproduce them on the version being reported before treating them as current.
Use [Validation](VALIDATION.md) for rerun scope and evidence collection.
Do not include private designs, personal paths, copied standard text or
commercial-simulator output in a public report.
