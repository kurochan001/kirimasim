# kirimasim Reference Manual

[日本語](REFERENCE_MANUAL.ja.md) · The English version is authoritative when the versions differ.

Checked on **2026-10-29**, against source `f8408ea37ac1` and the executable
`kirimasim 0.0.0 (commit f8408ea37ac1)`. This is a reference to the implemented
user interface, language subset and runtime behavior. P1 remains in progress.
For installation and a guided first run, use the [User Manual](USER_MANUAL.md).

The command-line tables below list the accepted interface at this revision.
Detailed specifications also contain planned work; those entries are separated
under [unimplemented interfaces](#unimplemented-interfaces). Check `--version`
and `--help` when using a different build.

## Contents

- [Invocation and notation](#invocation-and-notation)
- [Command-line options](#command-line-options)
- [Language and execution reference](#language-and-execution-reference)
- [System tasks, functions and methods](#system-tasks-functions-and-methods)
- [Waveform reference](#waveform-reference)
- [Output, diagnostics and statistics](#output-diagnostics-and-statistics)
- [Termination and exit codes](#termination-and-exit-codes)
- [Unimplemented interfaces](#unimplemented-interfaces)
- [Command examples](#command-examples)
- [Sources and related documents](#sources-and-related-documents)

## Invocation and notation

```text
kirimasim [options] <file>... [+plusarg]...
kirimasim parse [options] <file>...
kirimasim --help | -h
kirimasim --version
```

| Form | Action |
| --- | --- |
| No subcommand | Read and elaborate sources, lower and verify SIR, then interpret the design |
| `parse` | Preprocess, parse, type-check and elaborate; do not lower or simulate |
| `--dump-sir` with no subcommand | Compile and verify SIR, print its text form and stop before simulation |
| `--help`, `-h` | Print usage and accepted options to stdout; no input required |
| `--version` | Print version, source commit and compiler identification to stdout; no input required |

Put `parse` before input options and source paths. Its success establishes
frontend acceptance, not executable support for every construct. Simulation
options have no runtime effect when the design is not simulated. With `parse`,
`--dump-sir` does not replace parsing with SIR generation.

In this manual, `<name>` is a placeholder and `a|b` denotes alternative
values; do not type the brackets or a literal pipe. The short options `-I`,
`-D`, `-y` accept attached or separate operands. `-f` takes a separate operand;
`--top` accepts either `--top=name` or `--top name`. Other value-taking long
options below require `=`.

Quote each shell argument containing spaces, quotes or wildcard characters.
Unknown options and malformed operands are input errors, normally exit 2.
The `--` end-of-options separator is not implemented; use a path such as
`./-design.sv` for a source filename beginning with `-`.

Unless stated otherwise, numeric limits and seeds are decimal integers in
`0` through `18446744073709551615`. Signs, fractions, units and separators
are not accepted for these integer operands. `--max-time` uses its own
time-valued syntax.

## Command-line options

### Input options

| Syntax | Default | Meaning |
| --- | --- | --- |
| `-f <file>` | None | Read frontend arguments from a command file |
| `-I<dir>`, `-I <dir>` | No added user paths | Add an include search directory |
| `+incdir+<dir>` | No added user paths | Existing frontend spelling for include directories |
| `-D<name>[=<value>]`, `-D <name>[=<value>]` | No added macros | Predefine a macro; omitted value is `1` |
| `+define+<name>[=<value>]` | No added macros | Existing frontend spelling for macro definitions |
| `-y<dir>`, `-y <dir>`, `--libdir=<dir>` | No added library paths | Search for unresolved definitions in library files |
| `+libext+<ext>` | `.v` and `.sv` | Add a library filename extension, for example `+libext+.custom` |
| `--top=<name>`, `--top <name>` | Automatic selection | Select a top module or source config; repeat for multiple tops |
| `--timescale=<unit>/<precision>` | `1ns/1ns` | Supply defaults for design elements without their own timescale |

These input options apply to both normal execution and `parse`.

**Paths and command files.** Paths in `-f` files are relative to the process's
working directory, not the directory containing the command file. The file
contains frontend arguments, not shell commands. Keep kirimasim's runtime
options and runtime plusargs on the actual command line. Runtime file tasks
also use the working directory; include paths do not redirect `$fopen` or
`$readmemh`.

**Include lookup.** The containing source file's directory is searched first,
then user include paths in their specified order, including when `-I` and
`+incdir+` are mixed. `-I` does not add module-library paths. Missing/empty
directory operands are input errors.

**Macros.** `-DFLAG` means `-DFLAG=1`; `-DFLAG=` explicitly supplies an empty
replacement. `+` and additional `=` characters within a `-D` value are
preserved. Empty macro names are errors. The pinned frontend retains the
first predefinition of a repeated name, including when spellings are mixed.
For commands shared with other tools, specify the value explicitly, such as
`-DFLAG=1`; bare-definition defaults are not uniform across simulators.

**Libraries.** Library directories do not add include paths. For example,
an unresolved module `counter` can load `counter.sv` from a `-y` directory.
Library sources are not automatically instantiated as extra tops. The
existing frontend resolver and `+libext+` rules determine file lookup.
Adding an extension retains the built-in `.v` and `.sv` extensions.

**Top selection.** With no `--top`, one source config is selected automatically;
more than one requires an explicit choice. Without a config, the frontend
selects modules not instantiated by other modules. Use `--top=cfg:config`
to select a config explicitly, including when a module has the same name.
Config selection and binding retain the frontend's restrictions.

**Language version.** The frontend is fixed to IEEE 1800-2023. There is no
implemented language-version switch. Source `timeunit`, `timeprecision` and
`` `timescale`` declarations take precedence over supplied timescale defaults.

### Simulation time and value selection

| Syntax | Default | Meaning |
| --- | --- | --- |
| `--max-time=<time>` | No user time limit | End with `time_limit` after processing events at or before the boundary |
| `--max-time-ok` | Off | Map `time_limit` to exit 0 instead of 4 |
| `--mintypmax=min\|typ\|max` | `typ` | Select a branch of `min:typ:max` expressions, including delays |
| `--seed=<n>` | `0` | Default seed for `$urandom`, `$urandom_range` and `randcase` |
| `--stop=finish\|ignore\|error` | `finish` | Select the behavior of `$stop` |
| `--autoflush` | Off | Flush stdout after each display/write/strobe/monitor task targeting stdout |

`--max-time` accepts a nonnegative decimal number, optionally fractional,
immediately followed by `s`, `ms`, `us`, `ns`, `ps` or `fs`. Examples are
`10us`, `1.5ns` and `0ns`. No separating space is allowed. The value is rounded
to the design's global precision, to nearest with ties away from zero;
overflow is an input error. `0ns` is a time boundary, not a disabled limit.
A design can finish or become quiescent before reaching the boundary.

`--max-time-ok` only changes the exit status for `time_limit`. It does not
change wall-clock, zero-time, resource or design-error outcomes.

`--stop=finish` ends the run with reason `stop`, exit 0. `ignore` returns to
the caller; `error` ends with reason `stop`, exit 1. The seed does not replace
explicit source seeds or the separate `$random` / `$dist_*` seed rules.

### Execution and resource limits

| Syntax | Default | Counted quantity / outcome |
| --- | --- | --- |
| `--max-zero-delay=<n>` | `1000000` | Moves into Active or Reactive within one time slot; `zero_time_limit`, exit 5 |
| `--max-slot-events=<n>` | `100000000` | Executed events across all regions of one time slot; `zero_time_limit`, exit 5 |
| `--max-steps=<n>` | `1000000000` | Consecutive tier 1 instructions in one process without waiting; `zero_time_limit`, exit 5 |
| `--max-procs=<n>` | `1000000` | Unfinished runtime child processes; `resource_limit`, exit 6 |
| `--max-call-depth=<n>` | `10000` | Nested task/function calls per process; `resource_limit`, exit 6 |
| `--max-wall-time=<seconds>` | `0` | Whole elapsed simulation seconds, excluding compilation; `wall_time_limit`, exit 4 |
| `--max-scc-iter=<n>` | `0` | Accepted, with no effect until tier 2 static islands are implemented |

Zero disables the first four count limits and the wall-clock limit.
`--max-call-depth` must be positive. Its requested stack size may also be
rejected at startup with exit 2; remaining runtime stack exhaustion is a
resource-limit outcome. `--max-steps` is not a whole-simulation instruction budget.

`--max-procs` counts live runtime children, including queued/waiting ones;
it excludes static elaborated processes and records retained after a child
ends. Exceeding it rejects the new child before it starts.

Wall-clock checks occur at simulation safe points, including static
initialization. They do not forcibly interrupt a blocked host operation;
`final` procedures finish without the expired wall-clock budget. Increasing
or disabling a limit does not supply an unimplemented language feature.

### Diagnostics and inspection

| Syntax | Default | Meaning |
| --- | --- | --- |
| `--bounds-check=off\|warn` | `off` | Instrument invalid packed selections and fixed unpacked-array indices |
| `--cond-x=off\|warn\|error` | `off` | Instrument X/Z in runtime `if` / `?:` conditions and `case` selectors |
| `--dump-sir` | Off | Print verified SIR text to stdout instead of simulating |
| `--stats=json` | Off | Append counters as the final JSON line of stdout |

Bounds warnings are emitted once per source position, with access counts at
the end. They preserve read/write/NBA values and do not inspect every kind of
container operation. Invalid queue-element writes have their own mandatory
warning even with `--bounds-check=off`; storage is unchanged by an invalid write.

`--cond-x=warn` reports each source position once and counts evaluations;
`error` ends at the first report as a design runtime error, exit 2.
Instrumentation preserves expression values and branch selection.

SIR format 0 is an evolving diagnostic format, not a stable exchange format.
The executable cannot read the dump back as a compiled model. Counters also
work in `parse` and `--dump-sir`; they describe the work of that mode.

### Waveform options

| Syntax | Default | Meaning |
| --- | --- | --- |
| `--wave=<file>.vcd` | No CLI-selected dump | Create VCD from time zero using command-line selection |
| `--wave-scope=<pattern>[,<pattern>...]` | All eligible scopes | Select matching scope names and their descendants; repeatable |
| `--wave-glitches` | Off | Record intermediate changes within a time slot |

The waveform filename must have a nonempty stem and end in `.vcd`.
`.fst` is currently rejected. Scope patterns use `*` for any characters and
`?` for one; empty patterns are input errors. Quote wildcard arguments.
Without `--wave`, scope/glitch options apply to the design's own dumping.
See [waveform reference](#waveform-reference) for supported data and controls.

### Runtime plusargs

`+name` and `+name=value` are passed in command-line order to the design's
`$test$plusargs` and `$value$plusargs`. The reserved frontend prefixes
`+incdir+`, `+define+` and `+libext+` are not runtime plusargs.

A plusarg neither defines a preprocessor macro nor overrides a module
parameter. `$value$plusargs` uses the first match and leaves its destination
unchanged when there is no match. The design must read and use the argument.
`parse` does not simulate these calls.

## Language and execution reference

The executable uses the SIR interpreter and a shared kernel. Supported
source forms are a subset of the frontend's accepted SystemVerilog. For
precise storage, argument and source-shape limits, consult [P1 Support](P1_SUPPORT.md).

### Values, expressions and aggregates

- Integral values can be signed/unsigned and wider than 64 bits. Four-state
  values preserve 0, 1, X and Z; two-state conversions clear X/Z bits.
- Uninitialized supported four-state integral variables start as X;
  two-state variables as zero, reals as 0.0 and strings as empty. Ordinary
  undriven wire nets resolve to Z, subject to net kinds and other drivers.
  Eligible event declarations create synchronization objects.
- `real`, `shortreal`, `realtime`, strings, enums, packed/unpacked structures,
  packed unions and fixed multidimensional arrays have implemented subsets.
- Arithmetic, logical, bitwise, reduction, comparison, shift, conditional,
  concatenation, cast, `inside`, assignment-pattern and selection operations
  use typed widths and state rules. Case equality is distinct from ordinary
  equality; `case`, `casez`, `casex` and `case inside` have different matching rules.
- Fixed-aggregate copies, comparisons and streaming work for supported
  representations. Streaming requires constant slice sizes; streaming `with`,
  string targets and nested streaming targets remain unsupported.

See [value semantics](spec/VALUE_SEMANTICS.md) for operator-specific rules.
Specification entries for unimplemented representations are not support claims.

### Hierarchy, procedures and scheduling

Modules, packages, supported interfaces/modports, generate constructs,
parameters and source configs are elaborated by the pinned, unmodified
frontend. Broad parameterized or object-based language support is not implied.

Implemented execution forms include `initial`, `always`, `always_comb`,
`always_latch`, `always_ff`, `final`, named programs, static/automatic tasks
and functions, and supported `input`/`output`/`inout`/`ref`/`const ref` arguments.
`final` procedures cannot wait. Loops and branches include `for`, `while`,
`do`, `repeat`, `forever`, `foreach`, `break`, `continue`, `return`,
`unique`/`unique0`/`priority` and `randcase` in their supported forms.

Module processes use the Active evaluation set; program processes use the
Reactive set. Delays, event waits, nonblocking assignments and forked work
retain their documented scheduling/lifetime rules. Reporting in Observed
and output in Postponed are distinct from evaluating an assertion or RHS.
Zero-delay controls do not advance simulation time. Design-local time units
and the global finest precision determine scheduling and rounding;
`$timeformat` only changes presentation.

The supported concurrency subset includes `fork` / `join` / `join_any` /
`join_none`, `wait fork`, `disable fork`, named events and supported named
block/task disable. Input-only clocking blocks with default or explicit
`#1step` and `@(cb)` are implemented for eligible static integral storage;
other skews and output/inout clockvars remain unsupported.

Immediate and deferred immediate assertions are implemented in their documented
forms. Concurrent assertions are not. See the [scheduler](spec/SCHEDULER_SPEC.md)
and [clocking subset](notes/clocking-input-1step-proposal.md).

### Dynamic containers, classes and nets

Supported dynamic arrays and queues have construction, deep value copies,
indexing, references, explicit waits and eligible assignment/NBA paths.
Queue methods include `size`, `delete`, `insert`, `push_front`, `push_back`,
`pop_front` and `pop_back`; dynamic arrays include `size` and `delete`.
Array reductions and ordering are indexed below. Queue-slice writes,
associative arrays, unsupported aggregate/class/event elements and implicit
container sensitivity remain work.

Non-parameterized class static properties and static methods work for the
supported storage types without constructing an object. Class objects,
constructors, instance dispatch and parameterized static storage remain
unsupported. Static-property support does not enable UVM.

Supported nets include ordinary resolved nets, wired nets, pulls and strengths;
gates, UDPs, six directed MOS and six passive switch kinds execute on documented
terminal forms. Some static partial input/output net ports preserve strength.
Broader port/alias/force forms, charge storage, unconnected UDP ports and
specify paths/timing checks remain limited or unsupported. Dense UDP source
processing has an unresolved bounded-run case; full UDP coverage is unconfirmed.

## System tasks, functions and methods

This index names implemented families. Argument types and combinations remain
limited by [P1 Support](P1_SUPPORT.md#system-tasks-functions-and-methods) and
[System Tasks](spec/SYSTEM_TASKS.md). A name in a specification alone does not
establish source support.

### Output, formatting and reporting

| Family | Names / behavior |
| --- | --- |
| Immediate console output | `$display`, `$write` |
| Postponed output | `$strobe`, `$monitor`, `$monitoron`, `$monitoroff` |
| File output | `$fdisplay`, `$fwrite`, `$fstrobe`, `$fmonitor` |
| String output | `$swrite`, `$sformat`, `$sformatf`, `$psprintf` (alias of `$sformatf`) |
| Severity | `$fatal`, `$error`, `$warning`, `$info` |
| Execution control | `$finish`, `$stop`, `$exit`, `$stacktrace` |

Display/write/strobe/monitor families support `b`, `o`, `h` suffix variants,
including the file forms and `$swrite`. `$strobe` and `$monitor` arguments
cannot read automatic variables. Runtime string/packed format expressions
are supported in the documented forms.

| Output conversion | Purpose |
| --- | --- |
| `%b`, `%o`, `%h` / `%x`, `%d` | Integral binary, octal, hexadecimal and decimal |
| `%c`, `%s` | Character and string |
| `%e`, `%f`, `%g` | Real formatting |
| `%t` | Time formatted by `$timeformat` |
| `%m`, `%l` | Hierarchical scope and library/cell name |
| `%u`, `%z` | Raw 32-bit words, low word first, native byte order; `%u` clears X/Z |
| `%%` | A literal percent sign |

Widths and supported flags apply as documented. Real precision and flags do
not imply the same flags work on integers. Packed binary destinations retain
NUL bytes; strings omit them. Source `%p` and resolved-strength `%v` are not
implemented, although internal pattern-rendering facilities exist.

`$error` and `$warning` report diagnostics but do not by themselves force a
failing exit status; use `$fatal` for a self-check that must fail a run.
The argument of `$finish` / `$stop` is a reporting level, not an OS exit code:
0 suppresses their termination reports, 1 is the default, and 2 also requests
counter JSON on stderr. `$exit` terminates the originating program's work;
with no program origin it is ignored.

### File and memory operations

| Family | Names |
| --- | --- |
| Open, close, flush | `$fopen`, `$fclose`, `$fflush` |
| Character/string input | `$fgetc`, `$ungetc`, `$fgets` |
| Scanning | `$fscanf`, `$sscanf` |
| Binary input | `$fread` |
| Position and status | `$ftell`, `$fseek`, `$rewind`, `$feof`, `$ferror` |
| Memory files | `$readmemb`, `$readmemh`, `$writememb`, `$writememh` |

File descriptors include supported standard-input/output forms. Relative
filenames use the process's working directory. Append-mode output goes to
the end of the file even after a seek; seekable-file positions track the
written data. Memory operations support documented fixed integral memories,
multidimensional forms and ref forwarding; dynamic memories and unsupported
aggregate layouts remain outside that subset.

Scanning supports documented integer/string/real conversions, widths,
assignment suppression and raw `%u` / `%z` words. Scan `%t`, `%v` and `%m`
are not implemented. `$fread` has distinct integral-target and memory-target
rules for counts, indexing and copyback; use the detailed task reference
before relying on a partial read or an effectful destination.

### Queries, mathematics and random values

| Family | Names |
| --- | --- |
| Time | `$time`, `$stime`, `$realtime`, `$timeunit`, `$timeprecision`, `$timeformat`, `$printtimescale` |
| Conversions | `$signed`, `$unsigned`, `$rtoi`, `$itor`, `$realtobits`, `$bitstoreal`, `$shortrealtobits`, `$bitstoshortreal` |
| Types and dimensions | `$bits`, `$typename`, `$isunbounded`, `$dimensions`, `$unpacked_dimensions`, `$left`, `$right`, `$low`, `$high`, `$increment`, `$size` |
| Bit properties | `$countbits`, `$countones`, `$onehot`, `$onehot0`, `$isunknown` |
| Integer math | `$clog2` |
| Real math | `$ln`, `$log10`, `$exp`, `$sqrt`, `$pow`, `$floor`, `$ceil`, `$sin`, `$cos`, `$tan`, `$asin`, `$acos`, `$atan`, `$atan2`, `$hypot`, `$sinh`, `$cosh`, `$tanh`, `$asinh`, `$acosh`, `$atanh` |
| Legacy random | `$random`, `$dist_uniform`, `$dist_normal`, `$dist_exponential`, `$dist_poisson`, `$dist_chi_square`, `$dist_t`, `$dist_erlang` |
| Process random | `$urandom`, `$urandom_range`, `randcase`; CLI default seed from `--seed` |
| Runtime arguments | `$test$plusargs`, `$value$plusargs` |

Queries apply to supported types/dimensions. Real/integer conversion rounding,
unknown bits, wide operands and random distribution ranges have specific rules;
they are not interchangeable casts or host-library calls.

### Built-in methods

| Receiver | Implemented method subset |
| --- | --- |
| String | `len`, `getc`, `putc`, `toupper`, `tolower`, `compare`, `icompare`, `substr`, `atoi`, `atohex`, `atooct`, `atobin`, `atoreal`, `itoa`, `hextoa`, `octtoa`, `bintoa`, `realtoa`; character indexing |
| Enum | `first`, `last`, `num`, `name`, `next`, `prev` |
| Dynamic array / queue | The construction, sizing and mutation subset described above |
| Fixed array / supported dynamic array / queue | `sum`, `product`, `and`, `or`, `xor`, `reverse`, `sort`, `rsort`, `min`, `max`, with supported `with` expressions |
| Named event | `.triggered` and the documented trigger/wait forms |

Ordering is stable for supported keys and does not accept every effectful or
unverified-pure key function. Min/max return a separate queue with at most one
original element; an empty receiver gives an empty result. Other locator
methods and shuffle remain unimplemented.

## Waveform reference

VCD control tasks are `$dumpfile`, `$dumpvars`, `$dumpoff`, `$dumpon`,
`$dumpall`, `$dumplimit` and `$dumpflush`. `$dumpvars(0, scope)` selects that
scope and its descendants. CLI `--wave` sets the file and initial selection;
design `$dumpfile` / `$dumpvars` calls then warn and are ignored, while the
other controls still act.

Supported static nets and variables are recorded with four-state values.
By default, the settled value at a time-slot boundary is recorded;
`--wave-glitches` also records intermediate transitions. Files are flushed
and closed at termination, including cut-short runs and final procedures.

Unpacked arrays, automatic variables and dynamic data such as strings and
queues are omitted. Program/interface scope coverage remains incomplete.
FST and `$dumpports` are unimplemented. Failure to open a dump file produces
a warning and does not itself fail simulation; verify that the file exists.
Output paths are relative to the working directory and an existing file is
overwritten when dumping begins. See [Waveform](spec/WAVEFORM.md).

## Output, diagnostics and statistics

| Destination | Content |
| --- | --- |
| stdout | Design output directed to stdout; SIR with `--dump-sir`; final counter line with `--stats=json` |
| stderr | Version/seed/argument banner for normal simulation, diagnostics, severity reports and termination reports |
| Named files | File-task output and waveform data |

Typical positioned diagnostics identify a file, line and column. Unsupported
source constructs normally produce a positioned `error: unsupported:` and
exit 2. Warnings alone are not proof that every requested output was created.

`--stats=json` appends one JSON object as the last stdout line, after inserting
a newline if necessary. Earlier design or SIR output remains, so the whole
stream is not a JSON document. Ordinary counters are unsigned counts;
`sim.end_reason` is a string. Available keys depend on the execution mode.

| Counter family | Interpretation |
| --- | --- |
| `front.*` | Frontend activity and diagnostics; inspect keys for the current mode |
| `sir.*` | Lowered design size, including slots, processes, blocks and instructions |
| `sched.events.<region>`, `sched.time_slots` | Executed events by region and time slots |
| `proc.*` | Process creation, completion, wakeups and retained records |
| `slot.*`, `nba.*`, `wait.*`, `vm.instrs` | Value updates, pending updates, subscriptions and interpreted instructions |
| `sim.end_time`, `sim.end_reason` | End time in global-precision ticks and termination reason |

These are operation counts, not CPU time, elapsed seconds or RSS. Preserve
the source revision, options and seed when comparing them. The detailed
counter contract is in [Performance](spec/PERFORMANCE.md).

## Termination and exit codes

| Code | Reason / interpretation |
| --- | --- |
| `0` | `finish`, `quiescent`, normal `stop`, successful inspection, or `time_limit` with `--max-time-ok` |
| `1` | `fatal`, or `stop` with `--stop=error` |
| `2` | Invalid input/CLI, unsupported source or a design runtime error |
| `3` | Internal compiler/runtime failure, including a verifier failure |
| `4` | `time_limit` or `wall_time_limit` |
| `5` | `zero_time_limit` |
| `6` | `resource_limit`, including call/stack and runtime-child limits |
| `130` | SIGINT interruption |

An empty event queue ends as `quiescent`. Supported orderly simulation endings
run `final` procedures; those procedures cannot wait. Compilation/input failures
and abrupt process termination do not provide that simulation cleanup guarantee.

During simulation, a first SIGINT requests an orderly stop at a safe point;
a second ends the process immediately. During compilation, SIGINT has the
operating system's ordinary behavior. Reason names and OS exit codes are
different interfaces: `--max-time-ok` changes the code, not `sim.end_reason`.

## Unimplemented interfaces

The following are not accepted public driver options at this revision, even
when present in detailed plans or supported by standalone slang:

- Specified but unfinished options: `--init-values`, `--report-pulses`,
  `--log`, `--diag-format=json`.
- Direct frontend switches not exposed by the driver: `-W...`, `--compat`,
  `--max-hierarchy-depth`, `--max-generate-steps`, `--max-constexpr-depth`,
  `--max-constexpr-steps`; unlisted aliases such as `-U` are not enabled.
- Future execution/development options: `--tier`, `--tier-check`, `--state`,
  `--report-state-opt`, `--race-check`, `--race-seed`, `--race-nba`, `--public`,
  `--preserve`, `--disable-pass`, `--cxx`, `--cache-dir`, `--randomize-fail`,
  `--assert-incomplete`.
- Compiled-model subcommands `build`, `run <model>` and `lib-create` are
  planned interfaces, not current subcommands.

Class objects, associative arrays, broader randomization/UVM, concurrent
assertions, coverage, virtual interfaces, broader clocking, specify timing,
VPI/DPI-C and other forms remain unsupported or partial. VPI-dependent
cocotb integration and FST output are not available. Required Icarus-supported
mechanisms still belong to P1 under [P1 Scope](spec/P1_SCOPE.md); phase labels
do not remove those requirements. Full IEEE/Icarus feature equivalence remains
unconfirmed; the [remaining inventory](P1_SUPPORT.md#remaining-work-and-scope)
and [exit-criteria audit](P1_CRITERIA_GAPS_2026-10-28.md) give the dated evidence.

## Command examples

Run from the repository root after a development build. The supplied example
has 1 ns units and 1 ps precision. Full setup is in the [User Manual](USER_MANUAL.md).

### Includes, macros, libraries and runtime arguments

```sh
build/dev/src/driver/kirimasim --top=tb \
  -Idocs/examples/user_manual/include -DCOUNTER_WIDTH=8 \
  -y docs/examples/user_manual docs/examples/user_manual/tb.sv +cycles=6
```

The header is `width=8 cycles=6`; samples run from `t=20 ns q=1` through
`t=70 ns q=6`, exit 0. `counter.sv` is resolved through the library path.

### Frontend-only checking

```sh
build/dev/src/driver/kirimasim parse --top=tb \
  -f docs/examples/user_manual/files.f
```

Success returns 0 with no design output.

### Verified SIR output

```sh
build/dev/src/driver/kirimasim --dump-sir --top=tb \
  -f docs/examples/user_manual/files.f > counter.sir
```

This creates a diagnostic SIR dump, not a reusable executable.

### Time limit and machine-readable counters

```sh
build/dev/src/driver/kirimasim --max-time=25ns --max-time-ok --stats=json \
  --top=tb -f docs/examples/user_manual/files.f > bounded.out 2> bounded.log
```

The exit code is 0; the final JSON line has `sim.end_reason` equal to
`time_limit` and `sim.end_time` equal to `25000` ticks. Earlier stdout contains
the design header and the 20 ns sample.

### Scope-filtered waveform

```sh
build/dev/src/driver/kirimasim --wave=counter.vcd '--wave-scope=tb.dut' \
  --wave-glitches --top=tb -f docs/examples/user_manual/files.f
```

This selects the counter instance's eligible signals. Quote the full option
when using a wildcard, for example `'--wave-scope=tb.dut*'`.

## Sources and related documents

- [User Manual](USER_MANUAL.md): building, guided examples and troubleshooting.
- [P1 Support](P1_SUPPORT.md) and [P1 Features](P1_FEATURES.md): implemented
  source forms, restrictions and runnable examples.
- [CLI](spec/CLI.md), [Value Semantics](spec/VALUE_SEMANTICS.md),
  [Scheduler](spec/SCHEDULER_SPEC.md), [System Tasks](spec/SYSTEM_TASKS.md),
  [Waveform](spec/WAVEFORM.md), [Performance](spec/PERFORMANCE.md): detailed
  contracts, including explicitly planned sections.
- [Driver](../src/driver/main.cpp): accepted argument forms, defaults and exits;
  [common-input CLI tests](../tests/cli/run_common_input_tests.py) and
  [general CLI tests](../tests/cli/run_cli_tests.py): independent checks.

When reporting an issue, include the exact `--version`, command, working-directory
layout, self-written minimal source, expected/actual output, stderr and exit
status. Record timescales and seeds where relevant. Use the repository's Issue
forms and omit private designs, standard text and commercial-simulator output.
