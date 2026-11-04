# FAQ

**P1 policy update (2026-11-01):** Icarus's `wire real` extension is optional.
Only the reviewed `real_net` source is out of scope: the current 180-probe inventory
has 179 P1-evaluated cases, one exclusion, one required-scope gap and two raw gaps.
Unsupported results and independent faults remain recorded. Standard `real`
features retain their existing scope. See the [decision and checks](notes/p1-wire-real-exception.md).


[日本語](FAQ.ja.md) · [Documentation guide](README.md) · [Project README](../README.md)


Updated **2026-11-04**, including [scalar specify execution](notes/specify-execution.md) and [simple conditional/fallback paths](notes/specify-state-execution.md). These answers describe the
implemented subset; the [support inventory](P1_SUPPORT.md) gives the limits
and the [Glossary](GLOSSARY.md) explains the terms. Historical benchmark and
external-population results retain their recorded versions and dates.

- [Start and inspect a run](#start-and-inspect-a-run)
- [Coverage and comparison results](#coverage-and-comparison-results)
- [Values, scheduling and lifetime](#values-scheduling-and-lifetime)
- [Waveforms, limits and performance](#waveforms-limits-and-performance)
- [Development and contributions](#development-and-contributions)

## Start and inspect a run

### Can I use kirimasim on my project now?

It is still in early development, and P1 is unfinished. RTL and simple
testbenches run through the SIR interpreter; class objects and other newer
features have bounded subsets. Start with the [User Manual](USER_MANUAL.md),
then check the exact type, lifetime and context in [P1 Support](P1_SUPPORT.md).
A planned interface in a specification is not necessarily available.

### What should I record when a result needs reproducing?

Keep the ordered inputs, top, working directory, options, seed, stdout, stderr
and exit code. Save both `git rev-parse HEAD` and the executable's `--version`:
the checkout can be newer than the binary. See [Validation](VALIDATION.md)
and [Debugging](DEBUGGING.md) for commands and minimal original examples.

### Why can parse succeed while simulation reports unsupported?

`parse` runs slang's preprocessing, parsing, type checking and elaboration.
Normal execution also lowers to SIR, verifies it and runs the supported
runtime subset. `--dump-sir` checks lowering/verification and prints SIR
without simulating; it is not a complete saved executable. Compare the
[three invocation modes](REFERENCE_MANUAL.md#invocation-and-notation).

### Why does it stop at an unsupported construct instead of skipping it?

Skipping a statement could produce a plausible wrong result. Ordinary
unsupported source is reported with its position and exit 2 before the design
runs. Frontend legality errors, unsupported forms and runtime design errors
can share exit 2, so retain the diagnostic too. An internal failure (exit 3)
is a separate problem to report. See [exit codes](REFERENCE_MANUAL.md#termination-and-exit-codes).

### Can I pass every slang option, including --compat?

No. The driver exposes the [listed input options](REFERENCE_MANUAL.md#input-options).
`-W<name>`, `-Wno-<name>` and `-Werror[=<name>]` pass frontend warning policy
through in the specified order. They do not turn off runtime errors or limits.
Direct `--compat=default|vcs|all` and the four elaboration limits are available.
The modes select pinned frontend profiles; strict is the default. Frontend
acceptance can still lack executable support: relaxed source-driver conflicts
are compile errors, not successful simulations or hidden verifier failures.
See [mode differences](notes/compatibility-execution.md). Unlisted standalone
slang options are not automatically exposed by the driver.

### Why is a header, module or data file not found?

Include paths (`-I`, `+incdir+`), module-library paths (`-y`, `--libdir`) and
runtime file names serve different purposes. Paths in a `-f` list and names
passed to `$fopen`/`$readmemh` are relative to the working directory, not the
file list's directory. See [input lookup](USER_MANUAL.md#choose-inputs-and-execution-modes).

### Where do design output, statistics and diagnostics go?

Design output and the final `--stats=json` summary go to stdout; diagnostics
and informational messages go to stderr. With statistics enabled, stdout is
not necessarily one JSON document or just design text. Capture the streams
separately and parse the final summary line. The driver inserts a newline
before that JSON object when necessary:

```sh
build/dev/src/driver/kirimasim --stats=json demo.sv > run.out 2> diagnostics.log
tail -n 1 run.out
```

See [output and statistics](USER_MANUAL.md#collect-output-and-statistics).

## Coverage and comparison results

### Is P1 complete now that the numerical gates pass?

No. The required execution coverage, subject to the approved optional `wire real`
exception, is at least as broad as Icarus Verilog
13.0, while preserving the approved value/scheduling rules. Finite probes,
the frozen 1,466-test population, sv-tests counts and consecutive fuzz runs
answer different questions. The [P1 criteria](../README.md#p1-exit-criteria)
and [remaining inventory](P1_SUPPORT.md#remaining-work-and-scope) still apply.

<a id="why-do-the-comparison-counts-change-from-95-to-131-to-145"></a>

<a id="why-do-the-comparison-counts-change-from-95-to-131-to-145-to-156"></a>

### Why do probe, CTest and population counts differ?

They measure different things. The current [180-probe comparison](measurements/2026-11-04/icarus13-class-clocked-after.json)
has 179 P1-evaluated inputs and one optional `wire real` exclusion. The previous
176 inputs and their native results are unchanged; three of the four added
class-clocked probes close new Icarus-passing gaps, and one is a control.
Older 95/131/145/156/171/176 inventories keep their original dates and evidence.

Repository CTest currently has 12,012 tests in dev/ASan/UBSan and 12,013 in
release; `cli/slot_event_default` is release-only. One CLI test may execute many
source inputs. The fixed P1 population is a separate 1,466-test dataset, and
sv-tests has its own fixed list. More CTests or probes do not prove a language
completion percentage. See [recorded counts](VALIDATION_COUNTS.md) and
[the published validation scope](notes/main-integration-2026-11-04.md).

<a id="what-are-the-five-gaps-in-the-latest-finite-inventory"></a>

<a id="what-are-the-three-gaps-in-the-latest-finite-inventory"></a>

### Which remaining raw gaps are required for P1?

The [current 180-case run](measurements/2026-11-04/icarus13-class-clocked-after.json) has one required-scope gap, `dense_udp_coverage`. The original `specify_path` input now passes with scalar timing execution.
`real_net` is a confirmed optional `wire real` extension case. Its raw failure
remains visible and does not block P1 by itself. Crashes, hangs and internal
errors would still fail the gate, including on this optional case.

The following dated checkpoints retain their raw observations:

`specify_path`, `dense_udp_coverage` and the Icarus extension `real_net`.
The [same 145 frozen probes](measurements/2026-11-01/icarus13-port-effective-kinds.json)
now pass both supply-inner cases after [effective net kinds](notes/port-kind-execution.md).
The dense UDP case reaches the probe's 15-second frontend deadline; this
is distinct from an unsupported UDP keyword or a runtime mismatch. Required
specify/VPI coverage beyond the first implemented subsets still needs contracts and implementation.
This list is finite and is not the entire P1 backlog.

The partial-inout checkpoint [171-case run](measurements/2026-11-01/icarus13-partial-inout-ports.json)
adds fifteen partial-inout probes. The baseline's fourteen passing-reference
gaps fall to three; four new wired readout differences are recorded separately
with their reference cause unconfirmed. Earlier sources/oracles and native
statuses remain unchanged. See [partial-inout validation](notes/partial-inout-port-execution.md).


The latest [156-probe run](measurements/2026-11-01/icarus13-converting-ports.json)
adds eleven converting-port probes with a frozen before/after record. Five
new passing-reference gaps close; six new optional-collapse readout differences
stay separate. No earlier source/oracle byte changes. The three cases above
remain, and P1 is still incomplete.


### My design prints something different from Icarus Verilog or Verilator. Which is right?

Keep the written expectation and consult the standard and approved project
contract. A [reference mismatch](GLOSSARY.md#reference-mismatch) is different
from a [passing-reference gap](GLOSSARY.md#passing-reference-gap). Several
outputs can be legal where ordering or optional port collapse permits a
choice. The latest inventory keeps four earlier reference mismatches and
eleven new optional-collapse readout differences separate; it does not change
oracles to copy a reference output. See [Validation](VALIDATION.md).

### How does it differ from Icarus Verilog and Verilator?

kirimasim targets the approved four-state and full-region scheduling contract
through a shared kernel, with tier 1 interpreting SIR today and tier 2 C++
generation planned for P3. Other simulators are comparison evidence, not
oracles. Feature and speed comparisons apply to the exact builds, inputs and
options in [the recorded measurements](BENCHMARKS.md); they are not blanket
claims about every design or today's other tool releases.

### How does it differ from Lyra?

The [recorded Linux comparison](BENCHMARKS.md#development-versions-on-linux-wsl2-2026-10-25)
uses Lyra `63922502ff0a` and the kirimasim revision stated there. Treat its
backend, build coverage and timings as observations of those versions.
Lyra's current release/features were not remeasured in this documentation
update. For migrating a command, use the [Porting Guide](PORTING.md), whose
external-tool examples also state their own evidence and limits.

### Do you check against commercial simulators?

The project does not hold licenses for commercial SystemVerilog simulators.
We have not verified kirimasim with them and will not run or compare them for
this project. Third-party commercial simulator outputs and diagnostics are
also excluded from test expectations and implementation decisions. Keeping
results private does not by itself establish that their use is permitted.
Expectations follow IEEE 1800-2023 and the approved project contracts, with
independent reference models and the documented comparison environments.

A public example is the [Synopsys Cloud Service Agreement 2024](https://www.synopsys.com/cloud/service-agreement.html):
sections 9.3.2, 9.3.5 and 9.3.7 restrict reverse engineering, use to develop or
enhance competing products, and benchmarking or disclosure of benchmark
results. On 2026-11-01, that public page could be read without login or product
license authentication. Reading public contract text is separate from holding
a product license; this example does not indicate that the project has entered
into that agreement or that the agreement applies to it.

Terms vary by product, contract and jurisdiction. We do not claim that every
ordinary verification run is prohibited or constitutes reverse engineering.
The decision not to use commercial simulators is this project's operating
policy. Commercial-tool execution compatibility and matching results remain
unconfirmed; see the [posting policy](#may-i-include-commercial-simulator-output-or-diagnostics-in-an-issue).

### Does it support class objects, or only static properties?

Both have supported subsets. Constructed objects have typed fields, staged
constructors, inheritance, direct/virtual methods, waiting tasks, shallow copy
and retained receivers. Non-parameterized static properties use shared storage.
Property refs/selected writes/NBA targets, object observation/formatting,
managed-event fields and parameterized static storage remain boundaries.
See [examples](P1_FEATURES.md#class-objects-and-virtual-dispatch) and the
[precise source contract](notes/class-source-execution.md).

### Can an explicitly clocked process use class objects?

Yes. Ordinary `always @(posedge clock)` and `always_ff` bodies can use supported
property operations, method calls and allocation. A property read can supply
the captured RHS of an ordinary NBA to a static variable. Changing an object
property alone does not retrigger the process. Signal-based guards and expression
events retain their existing behavior. Object-dependent trigger/guard/watch
evaluation and implicit object sensitivity remain unsupported. A null receiver
still causes a positioned runtime error. See the [scope and checks](notes/class-clocked-execution.md).

### Does it run UVM?

Not yet. Working class/container examples do not enable every combination,
constraint, synchronization primitive or external interface UVM needs. The
required Icarus-supported subset remains P1 work; broader UVM and constrained
randomization retain later plans. UVM is intended to run as ordinary source,
with no special recognition of its class names.

### Does it support VPI, DPI-C or cocotb?

The [first VPI execution subset](notes/vpi-execution.md) is available through
`--vpi=<file>`: registered tasks and integer/sized/real functions, static numeric
reflection, variable writes/force/release and supported callbacks run in tier 1.
[Whole-net writes](notes/vpi-net-write-execution.md) support immediate point-net
deposits and registered-net force/release. `parse` and `--dump-sir` execute compiletf
but not calltf. Selected/delayed net writes, passive deposits, full VPI, unlisted
APIs and cocotb integration remain unfinished; DPI-C is unimplemented.
Unsupported requests are API or loader errors, not exclusions from required P1 scope.

### How do VPI net deposits differ from force/release?

A supported point-net deposit changes the public snapshot without changing the
drivers. Any actual committed driver-contribution change on that physical net
clears the deposit across the whole declaration, even if ordinary resolution
would stay the same. Repeating an identical contribution does not clear it.

Force holds the value while drivers continue updating. Release exposes the
current drivers; a deposit made under force does not survive release. HDL and VPI
share per-bit force ownership. Whole-vector commits precede callbacks, which may
perform another supported write. See the [scope and checks](notes/vpi-net-write-execution.md).

### Is slang modified?

No. Parsing and elaboration use the pinned, unmodified frontend identified
in the [README](../README.md#status-2026-11-01). A frontend source-processing
limit is distinct from a kernel execution limit; use `parse` when narrowing
one down. A newer upstream revision passing a case does not establish that
the repository's pinned build passes it. Record the exact versions.

## Values, scheduling and lifetime

### Which version of the standard does it follow?

IEEE 1800-2023, selected with slang's `--std=1800-2023`. There is no public
language-version switch. Strict frontend diagnostics remain the default;
[Unspecified Behavior](spec/UNSPECIFIED_BEHAVIOR.md) records reviewed choices.
This is the target contract, not a claim that every standard feature is implemented.

### Is there a two-state mode for speed?

There is no global option that turns four-state logic into a two-state
simulation. Declared types still matter: `logic` can retain X/Z and `bit`
cannot. Planned tier 2 analysis may use two-state storage only where it
preserves results. See [value types](spec/VALUE_SEMANTICS.md#12-4-state-and-2-state-types).

### Can I replace unknown initial values without changing explicit initializers?

`--init-values=x|zero|one|random` selects eligible uninitialized four-state
variables. It does not replace explicit initializers, net resolution or
all typed defaults. Random initialization uses `--seed` for reproduction.
It changes initial values and can hide a missing reset, so record the choice;
see [initial-value scope](P1_FEATURES.md#source-initial-values).

### Why does display after a nonblocking assignment show the earlier value?

An NBA schedules an update; a following statement in the issuing process can
run before that update. Program work uses Re-NBA rather than ordinary NBA.
Choose an observation point that follows the intended update; `#0` does not
mean “after every NBA.” See [scheduling terms](GLOSSARY.md#scheduling) and the
[reactive-set example](P1_FEATURES.md#programs-and-the-reactive-region-set).

### Can always_ff wait on a computed expression or runtime index?

Yes, within the [explicit expression-event subset](P1_FEATURES.md#expression-events-in-always_ff).
It uses typed watches, supports integral expressions/runtime selections and
mixed fixed items, and keeps the single-writer and timing restrictions.
Registering the initial baseline does not execute the body. Class/event
observation combinations and extra body waits remain restricted.

### Does changing an iff condition wake the process by itself?

No. `iff` is checked when its event occurs; it is not a replacement for
`wait (condition)`. The expression's edge/change and its guard are separate.
Functions used by the event/guard must respect the existing external-write
restrictions. See [watch behavior](notes/always-ff-expression-execution.md).

### What does disabling a named fork cancel?

`disable workers` targets the relevant named parallel scope and its retained
child activity, including children that remain after `join_none` or a returned
call. It preserves unrelated activity, already scheduled NBAs and confirmed
reports while canceling the targeted waits. This differs from assuming every
process with the same spelling is one target. See [the example and limits](P1_FEATURES.md#disable-of-a-named-fork).

### Are copying a class handle and copying an array the same operation?

No. A handle assignment aliases the same object; a shallow object copy creates
another object while retaining nested object handles. Supported dynamic-array
and queue value assignments copy their owned values under their typed rules.
`ref` aliases caller storage instead of taking a value snapshot. See
[storage/lifetime terms](GLOSSARY.md#storage-objects-and-lifetime).

### Why can differently numbered fixed-array ports share elements?

Compatible dimensions correspond in declaration order: the left end matches
the other left end, even with reversed numeric bounds. Each source declaration
keeps its own query/indexing view. Eligible static element/packed-part/concat
input/output ports preserve physical strengths and now apply
[effective wired/pull/supply kinds per bit](notes/port-kind-execution.md).
[Converting integral ports](notes/converting-port-execution.md) instead use
directed strong carriers. [Static partial inout](notes/partial-inout-port-execution.md)
uses identity. Inout conversions, uwire and force-observable identity connections
retain limits. See [the two port forms](P1_FEATURES.md#fixed-array-ports).

### Why does a width-changing net port use strong strength?

A conversion is a value operation. Under the approved port contract it uses a
directed implicit continuous assignment, so the converted contribution enters
the destination at ordinary strong strength. Local drivers do not flow back
through that assignment. Same-type eligible structural ports instead retain
physical origins through identity. Explicit casts, including nested casts and
casts with the same final width, remain value operations. See the
[execution note](notes/converting-port-execution.md).

### Does a partial inout port create two value drivers?

Eligible static net-net inout bits use identity and share physical origins.
The port adds no value carrier or root, even when there is no drive on either
side. A supply retains its strength across that identity; a cast on an ordinary
input/output port instead uses its documented strong value conversion.
[The partial-inout note](notes/partial-inout-port-execution.md) covers static
array elements, kinds, cycles and remaining legality boundaries.

## Waveforms, limits and performance

### Can it write waveforms?

Yes, four-state VCD through design dump tasks or `--wave=<file>.vcd`.
Native FST is selected explicitly by `--wave=<file>.fst`, sharing the same
static selection, aliases, slot timing, glitch mode and controls. Without
`--wave`, a source `$dumpfile("name.fst")` still writes VCD. FST `$dumplimit`
is a soft compressed-block limit: hierarchy, a block and finalization can make
the final file larger. Reader-based checks pass; viewer interoperability and
performance remain unconfirmed. See [waveform options](REFERENCE_MANUAL.md#waveform-options)
and [execution evidence](notes/fst-execution.md).

### Why is a signal or transition missing from VCD?

Check the selected scope/depth, `--wave-scope`, dump on/off state and whether
the value changed. Ordinary output coalesces committed changes within a
slot; `--wave-glitches` retains supported intermediate changes. Unpacked
arrays and class/object observations have separate unsupported boundaries.
See [VCD limits](USER_MANUAL.md#record-vcd-waveforms) rather than assuming
that execution support also enables waveform tracing for every type.

### How do I stop a run that does not end?

`--max-time` limits design time (exit 4, or 0 with `--max-time-ok`);
`--max-wall-time` limits elapsed simulation execution, excluding compilation
(exit 4). Zero-time limits catch work that never advances design time (exit 5).
Ctrl-C requests an orderly stop (130). `--max-time-ok` does not waive
wall-clock, zero-time, resource or design errors. Compile-time hierarchy,
generate and constant-evaluation limits are separate frontend options
([reference](REFERENCE_MANUAL.md#frontend-elaboration-limits)); they reject with
exit 2 before simulation, and zero is not unlimited. See [run limits](USER_MANUAL.md#control-execution-and-resource-limits).

### Why can a process limit be reached with few initial blocks?

`--max-procs` counts unfinished runtime children, including queued and waiting
forked work, not just source initial blocks. Ended children return capacity
even if a retained record is still needed by descendants. This is different
from a memory/stack or instruction-count limit. See [the process-limit example](P1_FEATURES.md#live-runtime-process-limits).

### Does exit 0 prove my self-check passed?

No. It can mean ordinary finish, quiescence or an allowed time limit; the
harness must check expected output too. `$error` alone does not make this
simulator's OS exit code nonzero. Use an appropriate failing self-check such
as `$fatal`, and retain stderr. See [termination](REFERENCE_MANUAL.md#termination-and-exit-codes).

### How fast is it?

Read the dated [benchmark records](BENCHMARKS.md). Design size, frontend work,
execution, output, build mode and tool versions affect elapsed time.
`--stats=json` and PerfCount measure operation counts, not elapsed speed.
Tier 2 C++ generation is planned for P3 and is not the current execution path.

### Does it use more than one thread?

One simulation executes on one thread. Independent Contexts can run concurrently;
each owns its state. Internal multithreaded simulation is later work.
A concurrent-Context test does not establish parallel execution of one design.
See [Architecture](ARCHITECTURE.md).

### Which platforms does it run on?

The repository targets Linux and macOS; the [build instructions](USER_MANUAL.md#build-and-version)
and committed CI/nightly workflows identify the supported compiler/configuration
checks. Windows support is unconfirmed. Do not substitute a host used for one
measurement for the complete supported-platform list.

## Development and contributions

### Does checking mutation anchors prove the tests catch mistakes?

No. `tools/mutants.py --check` only checks that replacements apply.
Detection requires compiling each selected mutant and running tests that fail
for it, then restoring the source/build. Keep the selected tests and logs;
[Validation](VALIDATION.md) distinguishes applicability from detection.

### Is the code written by AI?

Most is generated with AI tools, including Claude Code, Codex and OpenCode.
Commits record the tool/model in `Assisted-by`. Humans hold the specifications,
test expectations and review decisions; [AI Policy](spec/AI_POLICY.md) explains
the boundaries. An `ai` test marker does not mean human review is complete.

### May I include commercial simulator output or diagnostics in an issue?

No. Keep them out of issue bodies, comments and discussions, including logs,
screenshots, waveforms, attachments and linked resources. Anonymized excerpts,
summaries and third-party results are also excluded. Do not run a commercial
simulator for a report, and do not request such a run from another contributor.

Describe expected behavior in your own words with IEEE 1800-2023 clause numbers
and a minimal example you wrote yourself. Any execution evidence must come from
kirimasim or the project's documented comparison environments, with versions,
commands and the source commit recorded. Do not copy standard text, others'
code or private information. If prohibited material is encountered, do not
quote, summarize or incorporate it; ask the author or maintainers to address it.

All four [issue forms](../.github/ISSUE_TEMPLATE/) display this policy and require
a posting confirmation. Blank issues remain disabled for ordinary contributors.
These controls do not inspect content or completely prevent maintainer blank
issues, API/CLI submissions, or later comments and attachments. See
[GitHub's template configuration](https://docs.github.com/en/communities/using-templates-to-encourage-useful-issues-and-pull-requests/configuring-issue-templates-for-your-repository).

### How can I help?

Submit original minimal bug reports, expectation concerns and specification
proposals through [Issues](https://github.com/kurochan001/kirimasim/issues);
questions and ideas belong in [Discussions](https://github.com/kurochan001/kirimasim/discussions).
Pull requests are currently disabled. Include reproducible inputs, the clause
or contract in question, versions and permitted streams; follow the
[posting policy](#may-i-include-commercial-simulator-output-or-diagnostics-in-an-issue) and avoid private design data.
See [Contributing](../CONTRIBUTING.md).

### Can I print an array or a structure with `%p`?

Yes, for the [supported typed subset](notes/source-pattern-formatting.md).
Use `%p` for named record members and `%0p` for positional records. Constant
and runtime formats, file/string targets and eligible postponed output share
the same behavior. [This runnable example](P1_FEATURES.md#assignment-pattern-formatting)
shows records and a dynamic array. Handle identities and the remaining storage
and object-observation boundaries are still unimplemented.

### Why does `%v` show `631` when `%b` shows `1`?

`%b` is the four-state projection. `%v` preserves the complete captured
strength interval: `[3,6]` is a known 1 whose possible ranks span Weak through
Strong, so it prints `631`. A non-net integral expression uses Strong fallback.
Vectors join scalar tokens with `_`. Runtime formats preserve net provenance;
pinned slang still rejects literal field widths such as `%0v`. See the
[example](P1_FEATURES.md#captured-strength-output) and [scope](notes/strength-format-execution.md).

### Why can a short pulse disappear through a specify path?

The [first scalar implementation](notes/specify-execution.md) uses the approved default inertial rule. Returning to the committed contribution before the pending leading edge cancels that pulse. An identical pending contribution keeps its deadline. A force clamps the public net while these underlying updates continue; release reads the current committed contribution. A distributed functional delay and a path deadline combine by their maximum within one cell; distinct cascaded cells clock their paths from each preceding public arrival. [Simple conditional/fallback paths](notes/specify-state-execution.md) read their conditions at changed functional arrival: an LSB of 1/X/Z enables a path; 0 disables it. A condition-only change does not retime pending output. Vector and conditional edge-sensitive paths, explicit pulse controls and timing checks remain unsupported.
