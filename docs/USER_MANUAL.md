# kirimasim User Manual

[日本語](USER_MANUAL.ja.md) · The English version is authoritative when the two versions differ.

This manual covers building kirimasim, running SystemVerilog designs, collecting
waveforms and diagnosing runs. The first simulation example was checked with
version `0.0.0` at `f86b940f9f6f` on 2026-10-29. Later options are covered by
the repository's CLI regressions. Save the output of `--version` for the build
used to reproduce a result.

For lookup by option, system-task family or exit code, use the
[Reference Manual](REFERENCE_MANUAL.md). This user manual provides the setup
and step-by-step workflow.

For an existing simulator workflow, use the [Porting Guide](PORTING.md).
The [Documentation Guide](README.md) maps the remaining documents by purpose.

kirimasim is in early development. P1, the first execution tier, is still in
progress; it is not ready for production use. Designs currently execute through
the SIR interpreter and a shared simulation kernel. C++ generation and reusable
compiled models are not available yet.

## Contents

- [Build and version](#build-and-version)
- [Run your first simulation](#run-your-first-simulation)
- [Choose inputs and execution modes](#choose-inputs-and-execution-modes)
- [Set macros and runtime arguments](#set-macros-and-runtime-arguments)
- [Control simulation time](#control-simulation-time)
- [Control execution and resource limits](#control-execution-and-resource-limits)
- [Record VCD waveforms](#record-vcd-waveforms)
- [Collect output and statistics](#collect-output-and-statistics)
- [Use system tasks and data files](#use-system-tasks-and-data-files)
- [Interpret exit codes](#interpret-exit-codes)
- [Troubleshoot a design](#troubleshoot-a-design)
- [Find support and report problems](#find-support-and-report-problems)

## Build and version

Run the commands below in a POSIX shell on Linux or macOS. You need:

- Git and network access for the initial source and dependency downloads.
- CMake 3.28 or later, Ninja, and Python 3.10 or later.
- A supported C++23 compiler: GCC 13 or later; Clang 18 or later with libc++
  18 or later; Clang 19 or later with libstdc++ 13 or later; or Apple clang
  17 or later with Xcode 26. Clang 17/18 with libstdc++ cannot build the
  pinned slang dependency as C++23.
- zlib and bzip2 development headers and libraries. On Debian/Ubuntu the
  packages are `zlib1g-dev` and `libbz2-dev`; on macOS the build uses the
  Command Line Tools SDK.

```sh
git clone https://github.com/kurochan001/kirimasim.git
cd kirimasim
cmake --preset release
cmake --build --preset release
ctest --preset release --parallel 4
export KIRIMASIM="$PWD/build/release/src/driver/kirimasim"
"$KIRIMASIM" --version
"$KIRIMASIM" --help
```

If you already have a checkout, start with `cmake --preset release` in its
root directory. `KIRIMASIM` is a shell variable used by this manual; kirimasim
does not read it as a configuration setting. The absolute executable path lets
you change working directories without changing the variable.

The first configure fetches the pinned slang, GoogleTest and standalone libfst
dependencies. Preparing libfst does not enable FST waveform output. See
[FST dependency setup](notes/fst-dependency-setup.md) for build details.

Use `release` for ordinary runs, `dev` for debugging, or `asan` / `ubsan` for
sanitizer checks. Each preset has its own `build/<preset>/` directory; the
executable is at `build/<preset>/src/driver/kirimasim`. To use another preset,
configure and build it, then update `KIRIMASIM`. More contributor checks are
described in [CONTRIBUTING](../CONTRIBUTING.md).

The remaining examples assume the repository root is your working directory.

## Run your first simulation

The repository includes a complete example:

- [counter.sv](examples/user_manual/counter.sv): a parameterized counter with
  synchronous reset and a nonblocking update on each rising edge.
- [tb.sv](examples/user_manual/tb.sv): a testbench with a 10 ns clock period,
  reset release at 12 ns, and a configurable number of samples.
- [settings.svh](examples/user_manual/include/settings.svh): the default width.
- [files.f](examples/user_manual/files.f): the include directory and source files.

```sh
"$KIRIMASIM" --top=tb -f docs/examples/user_manual/files.f
```

The design writes this to standard output:

```text
width=4 cycles=4
t=20 ns q=1
t=30 ns q=2
t=40 ns q=3
t=50 ns q=4
```

Version, seed and termination messages go to standard error. The exit code is
0. The testbench samples on falling edges, after the preceding rising edge's
nonblocking updates have completed, and calls `$finish` after the last sample.
Its free-running clock would otherwise keep future events scheduled.

Try a longer run:

```sh
"$KIRIMASIM" --top=tb -f docs/examples/user_manual/files.f +cycles=6
```

The last sample is `t=70 ns q=6`. The example rejects a nonpositive cycle
count with `$fatal`, giving exit code 1.

## Choose inputs and execution modes

The command forms are:

```text
kirimasim [options] <file>... [+plusarg]...
kirimasim parse [options] <file>...
kirimasim --version
kirimasim --help
```

Without `parse`, each invocation reads the sources, elaborates the hierarchy,
lowers it to SIR and simulates it. There is no separate build/run cache or
saved executable model in the current tier.

### Check syntax and elaboration

Put `parse` before the input options:

```sh
"$KIRIMASIM" parse --top=tb -f docs/examples/user_manual/files.f
```

This checks preprocessing, syntax, types and elaboration without simulating.
Success does not establish that the design can be lowered or executed:
some constructs accepted by slang are not implemented by kirimasim.

### Check lowering and inspect SIR

```sh
"$KIRIMASIM" --dump-sir --top=tb -f docs/examples/user_manual/files.f > counter.sir
```

This also lowers and verifies the design, then writes SIR instead of running
it. SIR format 0 is an evolving diagnostic representation, not a stable
interchange format or a model you can pass back to the executable to run.

### Name the top

`--top=tb` and `--top tb` select the top to elaborate. Repeat the option to
select several tops. Without it, a single config in the input sources is
selected; more than one config requires an explicit choice. Without a config,
slang selects modules that no other module instantiates.

Use `--top=cfg:config` when explicitly selecting a config, including when a
module has the same name. Config selection and source library binding have
additional constraints; see the [CLI specification](spec/CLI.md).

### List sources and include directories

The supplied `files.f` contains:

```text
+incdir+docs/examples/user_manual/include
docs/examples/user_manual/counter.sv
docs/examples/user_manual/tb.sv
```

`-f <file>` reads frontend arguments from that file. Paths in this list are
relative to the process's working directory, not to the list's directory.
Keep kirimasim's execution options and runtime plusargs on the command line.

The same inputs can be given directly:

```sh
"$KIRIMASIM" --top=tb -Idocs/examples/user_manual/include \
  docs/examples/user_manual/counter.sv docs/examples/user_manual/tb.sv
```

`-I<dir>` and `-I <dir>` add include paths, as does the existing
`+incdir+<dir>` form in `files.f`. Repeated paths, including paths mixed with
`+incdir+`, retain their specified order; the directory of the file containing
an include is searched first. Quote paths containing spaces, such as
`-I "include dir"`. The common `-I`, `-D` and `-y` forms are available from
commit `dd2c0188`; check `--version` and `--help` when using an older build.

### Find library modules

For library modules, use `-y<dir>`, `-y <dir>` or `--libdir=<dir>`. Slang
looks for unresolved definitions using its library resolver and the configured
`+libext+<ext>` extensions. These paths do not add include paths; `-I` does
not add library paths.

The counter can be found as a library instead of listing `counter.sv` explicitly:

```sh
"$KIRIMASIM" --top=tb -Idocs/examples/user_manual/include \
  -DCOUNTER_WIDTH=8 -y docs/examples/user_manual \
  docs/examples/user_manual/tb.sv +cycles=6
```

Only `tb.sv` is an explicit source. The unresolved `counter` instance causes
`counter.sv` to be loaded from the library directory. The run prints
`width=8 cycles=6` and ends with `t=70 ns q=6`. The library option can also be
written as `-ydocs/examples/user_manual` or
`--libdir=docs/examples/user_manual`. These input forms work with `parse` too.

The language version is fixed to IEEE 1800-2023; there is no language-version
switch. The kirimasim command line accepts a defined subset of frontend
options, not every option of the standalone slang tool. Direct `-W...`,
`--compat` and elaboration-limit switches are not implemented in this driver.

## Set macros and runtime arguments

`-DNAME`, `-DNAME=value`, `-D NAME` and `-D NAME=value` set preprocessor macros.
The existing `+define+NAME` and `+define+NAME=value` forms are also supported.
Quote the entire shell argument if its value contains spaces, quotes or
wildcard characters.

An omitted value means `1`; `-DNAME=` explicitly defines
an empty replacement. Values containing `+` or additional `=` characters
are preserved. The pinned frontend keeps the first definition when a macro
is predefined more than once, including when the two spellings are mixed.
For commands shared with other simulators, write `-DNAME=1` explicitly:
bare `-DNAME` defaults differ between tools.

```sh
"$KIRIMASIM" --top=tb -DCOUNTER_WIDTH=8 \
  -f docs/examples/user_manual/files.f +cycles=6
```

This prints `width=8 cycles=6` before the samples. The header only defines
`COUNTER_WIDTH` when it has not already been defined.

The macro can equivalently be supplied using the existing plus form:

```sh
"$KIRIMASIM" --top=tb '+define+COUNTER_WIDTH=8' \
  -f docs/examples/user_manual/files.f +cycles=6
```

Other arguments beginning with `+` are runtime plusargs, except the reserved
frontend prefixes `+incdir+`, `+define+` and `+libext+`. The design reads them
with `$test$plusargs` or `$value$plusargs`. In the example:

```systemverilog
if (!$value$plusargs("cycles=%d", cycles)) cycles = 4;
```

Plusargs are passed in command-line order. A plusarg does not define a macro
or override a module parameter: the design must read and use it.

`--seed=<n>` sets the default seed used by `$urandom`, `$urandom_range` and
`randcase`. The default is 0; the argument is an unsigned 64-bit decimal
integer. Explicit seeds in the design and the older `$random` family have
their own rules; see [system tasks](spec/SYSTEM_TASKS.md).

```sh
"$KIRIMASIM" --seed=42 --top=tb -f docs/examples/user_manual/files.f
```

The counter example does not use random values, so its samples do not change.

## Control simulation time

Use `timeunit` and `timeprecision` in the design, or a `` `timescale`` directive.
`--timescale=<unit>/<precision>` supplies the default for design elements
without their own timescale; its default is `1ns/1ns`. The example declares
`1ns` units and `1ps` precision explicitly. `$timeformat` changes displayed
times, not scheduling precision.

```sh
"$KIRIMASIM" --timescale=1ns/1ps --top=tb -f docs/examples/user_manual/files.f
```

`--mintypmax=min|typ|max` chooses the branch of `min:typ:max` expressions,
including delay expressions. Its default is `typ`.

### End at a simulation time

```sh
"$KIRIMASIM" --max-time=25ns --top=tb -f docs/examples/user_manual/files.f
```

The design prints its header and the 20 ns sample, then ends with reason
`time_limit` and exit code 4. When the limit ends a run, all events at or
before its boundary are processed and `final` procedures run. A design that
finishes or runs out of events earlier can end before the limit.

The value must be a nonnegative decimal number immediately followed by `s`,
`ms`, `us`, `ns`, `ps` or `fs`: for example, `10us` or `1.5ns`. It is rounded
to the design's time precision, to nearest with ties away from zero.

Add `--max-time-ok` to make a run ended by this simulation-time limit return 0:

```sh
"$KIRIMASIM" --max-time=25ns --max-time-ok --top=tb \
  -f docs/examples/user_manual/files.f
```

### Limit elapsed execution time

`--max-wall-time=<seconds>` limits elapsed simulation execution time, excluding
compilation. It takes whole seconds; 0, the default, disables it. Expiration
returns 4 with reason `wall_time_limit`, even with `--max-time-ok`.

Checks happen at simulation safe points. This is not a hard timeout for a
blocked host operation, and `final` procedures run without the expired budget.

## Control execution and resource limits

Execution limits help distinguish a long simulation from a loop that never
advances time. Counts are unsigned decimal integers:

- `--max-zero-delay=<n>`: moves of events into Active or Reactive during one
  time slot. Default: **1,000,000**.
- `--max-slot-events=<n>`: events executed across all regions of one time
  slot. Default: **100,000,000**. This also catches processes repeatedly
  waking each other without the region moves counted above.
- `--max-steps=<n>`: consecutive tier 1 instructions executed by one process
  without waiting. Default: **1,000,000,000**. This catches a loop with no
  delay or event wait.
- `--max-procs=<n>`: runtime child processes that have not ended. Default:
  **1,000,000**. Static elaborated processes and retained records of ended
  processes are excluded.
- `--max-call-depth=<n>`: nested task/function calls per process. Default:
  **10,000**. The runtime also guards the remaining simulation-thread stack.

The first three limits end with exit code 5; process and call/stack limits
end with code 6. A value of 0 disables each count limit except
`--max-call-depth`, which must be positive. `--max-scc-iter=<n>` is accepted
but has no effect until static islands in tier 2 are implemented.

For example, to use tighter limits during development:

```sh
"$KIRIMASIM" --max-steps=1000000 --max-slot-events=100000 \
  --max-procs=10000 --max-call-depth=1000 --top=tb \
  -f docs/examples/user_manual/files.f
```

The defaults normally suffice. If a limit fires, check the reported limit
and count, then inspect the reproducing design before raising it.
A `--max-steps` diagnostic also identifies the process.

### Stop policy and bounds warnings

- `--stop=finish`: `$stop` ends the simulation with code 0; the default.
- `--stop=ignore`: `$stop` returns and the calling process continues.
- `--stop=error`: `$stop` ends with code 1. The reason remains `stop`.
- `--bounds-check=warn`: warn about invalid packed selections and fixed-array
  indices at runtime, once per source position, and report access counts at
  the end. `--bounds-check=off` is the default. Warnings do not change the
  values produced by these accesses or make the run fail.

```sh
"$KIRIMASIM" --bounds-check=warn --stop=error --top=tb \
  -f docs/examples/user_manual/files.f
```

These bounds diagnostics are not a check of every container operation.
Detailed coverage and examples are in [P1 features](P1_FEATURES.md).

### Report removed pulses

Use `--report-pulses` to see transitions removed by inertial delays in continuous
assignments, gates, UDPs and delayed strength/switch models. Each cancellation
prints a source-positioned stderr note with its exact time. The option preserves
values, event scheduling, stdout and counters. It is off by default; repeated
cancellations at one position are all reported. Identical pending values,
transport NBAs and ending the simulation produce no pulse notes. See the
[example](P1_FEATURES.md#report-removed-pulses).

## Record VCD waveforms

```sh
"$KIRIMASIM" --wave=counter.vcd --top=tb -f docs/examples/user_manual/files.f
"$KIRIMASIM" --wave=counter-dut.vcd '--wave-scope=tb.dut' \
  --top=tb -f docs/examples/user_manual/files.f
```

Open the resulting VCD in a viewer that accepts VCD files. Output paths are
relative to the working directory. An existing file of the same name is
overwritten when dumping starts.

`--wave-scope=<pattern>[,<pattern>...]` selects scopes and their descendants.
Patterns match hierarchical scope names: `*` matches any characters, `?` one
character. A match on an enclosing scope also selects its descendants.
Quote patterns so the shell does not expand them:

```sh
"$KIRIMASIM" --wave=counter-filtered.vcd '--wave-scope=tb.dut*' \
  --top=tb -f docs/examples/user_manual/files.f
```

By default, VCD records the settled value at the end of each time slot.
`--wave-glitches` records intermediate changes within the slot too.

You can instead add this block to the example's `tb` module and run without
`--wave`:

```systemverilog
initial begin
  $dumpfile("counter-from-design.vcd");
  $dumpvars(0, tb);
end
```

`$dumpvars(0, tb)` includes the scope and all levels below it. The control
tasks `$dumpoff`, `$dumpon`, `$dumpall`, `$dumplimit` and `$dumpflush` are also
supported. With `--wave`, the command line supplies the file and selection,
so design calls to `$dumpfile` and `$dumpvars` are ignored with warnings;
the other control tasks still act. Without `--wave`, `--wave-scope` and
`--wave-glitches` apply to the design's own dumping.

The current VCD output covers supported static nets/variables and scopes;
unpacked arrays, automatic variables and dynamic data such as strings and
queues are omitted. Program/interface scope coverage remains limited.
FST output and `$dumpports` are not implemented. A dump file that cannot be
opened produces a warning and simulation continues, so check that the file
was created. See the [waveform specification](spec/WAVEFORM.md).

## Collect output and statistics

Ordinary simulation output is separated into two streams:

- **stdout**: design output from `$display`, `$write`, `$strobe`, `$monitor`
  and related tasks directed to stdout.
- **stderr**: kirimasim's version/seed/argument line, diagnostics, reporting
  tasks such as `$error` and `$warning`, and termination messages.

Capture them separately when comparing design output:

```sh
"$KIRIMASIM" --top=tb -f docs/examples/user_manual/files.f \
  > design.out 2> diagnostics.log
```

`--autoflush` flushes stdout after each display/write/strobe/monitor task,
including file tasks targeting stdout. Use it when a pipe or interactive
reader needs to see output while the design is still running.

```sh
"$KIRIMASIM" --autoflush --top=tb -f docs/examples/user_manual/files.f
```

### Get counters

```sh
"$KIRIMASIM" --stats=json --top=tb -f docs/examples/user_manual/files.f \
  > stats.out 2> diagnostics.log
tail -n 1 stats.out
```

`--stats=json` appends one JSON object as the last line of stdout. The preceding
design output is still present, so the entire file is not a JSON document.
Useful keys include `sim.end_reason`, `sim.end_time`, `sched.time_slots`,
`sched.events.active`, `nba.scheduled` and `vm.instrs`. `sim.end_time` is in
ticks of the design's global precision, not necessarily nanoseconds. These
are operation counts, not elapsed-time or memory measurements.

Counters are also available with `parse` or `--dump-sir`; the counters reflect
the work of that mode, not a simulation. See [performance counters](spec/PERFORMANCE.md).

## Use system tasks and data files

Implemented tasks include display/formatting, file input/output, memory-file
loading, time and type queries, random values and execution control.
Supported forms and argument types are listed in [P1 support](P1_SUPPORT.md).
Source `%p` and resolved-strength `%v` formatting remain unsupported.

For a small memory-file example, save the following as `memory_demo.sv` in
your working directory:

```systemverilog
module memory_demo;
  logic [7:0] memory [0:3];
  initial begin
    $readmemh("memory.hex", memory);
    $display("first=%h last=%h", memory[0], memory[3]);
  end
endmodule
```

Create `memory.hex` beside it:

```text
10 20 ab ff
```

```sh
"$KIRIMASIM" --top=memory_demo memory_demo.sv
```

The output is `first=10 last=ff`. With no future events, the design ends
normally with reason `quiescent`; a clocked testbench usually needs an
explicit `$finish` instead.

Relative file names passed to `$readmemh`, `$fopen`, `$dumpfile` and similar
tasks are resolved from the process's working directory. Check descriptor
and return values when using file tasks. Fixed arrays, dynamic arrays and
queues have different supported operations; see the
[container examples](P1_FEATURES.md#dynamic-arrays-and-queues) before moving
code between them.

## Interpret exit codes

Capture the executable's exit status in your build or test script:

- **0**: normal `$finish`, no events left (`quiescent`), or the default
  `$stop`. A simulation-time limit also returns 0 with `--max-time-ok`.
- **1**: `$fatal`, or `$stop` with `--stop=error`.
- **2**: invalid command line or input, unsupported constructs, or a runtime
  design error.
- **3**: an internal kirimasim error, such as a SIR verifier failure.
- **4**: simulation-time or wall-clock limit reached, subject to the
  simulation-time-only exception above.
- **5**: a limit on execution without advancing time.
- **6**: runtime process, call-depth or simulation-stack resource limit.
- **130**: SIGINT, usually Ctrl-C.

For a self-check that must fail an automated run, use `$fatal`, for example
`if (actual !== expected) $fatal(1, "mismatch");`. `$error` reports a diagnostic
but does not by itself make the current executable return a failure status.
The numeric argument to `$finish` or `$stop` selects a reporting level, not
an operating-system exit code. `$finish(2)` also requests statistics.

During simulation, a first Ctrl-C requests an orderly stop at a safe point
and execution of `final` procedures; a second ends the process immediately.
During compilation, SIGINT has the operating system's ordinary behavior.

## Troubleshoot a design

### A header or data file cannot be found

Check the working directory and the paths in `-f`. Add the include directory
with `-I<dir>`, `-I <dir>` or `+incdir+<dir>`. Include search paths do not change
where runtime file tasks look for their data files.

### A module cannot be found

List its source explicitly, or use `-y <dir>` / `--libdir=<dir>` with a
matching filename, such as `counter.sv` for module `counter`. Use `+libext+<ext>`
when another library extension is needed. An `-I` directory only supplies
include lookup; a library directory does not supply headers. See the
[library example](#find-library-modules).

### Parse succeeds but simulation fails with code 2

`parse` only exercises slang's frontend. Run `--dump-sir` to check lowering,
then inspect the first positioned diagnostic. A legal SystemVerilog form
can still be unsupported by kirimasim. Reduce the example and check
[P1 support](P1_SUPPORT.md) for the particular form and storage type.

### Signals contain X or Z

kirimasim preserves four-state values. Uninitialized ordinary four-state
integral variables start as X; undriven ordinary wire nets resolve to Z
unless another driver or net kind determines a value. Check reset and
drivers, and use `%b` or `%h` to inspect values.

`--init-values=zero`, `one` or `random` changes defaults of uninitialized
four-state integral variables and fresh memory elements. Explicit initializers,
nets, two-state variables and invalid-read defaults keep their standard values.
`random` uses `--seed` independently of the design's `$random` and `$urandom` streams.
Non-X modes print a note after the version line; `--init-values=x` restores
the default. See the [example](P1_FEATURES.md#source-initial-values).

Use `--cond-x=warn` to report X/Z bits in `if` and `?:` conditions and `case`
selectors. It warns once per source position and prints the number of X/Z
evaluations at the end. A value such as `2'b1x` is reported even though its known 1 makes
the condition true. Values and branch selection are preserved. The default is
`--cond-x=off`; `--cond-x=error` raises a design runtime error and runs `final`.
Diagnostics reached inside `final` follow the existing final-termination
policy and retain the reason decided before `final` began.

### A run hangs or reaches a limit

Look for a loop without a wait, a repeated `#0`, processes waking one another,
or a free-running clock with no finish condition. For code 5, check the
reported limit and count; `--max-steps` also identifies the process.
For code 6, check recursion
and runtime fork creation. Use the time and resource options above to bound
an investigation.

### A waveform is missing a signal or a transition

Check dump-file warnings, scope patterns, the signal's storage/type and the
current VCD limitations. Try `--wave-glitches` for changes within one time
slot. A successful simulation exit does not imply that a dump file opened.

### An option from the specifications is rejected

The specifications also describe planned behavior. Options such as
`--log`, `--diag-format`,
`--tier` and `--state` are not implemented by the current driver. Use shell
redirection for log files. `--help` gives a short list; the sections above
describe the currently accepted interface, including frontend plus-options.

### Results differ from another simulator

Record both versions, input files, options, seed, stdout, stderr and exit
codes. Check races, initialization, time precision and unsupported forms
before comparing outputs. Check [P1 support](P1_SUPPORT.md) for remaining
restrictions on nets, switches and other constructs. Complete IEEE or Icarus
feature equivalence has not been established.

## Find support and report problems

The [Reference Manual](REFERENCE_MANUAL.md) collects the implemented CLI,
defaults, task index and runtime outcomes in one place.

The 2026-10-29 extension supports input-only clocking blocks with default or
explicit `#1step` and `@(cb)`, and strength-preserving static partial
input/output net ports. See [P1 support](P1_SUPPORT.md) for the exact subset;
other clocking skews, output/inout clockvars and broader partial-port forms
remain unsupported. This extension is newer than the manual's source baseline.

- [P1 support](P1_SUPPORT.md): implemented forms and remaining restrictions.
- [P1 examples](P1_FEATURES.md): additional runnable language examples.
- [CLI specification](spec/CLI.md), [system tasks](spec/SYSTEM_TASKS.md) and
  [waveforms](spec/WAVEFORM.md): detailed contracts; check implementation
  status when a section describes planned work.
- [P1 criteria audit](P1_CRITERIA_GAPS_2026-10-28.md) (Japanese): remaining completion work.
- [CONTRIBUTING](../CONTRIBUTING.md): issue forms and participation rules.

Report a problem through the repository's issue forms. Include a minimal
original reproducer, the exact command and working-directory layout,
`--version`, expected and actual results, and the exit status. For timing or
random problems, include timescales and the seed. Do not include confidential
designs or copied standard text; describe the issue in your own words and
cite the relevant standard clause when known. The repository currently
accepts issues and specification proposals rather than pull requests.
