# Glossary

[日本語](GLOSSARY.ja.md) · [Documentation guide](README.md) · [Project README](../README.md)

Terms used in the [project README](../README.md) and the [documentation](README.md).
The [Japanese glossary](GLOSSARY.ja.md) gives the Japanese terms.

## Structure

| Term | Meaning |
| --- | --- |
| slang | The SystemVerilog front end library (parsing, elaboration, type checks) that kirimasim uses unmodified, pinned to one commit |
| elaboration | Building the design's instance tree, parameters and types from the sources; slang does it |
| `front` | kirimasim's library that runs slang and lowers its result to SIR |
| lowering | Turning slang's elaborated design into SIR instructions |
| SIR | kirimasim's intermediate representation: storage slots and statically typed instructions, the same whichever tier runs it ([SIR_SPEC.md](spec/SIR_SPEC.md)) |
| `valsem` | The reference implementation of value semantics: bit-by-bit integral operations and an independent container-cell model. It decides what the operators, edges and case matches mean; every fast version is tested against it |
| kernel | The execution kernel shared by every tier: scheduler, value operations, VCD output and the typed heap API; typed SIR/VM heap execution, collection and heap-dependent waits are implemented; additional SystemVerilog container forms, VPI and DPI-C remain pending. It is linked with the user's design, so it depends on neither slang nor LLVM |
| tier 1 / VM | The interpreter of tier 1 (P1). For now it runs SIR directly; compiling SIR to register-based bytecode comes later |
| tier 2 | C++ generated from SIR and built ahead of time, for speed (P3) |
| Context | The object that owns one simulation's run-time state; independent Contexts can run concurrently |
| `analysis` | Analyses for tier 2 (P3): X-reachability (where x can never arrive, so values can stay two-state), static islands, levelization (ordering combinational logic by dependence) and observability (what VPI or force can see, which must not be folded away) |
| static island | A part of the design that tier 2 is to compile as one block for speed (P3; the first rules are in CODEGEN_SPEC.md) |
| slot | A place in the kernel's store for a static variable, a net or an array of them |
| frame / frame variable | The storage of a process's or a subroutine call's automatic variables |
| W4 | The kernel's four-state 64-bit word: two planes a and b; 0 is a=0 b=0, 1 is a=1 b=0, z is a=0 b=1, x is a=1 b=1 |
| chunk | One W4 of a value wider than 64 bits, least significant first |
| P0 to P5 | The project's phases (see [Phases](../README.md#phases)) |

## Values

| Term | Meaning |
| --- | --- |
| four-state / two-state | Types whose bits can be 0, 1, x or z (`logic`, `integer`) or only 0 and 1 (`bit`, `int`); a two-state target turns x and z into 0 |
| x / z | The unknown value and the high-impedance value of a bit |
| strength | How strongly a driver drives a net (supply, strong, pull, weak, ...), used when several drivers meet |
| resolution | Working out a net's value from all its drivers and their strengths ([VALUE_SEMANTICS.md](spec/VALUE_SEMANTICS.md) 5) |
| wired logic | Nets that combine their drivers with AND or OR (`wand`, `wor`) |
| side slot | A two-state slot that a gate writes beside its output: which output bits it drives on its 1 side and on its 0 side, for resolution |
| UDP | User-defined primitive: a gate described by a truth table (`primitive`) |
| override process | The process that holds a procedural continuous assignment (`assign`, `force`) |

## Scheduling

| Term | Meaning |
| --- | --- |
| time slot | Everything that happens at one simulation time |
| region | One of the 17 queues of a time slot that the standard runs in a fixed order (Active, Inactive, NBA, Observed, Reactive, Postponed and others; [SCHEDULER_SPEC.md](spec/SCHEDULER_SPEC.md) 2) |
| active / reactive region set | The regions that run design code (Active, Inactive, NBA) and those that run program and checker code (Reactive, Re-Inactive, Re-NBA) |
| NBA | Nonblocking assignment (`<=`): its update happens later, in the NBA region |
| subscription | A process waiting for changes of some bits of a slot |
| monitor record | A kernel record that re-evaluates an expression when what it reads changes (`wait (...)`, event controls on expressions) |
| held report / flush point | A unique or priority violation or a deferred assertion is held until the Observed region and dropped if its process resumes from an event control first (the flush point) |
| deferred immediate assertion | `assert #0` or `assert final`: its action runs only once the time slot settles |
| static-init process | The process that sets static variables' initial values before time 0's procedures start |
| quiescent | The end of a run because no event is left |
| zero-time limit | The end of a run because time stopped advancing past a limit (`--max-steps`, `--max-zero-delay`, `--max-slot-events`) |
| record release | Giving back the memory of finished processes ([SCHEDULER_SPEC.md](spec/SCHEDULER_SPEC.md) 10.8) |

## Testing and the way of working

| Term | Meaning |
| --- | --- |
| unsupported | A construct kirimasim does not implement yet: it is meant to stop with its position and exit code 2, not run some other way |
| mechanism matrix | A test that a generator makes over the axes of one mechanism (places, ways of reading and writing, spellings, containers), so that a fix covers the neighbors of a bug, not only its own shape |
| generator | A script `tools/gen_*.py` that writes a mechanism matrix and its expected output from the written rules; `--check` tells whether the committed files are up to date |
| container | Where a statement runs: a module procedure, a task, a function, a fork branch, an interface, a generate block, a package; a statement means the same in all of them |
| mutant / negative control | A known mistake that `tools/mutants.py` plants in the code, one at a time, to prove that some test catches it. After each one the code is built and every test runs: the mutant is *caught* when some test fails, and *survives* when none does, which shows a hole in the tests. The code is put back each time. The nightly workflow runs every mutant |
| PerfCount | Regression tests on event counters (not on time), against `tests/perf_counts/expected.json` |
| `--stats=json` | The option that prints those counters |
| oracle | Where a test's expected output comes from (`:oracle:` in its header: the standard, or a named simulator as evidence) |
| `:spec_check:` | Who checked a test against the standard: `ai` (Claude Code) or `human` |
| P1 population | The fixed list of 1,466 tests of the own test suite that the P1 criterion counts |
| mandatory tests | A fixed list of tests that must not fail, apart from any pass rate (TEST_SUITE.md); for P1, all but the one that needs P4 must pass |
| sv-tests | The CHIPS Alliance's public SystemVerilog test collection |
| Icarus Verilog, Verilator | Other open-source simulators, run for comparison and as evidence, not as the truth |
| DPI-C / VPI | The standard interfaces to C code and tools; unimplemented, with the required Icarus-supported subset advanced into P1 |
| FST | A compact waveform file format (later) |
| checked, rule of this document, interpretation, not confirmed | The notes the specifications put on their statements: "checked", read and confirmed in a clause of the standard; "rule of this document", kirimasim's own choice where the standard leaves it to the implementation; "interpretation", how kirimasim reads text that allows two readings; "not confirmed", not checked yet, with how to check it |
