# FAQ

[日本語](FAQ.ja.md) · [Documentation guide](README.md) · [Project README](../README.md)

Answers to common questions about kirimasim. For project terminology, see the [Glossary](GLOSSARY.md).

## Can I use kirimasim on my project now?

Not yet. It is in early development (P1): its first tier, an interpreter,
runs RTL and simple testbenches, and [What works](../README.md#what-works) and
[What does not work yet](../README.md#what-does-not-work-yet) say how far it goes. You
can try it on your design: what it does not support yet is meant to stop
the run with an error at its source position. The current
[class-static boundary](notes/class-static-execution.md) is documented separately.

## Why does it stop at an unsupported construct instead of skipping it?

A simulator that skips what it does not understand prints results that
look right and are not. kirimasim is meant to report every unsupported
construct with its position and exit with code 2 before the simulation
starts ([CLI.md](spec/CLI.md) 5). Parameterized class static storage
currently receives an explicit unsupported diagnostic.

## How does it differ from Icarus Verilog and Verilator?

In two points of focus: four-state values by default, with no optimization
that changes their meaning, and the scheduling of IEEE 1800-2023 with every
region of the time slot. Both simulators are run on kirimasim's tests as
evidence, but the expected results come from the standard
([How it is tested](../README.md#how-it-is-tested)). Verilator compiles the design to
C++ with two-state values and is far faster today
([Simulation speed](../README.md#simulation-speed)); kirimasim's tier 2 (P3) will
compile to C++ too.

## How does it differ from Lyra?

[Lyra](https://github.com/hankhsu1996/lyra) is another open-source
SystemVerilog simulator built on slang. By its README, it compiles each
module, package and interface on its own and aims at a fast edit-run loop
rather than peak simulation speed; its main backend makes machine code
through LLVM, and its C++ backend is there to make its intermediate
representation readable. kirimasim aims first at running designs as IEEE
1800-2023 says, with four-state values and every region of the time slot,
and today interprets its intermediate representation (tier 1); compiling to
C++ comes in P3. On the benchmark designs of
[Simulation speed](../README.md#simulation-speed), Lyra's LLVM backend was faster than
kirimasim on nine of the ten designs it built and about as fast on the
tenth; the version measured did not build the design with gates and UDPs.
Lyra's documents name Linux as its platform; kirimasim runs on Linux and
macOS.

For reference, Lyra's design documents at the measured commit
(`63922502ff0a`) say more: it keeps a four-state value as a value plane and
an unknown plane, and a two-state one without the unknown plane
(`docs/decisions/a-packed-value-is-its-words.md`); its processes run as
coroutines under a stratified event scheduler with the time-slot regions of
the standard (`docs/architecture/scheduling.md`); and building it needs an
x86-64 Linux with glibc 2.34 or newer (`CONTRIBUTING.md`). Which constructs
it supports is, by its documents, what its conformance tests and its code
show; this FAQ does not list them.

## Which version of the standard does it follow?

IEEE 1800-2023. kirimasim always runs slang with `--std=1800-2023`, and
keeps slang's diagnostics strict by default ([CLI.md](spec/CLI.md) 2.1). Where
the standard leaves a choice to the implementation, the specifications say
which reading kirimasim takes;
[UNSPECIFIED_BEHAVIOR.md](spec/UNSPECIFIED_BEHAVIOR.md) collects those
points.

## My design prints something different from Icarus Verilog or Verilator. Which is right?

Look at the standard first. Where it fixes the result, kirimasim's
specifications follow it, and each test is checked against the clauses it
cites; where Icarus Verilog or Verilator differs from the standard, the
test's header says so. Where the standard leaves an order open, for example
which of several processes woken at the same time runs first, more than one
output is correct: kirimasim always takes the same order
([SCHEDULER_SPEC.md](spec/SCHEDULER_SPEC.md) 4.3), and another
simulator may take another. If you think kirimasim is wrong, a bug report
that names the clause of the standard helps most.

## Is there a two-state mode for speed?

No. Values are always four-state, and four-state variables without an
initial value start at x, as the standard says. Speed is to come from tier
2 (P3), whose analysis is to keep values two-state only where an x can
never arrive, so that results do not change ([How it works](../README.md#how-it-works)).

## How fast is it?

Its first tier is an interpreter. On the benchmark designs of
[Simulation speed](../README.md#simulation-speed) it runs at about the speed of Icarus
Verilog, faster on some designs and slower on others. Verilator, which
compiles the design to C++, is much faster. Compiling to C++ is tier 2
(P3).

## Does it run UVM?

Not yet. The required Icarus-supported class and container mechanisms are
P1 work; broader UVM and constrained randomization retain their later-phase
plans. A tested SystemVerilog dynamic-array/queue subset uses the shared
heap; broader container forms and classes remain unfinished. UVM is to run as ordinary SystemVerilog source, with no special
handling of its classes.

## Does it support VPI, DPI-C or cocotb?

Not yet. The VPI mechanisms that Icarus Verilog supports are part of P1
under the approved scope; DPI-C and the rest of VPI belong to later phases
([Phases](../README.md#phases)). cocotb needs VPI, so it does not run yet.

## Can it write waveforms?

Yes, as 4-state VCD: from the design's `$dumpvars`, or from the command
line with `--wave=<file>.vcd` ([WAVEFORM.md](spec/WAVEFORM.md)). FST dependencies are prepared,
but the waveform backend remains unimplemented; `.fst` output is not enabled yet.

## How do I stop a run that does not end?

`--max-time=<time>` stops it at a simulation time (exit code 4, or 0 with
`--max-time-ok`), and `--max-wall-time=<seconds>` after that much
wall-clock time (exit code 4). A design that loops without advancing time
is stopped by the zero-time limits, which have default values (exit code
5). Ctrl-C stops the run with exit code 130. See [Usage](../README.md#usage).

## Does it use more than one thread?

One simulation runs on one thread; multithreaded execution is planned for
P5. Each simulation keeps its state in its own Context, with no global
state ([RUNTIME_ABI.md](spec/RUNTIME_ABI.md) 9), which CI checks under
ThreadSanitizer.

## Which platforms does it run on?

It is developed on macOS (arm64), and CI builds and tests it on x86-64
Linux and arm64 macOS, also with the oldest compilers it supports (see
[Building](../README.md#building)); the nightly runs also build it on arm64 Linux, with
GCC 16 and with Clang 23, and as C++26. Windows is not tested.

## Is slang modified?

No. kirimasim uses [slang](https://github.com/MikePopoloski/slang)
unmodified for parsing and elaboration, pinned to one development commit of its
`master` branch. Bugs found in slang are reported upstream, and kirimasim works
around them until the pinned commit has the fix
([SIR_SPEC.md](spec/SIR_SPEC.md) 6). Where slang's maintainer keeps a
behavior on purpose, kirimasim follows slang, as it did for slang issue
#2040 ([SCHEDULER_SPEC.md](spec/SCHEDULER_SPEC.md) 6.4).

## Is the code written by AI?

Most of it, by AI tools: mostly Claude Code, and also Codex and OpenCode.
Each commit names the tool in its `Assisted-by` trailer. Humans are
responsible for the specifications, the tests and the review;
[AI_POLICY.md](spec/AI_POLICY.md) explains how AI is used.

## How can I help?

For now, with issues: bug reports, wrong expected values, and proposals for
the specifications under `docs/spec/`. Questions and ideas go to
[Discussions](https://github.com/kurochan001/kirimasim/discussions). Pull
requests are not accepted: they are turned off for this repository
([Contributions](../README.md#contributions-specification-proposals-and-issues-only-for-now)).
