# Tests of kirimasim that kirimasim-tests does not have

[日本語](kirimasim-tests-port-survey.ja.md) · 2026-11-07.

**Status:** an investigation and a proposal to the maintainer. No test was moved or
changed, and no expected value was changed. Whether and when to move tests is
the maintainer's decision: [TEST_SUITE.md](../spec/TEST_SUITE.md) lists "When to
split the test suite into a separate repository" as not decided, and
[VALIDATION.md](../VALIDATION.md) gives same-stimulus cross-simulator checks to
[kirimasim-tests](https://github.com/kurochan001/kirimasim-tests).

## Question and short answer

Should the tests of `tests/suite/` that kirimasim-tests does not have be added to
kirimasim-tests?

**Not all of them, but a selected set, yes.** Almost none of the 11,604 tests is
in kirimasim-tests, and they reach 2,814 lines of kirimasim that the tests of
kirimasim-tests do not reach. A bulk addition would still not suit that
repository: most of the tests break its rules on comments and expected outputs,
and for most of them no control simulator gives the expected output. A smaller
set does carry the gap. 2,625 tests whose expected output a control simulator
gives, counted as kirimasim-tests counts its oracles, reach 1,173 of those lines; 40 of
them reach all 1,173, and 25 reach 1,149. The proposal at the end starts there.

## What was compared

| | kirimasim `tests/suite/` | kirimasim-tests `tests/` |
| --- | --- | --- |
| Commit | `6e32b04e` | `6a0f260c` |
| Tests | 11,604 (11,545 simulation, 58 elaboration, 1 parsing) | 11,440 `test_*.sv` |
| Generated | 11,498, by 176 generators in `tools/` | 5,005, by 104 generators in `generators/` |
| Size on disk | 95 MB | 126 MB |

The other trees of `tests/` check kirimasim itself and are not candidates:
GoogleTest unit tests (`tests/unit/`), the command line (`tests/cli/`), SIR text
(`tests/dump_sir/`), Contexts (`tests/context/`), operation counts
(`tests/perf_counts/`), VPI probes (`tests/probes/`), fuzz seeds (`tests/fuzz/`),
the tools (`tests/tools/`) and the gate lists (`tests/gates/`).

The runs used Linux under WSL2 on x86-64, Icarus Verilog `13.0 (stable)
(v13_0-25-g07dd50d37)`, Verilator `5.053 devel rev vUNKNOWN-built20261025-1838b6e`,
and a kirimasim of `6e32b04e` built with Clang 20.1.8 and source-based coverage.
Neither control is the fixed release that `:oracle:` names (Icarus Verilog 13.0,
Verilator 5.052; see "Not confirmed").

## How much of it kirimasim-tests already has

| Measure | Tests of `tests/suite/` |
| --- | ---: |
| Same name (without `test_`) | 32 |
| Same source, comments and the module name aside | 22 |
| Same source with identifiers renamed | 23 |
| Shares at least 90% of its 5-token runs with one test of kirimasim-tests | 14 |
| Shares 30% to 90% | 52 |
| Shares less than 30% | 11,538 |

The clauses overlap much more than the text. Of the clauses that `tests/suite/`
cites in `:lrm:`, only six are cited by no test of kirimasim-tests (18.13, 20.14,
23.3.3.6, 30.1, 12.5.3.1 and 13, in 16 tests), and only four pairs of chapters
cited together are new (5 with 15, 20 with 28, 14 with 28, 12 with 21). Text and
citations are rough measures: different text can check one behaviour, and one
clause holds many behaviours. The reach in kirimasim's code below says more.

## What tests/suite/ reaches that kirimasim-tests does not

kirimasim's `src/` was built with line coverage, and each suite was run with its
own runner: `tools/run_suite_test.py` for `tests/suite/`, and kirimasim-tests'
`tools/run_kirimasim.py` for kirimasim-tests. On this build that runner reported
8,700 of 11,172 tests matching and 2,343 stopping at constructs kirimasim does not
support yet; failing tests count too.

| Library | Lines | Both | Only `tests/suite/` | Only kirimasim-tests | Neither |
| --- | ---: | ---: | ---: | ---: | ---: |
| `driver` | 543 | 225 | 46 | 6 | 266 |
| `front` | 13,122 | 10,449 | 927 | 691 | 1,055 |
| `kernel` | 12,397 | 7,583 | 757 | 511 | 3,546 |
| `sir` | 6,683 | 3,893 | 178 | 435 | 2,177 |
| `vm` | 8,687 | 6,048 | 906 | 228 | 1,505 |
| **All** | **41,432** | **28,198** | **2,814** | **1,871** | **8,549** |

The lines that only `tests/suite/` reaches are mostly in `src/vm/vm.cpp` (682),
`src/front/lower_net_array.cpp` (193), `src/kernel/sim.cpp` (149),
`src/vm/scoped_reference.cpp` (122), `src/kernel/managed_event.cpp` (109),
`src/front/lower_container.cpp` (101), `src/kernel/nba_event.cpp` (92),
`src/front/lower_nba.cpp` (85), `src/front/lower_net_ports.cpp` (83) and
`src/kernel/activation.cpp` (78): events kept in objects and containers,
references captured by automatic scopes, arrays of nets and net ports, and
nonblocking assignments with an event control. Of the 2,814 lines, 269 are
reached only by tests that a port could not take (below), mostly in `src/vm/vm.cpp`
(101), `src/kernel/dump.cpp` (58, waveform options) and `src/driver/main.cpp` (46,
command-line options).

Portable tests reach the other 2,545. The 2,625 portable tests that a control
confirms as kirimasim-tests counts its oracles (next sections) reach 1,173 of them, and 40 of
these tests reach all 1,173: 5 reach 936, 10 reach 1,037 and 25 reach 1,149. If
Verilator counted for every test, 4,561 tests would reach 1,373, and 62 of them
all 1,373. The 40 were chosen greedily, each test adding the most lines not yet
reached.

The other way round, kirimasim-tests reaches 1,871 lines that `tests/suite/`
does not. kirimasim's nightly job runs every test of kirimasim-tests, but apart
from the frozen P1 population it only reports the result.

## What a port needs

kirimasim-tests' [Contributing](https://github.com/kurochan001/kirimasim-tests#contributing)
section and `tools/lint_metadata.py` set its rules. Measured against them:

| Item | Tests of `tests/suite/` | Rule of kirimasim-tests | What a port needs |
| --- | ---: | --- | --- |
| File name and place | 11,604 | `test_<what>.sv` in `tests/chNN/`, `:tags: chNN` | A new name and folder (mechanical) |
| Comment explaining what the clauses require | 10,289 have none beyond "Generated by" | Required | Written for each test or generator |
| Line width | 987 with a comment line over 80 columns, 2,752 with code over 100 | 80 and 100 | Rewrapped |
| `:generator:` | 11,498 | Names a generator of `generators/` | The generator moved, or the field removed |
| `:expected_stderr:` (exact warning and error lines) | 3,977; their files: 3,552 empty, 255 warnings only, 166 with an error or fatal line | Exact messages of a tool are not compared | Dropped for the first two kinds |
| `:nondeterministic: yes` | 466 (418 with `.choice`) | No test has it; the runners compare one expected output, and `choice` is not accepted in `:oracle:` | A convention in kirimasim-tests first |
| Illegal designs | 59 with `:expected_error:` (a position) | `:type: elaboration` and `:expected_stderr_regex:` | Rewritten |
| Stops with an error | 249 (exit code 2: 230, 6: 14, 5: 2, 1: 1, error line only: 2) | — | Kept in kirimasim (below) |
| kirimasim options in `:args:` | 19 (`--max-call-depth`, `--wave`, `--seed`) | — | Kept in kirimasim |
| `:timeout:` | 4,267 | Not rejected, not read | Nothing |
| `:oracle:` | 11,089 `spec` alone, 418 `spec choice`, 97 with a simulator | Confirmed with Icarus Verilog 13.0 or Verilator 5.052 where possible | See the next section |

The 249 tests that stop with an error check kirimasim's own run-time errors and
limits: a write or event in the Postponed region, a delay past the largest time,
a wait on a null event, the call-depth limit and time that does not advance. Their
exit codes and messages are kirimasim's interface (the
[Reference Manual](../REFERENCE_MANUAL.md#termination-and-exit-codes)); how other
simulators treat these designs was not measured.

So the tests fall into these classes: 10,811 compare standard output only, 466
allow several outputs, 59 are illegal designs, 249 stop with an error and 19 need
kirimasim options. The first two, 11,277 tests, are the ones a port could take
without changing what they check.

## Running them on Icarus Verilog and Verilator

Each simulation test was built and run with the options of kirimasim-tests'
`tools/run_icarus.py` and `tools/run_verilator.py`, and its standard output was
compared with `<name>.expected` as both suites normalize it. The `.choice` files
were not used, as TEST_SUITE.md has other simulators compared with `.expected`.

| Outcome | Icarus Verilog | Verilator |
| --- | ---: | ---: |
| Gives the expected output | 2,422 | 3,102 |
| Gives the expected output, other exit code | 1 | 27 |
| Runs to the end and differs | 1,478 | 2,778 |
| Differs and stops with an error of its own | 205 | 300 |
| The simulator aborts (an internal check fails) | 384 | 288 |
| The compiler aborts | 643 | — |
| Rejects the design (`sorry` for Icarus Verilog) | 2,227 | — |
| Other compile errors (`build_failed` for Verilator) | 4,183 | 5,047 |
| Timed out (60 s) | 2 | 3 |

For the 11,277 portable tests:

| | Tests |
| --- | ---: |
| Icarus Verilog gives the expected output | 2,418 |
| Verilator does, and the test is `:fourstate: no` (and Icarus Verilog does not) | 207 |
| **Confirmed as kirimasim-tests counts oracles** (Verilator counts only for `:fourstate: no`) | **2,625** |
| Verilator does, but the test is `:fourstate: yes` | 1,936 more |
| Confirmed if Verilator counted for every test | 4,561 |
| A control runs to the end and differs, and none matches | 2,872 |
| No control gives a run to compare | 3,844 |

kirimasim-tests counts Verilator only for `:fourstate: no` tests, because it has
two states. Its README says so where it gives the source of expected outputs
([Where the expected outputs come from](https://github.com/kurochan001/kirimasim-tests#where-the-expected-outputs-come-from)),
and `tools/run_verilator.py` repeats it; `tools/lint_metadata.py` does not check
this pairing. 10,334 tests of `tests/suite/` are `:fourstate: yes`, so most of
Verilator's matches (2,795 of 3,099 on portable tests) cannot be used there as
they are. A test that matches Verilator and prints no x or z might be relabelled,
which is a review per test.

In the largest groups, this was seen:

| Generator (tests) | What was seen |
| --- | --- |
| `gen_named_fork_disable_matrix` (297) | `vvp` aborts in 252 (an internal check in `of_JOIN_DETACH`); Verilator gives the expected output in 288 |
| `gen_delayed_gate_state_matrix` (240) | Icarus Verilog differs in 180. In the one test read, it prints x for a delayed `notif1` with an x control 1 ps after the data changed, where the expected output keeps the earlier value. kirimasim-tests [records](https://github.com/kurochan001/kirimasim-tests/blob/main/docs/simulator-differences.md) that Icarus Verilog gives StX where Table 28-5 gives H or L for `notif` gates with an x or z control (28.6). The other 179 were not read |
| `gen_converting_port_matrix` (324), `gen_port_kind_matrix` (238), `gen_partial_inout_port_kind_matrix` (119) | Icarus Verilog differs in 180, 159 and 71. Not analysed. kirimasim-tests records that Icarus Verilog ignores driver strengths on wired nets (28.12); whether that is the cause is not confirmed |
| `gen_fixed_array_port_matrix` (358) | Icarus Verilog differs in 152: every `variable` case and every array of 2; every `alias`, `copy` and `resolvedcopy` case matches. Verilator gives the expected output in 96 of the 152. Not analysed further |
| `gen_container_conditional_matrix` (351) | `vvp` aborts in 56 (an internal check in `pop_object`); Verilator differs in 324, all `:fourstate: yes` |
| `gen_aggregate_bit_query_matrix`, `gen_runtime_plusargs_matrix`, `gen_runtime_scan_matrix` | Icarus Verilog stops with its own errors: it wants a numeric first argument for `$countbits`, and a variable rather than a part-select for `$value$plusargs` |
| `gen_scalar_fread_matrix`, `gen_multidim_fread_matrix` | Verilator's model aborts in 144 and 132 |

What Icarus Verilog does not compile is mostly what it does not have:
`syntax error` in 3,322 tests (for example `'{default:'0}` for an unpacked array,
`static event` and `interface class`), and `sorry` for unpacked structures (646),
explicit lifetimes (393) and arrays of events (390).

## Expected values to read first

In 495 tests both controls run to the end and both differ from the expected
output. In 87 of them the two controls give the same output. 27 of the 87 stop
with an error and 5 need kirimasim options: the controls agree there because they
have no such limit. The other 55 are portable: `gen_override_initial_matrix` (24),
`gen_port_kind_matrix` (8), `gen_real_delay_rounding_matrix` (6),
`gen_program_execution_matrix` (5), `gen_class_observer_matrix` (5),
`gen_partial_inout_port_kind_matrix` (2), `gen_later_work_override_matrix` (2),
and one each of a hand-written test, `gen_time_format_matrix` and
`gen_event_nba_matrix`.

One test of each family was read:

| Test | Expected | Both controls | Ground of the expected value |
| --- | --- | --- | --- |
| `sched/override_initial_generate_assign_var_four_call` | `seen=0`, `seen=1`, `value=1` | `seen=0`, `value=1` | A procedural continuous assignment is evaluated again when its operand changes (10.6.1), and here the operand is its own target |
| `sched/delay_real_rounding_assign_shortreal_const` | `2.675 at 7267`, `0.015 at 8001` | `7268`, `8002` | The delay is the `shortreal` value, which is narrower than the decimal literal (6.12.1), before rounding |
| `proc/program_grandchild_lifetime` | `keeper` | also prints `unexpected grandchild` | When the initial procedures of a program end, their descendants end too (24.7) |
| `sched/class_observer_interface_nba_expression_slot_calls` | `errors=0` | `errors=1` | Verilator has two states and the test reads an x; Icarus Verilog's reason was not looked at |
| `ports/port_kind_supply0_tri0_output_top_concat_129` | `errors=0` | `errors=3` | Net kinds merged through a port (23.3.3.7); not analysed |
| `proc/program_delayed_net_regions` (hand-written) | the program's lines first | the module's lines first | Program processes and their zero-delay updates run in the Reactive region set before the Active region's waiters (4.5, 24.3) |
| `proc/time_format_matrix` | half rounded away from zero | rounded otherwise | SYSTEM_TASKS.md 3.7; the test's comment names the Icarus Verilog difference |
| `sched/event_nba_front_program_completion` | `final 0` | `final 42` | The approved event-NBA contract of this repository, which ends the pending update with the program. Whether the standard requires it was not confirmed; a test that rests on a contract of this repository does not suit kirimasim-tests as it is |

In none of the eight is a control plainly right, but which side follows the
standard was not confirmed for any of them; a person decides that. Two controls that
agree do not settle it: kirimasim-tests has a test whose expected output had
followed both releases rather than the standard. There are also 143 tests where
Icarus Verilog differs and Verilator gives the expected output; 141 of them are
`:fourstate: yes`.

## What kirimasim-tests would gain in requirements

kirimasim-tests links tests to the requirement rows of `coverage/behaviors.json`.
Of its 12,298 rows, 1,318 have no linked test. Of these, 250 are in a clause that a
test of `tests/suite/` cites, 63 in a clause whose parent is cited and 17 in a
clause whose subclause is cited. The other 988 are in clauses that no test of
`tests/suite/` cites, most in chapters 34 (209), 37 (101), 38 (88) and 40 (46).
So `tests/suite/` could add at most 330 rows (2.7%) before anyone reads a test,
and fewer after.

kirimasim-tests tried this once. On 2026-10-25, when `tests/suite/` had 2,818
tests, it took 540 of them, found 641 candidate rows, registered 21 tests and 58
rows, and removed the other 519 tests so as not to keep tests that no row
counts ([its record](https://github.com/kurochan001/kirimasim-tests/blob/main/docs/existing-test-matching-unregistered-20261025.en.md)).

## What it would cost

Adding all 11,604 tests would double kirimasim-tests and the share of every
simulator in its nightly job. The nightly runs of 2026-11-03 to 2026-11-06 took
2 h 13 min to 4 h 43 min. In the run that began on 2026-11-06 (UTC) the Lyra
job was cancelled after 6 h 0 min, the time limit of a job on GitHub's hosted
runners, and the Verilator job took 1 h 41 min.

## Proposal

1. **Do not add `tests/suite/` in bulk.** The 2026-10-25 attempt and the counts above
   predict the same result.
2. **Port the first candidates first.** These are the 40 tests of
   [`first_candidates.txt`](../measurements/2026-11-07-suite-gap/first_candidates.txt),
   in order of the new lines each adds. All are portable and confirmed as
   kirimasim-tests counts its oracles, and together they reach the 1,173 lines that
   confirmed tests reach and kirimasim-tests does not. Each still needs that
   repository's review: a name and chapter, a comment, a link to requirement rows,
   and the expected output confirmed with the fixed releases. The order is by lines
   of one implementation, not by behaviours, so it is a reading order, not a list
   to copy. By generator, the first candidates come mostly from
   `gen_event_initializer_matrix` (1 test, 398 lines), `gen_port_kind_matrix`
   (1, 277), hand-written tests (5, 162) and `gen_class_observer_matrix` (3, 105).
3. **Then move the generators behind them, not their whole matrices.** kirimasim-tests
   chooses cases with constrained pairwise coverage and boundary cases; a moved
   generator would do the same, rather than bring every combination.
4. **The portable tests that no control confirms** (8,652 tests, reaching
   1,372 more of the 2,814 lines) need an expected value checked in
   the standard alone. kirimasim-tests accepts that (`:oracle: spec`), with more
   review; they come after the first two steps.
5. **Keep in kirimasim:** the 249 tests that stop with an error, the 19 that need
   kirimasim options, the `.choice` part of the 466 nondeterministic tests unless
   kirimasim-tests adopts a convention for allowed sets, and every tree of `tests/`
   other than `tests/suite/`.
6. **In kirimasim, whatever is decided:** read the expected values of the 55
   portable tests above, the 24 of `gen_override_initial_matrix` first. Writing a
   control into `:oracle:` of an existing test changes that test, which goes in a
   change of its own under CLAUDE.md "Changes of expected values go in separate
   PRs"; and as this run did not use the fixed releases, nothing from it goes into
   `:oracle:`.

## Not confirmed

- **The control versions.** Icarus Verilog was the 13.0 stable branch at
  `v13_0-25-g07dd50d37`, not the 13.0 release, and Verilator was a 5.053
  development build, not 5.052. With the fixed releases the counts may differ.
- **Which side is right.** For the 2,872 portable tests where a control runs and
  none matches, which side follows the standard was not confirmed; eight of the 55
  above were read.
- **Lines are not behaviours.** A test can reach a line and judge nothing about it,
  a line can hold several behaviours, and a failing test counts too. The measure is
  of one implementation at one commit.
- **kirimasim-tests' reach is understated.** Its kirimasim runner does not run its
  684 tests with `:dpi_c:` or `:vpi_c:`, and 2,343 of its tests stop at
  constructs kirimasim does not support yet. The 2,814 lines are an upper bound.
- **The runners.** `measure.py` builds and runs each control with the options of
  kirimasim-tests' runners and normalizes the output as they do, but it is not
  those runners, and it compares with every allowed output of a nondeterministic
  test, which they do not do.
- **Rough measures.** Text similarity and clause citations count words, not
  behaviours.
- **Timing.** Each run had 60 seconds; 2 Icarus Verilog runs and 3 Verilator runs
  timed out.

## Reproduce

The scripts and their results are in
[measurements/2026-11-07-suite-gap](../measurements/2026-11-07-suite-gap/).

```sh
D=docs/measurements/2026-11-07-suite-gap
python3 $D/measure.py --kirimasim . --tests ../kirimasim-tests --out out \
    --iverilog-bin <icarus>/bin --verilator <verilator>/bin/verilator -j 24
python3 $D/agreement.py --kirimasim . --both-differ out/both_differ.txt \
    --iverilog-bin <icarus>/bin --verilator <verilator>/bin/verilator \
    --out out/both_differ_controls_agree.txt
python3 $D/requirements.py --kirimasim . --tests ../kirimasim-tests --out out/requirements.json
# coverage.py explains how to build the instrumented kirimasim.
python3 $D/coverage.py --kirimasim . --tests ../kirimasim-tests --binary <build>/src/driver/kirimasim \
    --llvm-bin <llvm>/bin --outcomes out/outcomes.tsv --work work --out out -j 24
```

`outcomes.tsv` holds, for each test, its class, its generator and the outcome with
each control. `summary.json` and `by_generator.json` hold the counts above, and
`coverage.json` the line counts. `measure.py` took about 10 minutes and
`coverage.py` about 16 minutes with 24 jobs.
