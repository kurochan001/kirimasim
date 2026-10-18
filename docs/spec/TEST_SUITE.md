# TEST_SUITE: the test suite

[日本語](TEST_SUITE.ja.md) · This English text is the main version; the Japanese version follows it, and where they differ, this text holds.

Priority: P0. Status: **approved (amendment approved 2026-10-15)**. The points of an independent review and the results of writing tests were applied to the version approved on 2026-10-14. The same day, the rule that tests depending on the unit of time write a timescale was added (approved). The "list of required tests" was fixed on 2026-10-15, after every test in it was written and the clauses of the standard checked. The same day, tests that run in P3 were removed from the P1 passing condition of the list of required tests (approved). Also, the meaning of `spec` was changed to "decided against the clause of the standard", and `:spec_check:`, which says whether an AI or a person did the checking, was added (approved). On 2026-10-16 the items `:expected_exit:` and `:expected_stderr:`, and how the P1 passing conditions are measured, were added (approved). The same day, the rule of comparing without blank lines, `:issue:`, the rules for bug tests and negative controls, and how the control simulators are treated were added (approved). The same day, the gaps in the tests and the plan to fill them, and the rate and procedure of sampling by people, were added (approved). The same day, the item `:generator:` and an update of the list of categories were added (approved). The same day, `:expected_stderr:` was made to compare fatal and info lines too (approved). On 2026-10-18, the category `illegal`, which already held tests, was added to the list of categories (approved). The same day, the rules protecting expected values were made to name `.expected` and `.stderr` files, as the tool now checks them (approved), and CI was made to check pushes to `main` too (approved). The same day, headers that cannot be read as written and the list of allowed skips were added to "What is judged now", and the list to the rules protecting expected values (approved).

This document sets the format, categories, rules of expected values and the simulators compared against for kirimasim's test suite (`tests/suite/`, and in future a conformance test suite published in a separate repository). Unit tests (`tests/unit/`) and PerfCount (`tests/perf_counts/`) share only the rules that protect expected values.

## Purpose

- Base correctness on "expected values whose source is recorded".
- Make a conformance test suite that developers of other simulators can use too, not only kirimasim. For that, the format is compatible with sv-tests.
- Make it possible to trace, from the test file alone, when, by whom and on what grounds an expected value was decided.

## Format of a test

### Where files go, and their granularity

- One test is one `.sv` file (and helper files if needed).
- It goes in `tests/suite/<category>/<name>.sv`. The categories now are `sched` (scheduling), `syntax` (syntax), `values` (value semantics), `proc` (process control), `cb` (clocking blocks), `smoke` (minimal checks that things work), `nets` (nets), `hier` (hierarchy and ports), `gen` (generate), `cont` (continuous assignments) (approved 2026-10-16) and `illegal` (illegal designs that must be rejected). When tests grow, categories are added to this list.
- One test checks one feature only (as sv-tests does). When another feature has to be used, there is a separate test of that feature too.
- The name of a test (`:name:`) is unique within the suite, and equals the file name without its extension.

### Metadata header

In the comment at the top of the file, one item per line in the form `:item: value`.

Items defined by sv-tests (read by sv-tests' tools):

| Item | Required | Meaning |
| --- | --- | --- |
| `:name:` | Required | The test's name |
| `:description:` | Required | A short description of what it checks |
| `:tags:` | Required | The features it checks (separated by spaces) |
| `:type:` | Required (a rule of this suite) | `simulation`, `elaboration`, `parsing` and so on (separated by spaces). sv-tests' default is `parsing elaboration`, so a test that runs always writes `simulation` |
| `:should_fail_because:` | Only when failure is expected | Why it fails. With it, the test passes when the tool fails |
| `:files:`, `:incdirs:`, `:defines:`, `:top_module:` | Optional | Several files, include directories, macros, the top module |
| `:timeout:` | Optional | Seconds. sv-tests' default is 30 |

Items this suite adds (sv-tests' tools warn "Unsupported test param" and ignore them; checked in the implementation of sv-tests' `tools/runner`):

| Item | Required | Meaning |
| --- | --- | --- |
| `:lrm:` | Required | The standard and clause numbers it rests on. Example: `1800-2023 4.6`. Several clauses are separated by spaces |
| `:oracle:` | Required | The source of the expected values ("Source of expected values" below) |
| `:fourstate:` | Required | Whether it uses 4-state values (X, Z). `yes` or `no` |
| `:nondeterministic:` | Required | Whether the result may depend on a point whose order the standard does not set. `yes` or `no` |
| `:expected_stdout:` | Required for `simulation` | The expected standard output of the run ("Expected standard output" below) |
| `:expected_error:` | Required with `:should_fail_because:` | The position and text of the intended diagnostic. `<line>:<column>: error: <text>` |
| `:spec_check:` | Required when `:oracle:` has `spec` | Whether an AI or a person checked against the standard. `ai` or `human` ("Source of expected values" below) |
| `:issue:` (approved 2026-10-16) | Optional | The numbers of the issues that led to the test (separated by spaces). The test's name and place are decided by the clause of the standard and the mechanism, not by issue numbers |
| `:expected_exit:` (approved 2026-10-16) | Optional | kirimasim's exit code (CLI.md 5). 0 if not written. It is kirimasim's rule, so it is not used to judge other simulators |
| `:generator:` (approved 2026-10-16) | Required only for tests made by a generator | The generator that made it, and the version of its table of rules. `<file name under tools>@<version>` (for example `gen_net_matrix.py@1`). Versions start at 1 and rise when the table of rules changes. Generated tests are not edited by hand; the generator is fixed and they are made again |
| `:expected_stderr:` (approved 2026-10-16; fatal and info lines added the same day) | Optional | In the form `@<name>.stderr`, the complete sequence of these lines kirimasim writes to standard error: lines containing `: warning: ` or `: error: `, and lines starting with a source position (`<file>:<line>`) and containing `: fatal: ` or `: info: ` (`$fatal` and `$info`; SYSTEM_TASKS.md 10.1). kirimasim's own `kirimasim: info:` lines (the version line, the line of the reason for ending) are not compared (the version line changes with each commit). Of a file's path, only the part from the test's file name on is compared. It depends on kirimasim's form, so it is not used to judge other simulators. `.stderr` files too fall under the rules protecting expected values |

Rules:

- Do not copy text of the standard into the header. Write descriptions in your own words.
- `:expected_error:` follows the form of kirimasim's diagnostics (slang's form: `<file>:<line>:<column>: error: <text>`). Other simulators are not required to give the same text. They are judged only on "did it fail", as sv-tests' rules say.
- A `:should_fail_because:` test passes only when the exit code is 2 and also standard error contains `<file name>:<value of :expected_error:>`. Failing with an unrelated error does not pass (`tools/run_suite_test.py`).
- A test whose output changes with the unit or precision of time (one that prints a nonzero time with `%t`, or uses `$timeformat`, `$printtimescale`, fractional delays or delays with units) writes a `` `timescale `` before its first design element. Without a timescale, the unit and precision of time are up to the implementation (checked: 3.14.2.3), and the default unit of `%t` is the smallest precision in the design (checked: 20.4.3), so without one the expected values depend on the implementation.

### Expected standard output

- A single line is written in `:expected_stdout:`.
- Several lines go in `<name>.expected` next to the test, and the header says `:expected_stdout: @<name>.expected`. The way sv-tests reads headers keeps only the last value of an item written several times, so repeating the item line by line is not used.
- **Blank lines (approved 2026-10-16):** when comparing, blank lines (including lines of only white space) are removed from both the result of the run and the expected values. This matches the rule of the test suite repository's README (`tools/normalize_output.py`). That repository's expected values were made without blank lines, and a comparison that removes only trailing blank lines would fail tests whose output is correct (checked on 2026-10-16 with 4 tests of our own population). The number of blank lines itself is not compared.
- Only standard output is compared. The line of version and options kirimasim gives at the start of a run is removed before comparing (CLI.md decides how).
- A `:nondeterministic: yes` test writes the set of allowed results in `<name>.expected`, the candidates separated by lines of only `---`. The run passes if its result equals any one of them. The set of allowed results is not decided from observed results alone; a person checks it against the standard.
- `<name>.expected` holds expected values, so it falls under the rules protecting expected values (`tools/check_expected_change.py` checks).

### Relation to sv-tests' self-checks

sv-tests judges a `simulation` test by the `:assert: <Python expression>` lines in the simulator's output (checked in sv-tests' `tools/logparser.py`: the test fails if any line's expression is false). So the `simulation` tests of this suite are encouraged to print, apart from the lines compared by `:expected_stdout:`, `:assert:` lines that check the expected values themselves. This is not required. `:assert:` lines are part of standard output too, so when printed they are also included in `:expected_stdout:`. Then the judgment is meaningful when the tests are taken into sv-tests as well.

### What is judged now (approved 2026-10-16)

- A `simulation` test is run, and `:expected_stdout:`, `:expected_exit:` and `:expected_stderr:` are compared (`tools/run_suite_test.py`). A test using a construct not supported yet (exit code 2 and an `unsupported` diagnostic) counts as skipped, neither passed nor failed, when it is on the list of allowed skips (below).
- A test expected to fail passes on exit code 2 together with the diagnostic at the intended position (the rule above).
- **Headers that cannot be read as written (added 2026-10-18, approved):** a test fails when the file does not start with its header comment, when an item is not in the tables of "Metadata header" (a misspelled item would otherwise be dropped without a word), when a word of `:type:` is not a kind of test (`preprocessing`, `parsing`, `elaboration`, `simulation`), when a required item is missing, or when `:fourstate:` or `:nondeterministic:` is not `yes` or `no`. An empty candidate among the allowed results of a `:nondeterministic: yes` test (a stray `---`) fails too, since it would pass a run that prints nothing (`tools/suite_header.py`, `tools/run_suite_test.py`).
- **Allowed skips (added 2026-10-18, approved):** the tests that may be skipped as unsupported are listed in `tests/suite/skips.txt`, one per line with its reason. A test not on the list that stops at an unsupported construct fails, and a listed test that runs and passes fails until it is taken off the list, so that a test that passed cannot turn into a skip unseen. Each line must name a test that exists (`tools/check_suite_files.py`).

## Categories

| Category | Content | Treatment |
| --- | --- | --- |
| A | Tests we wrote | Published. Licence Apache-2.0 |
| B | Copies of examples in the standard | Not published. Rewritten with different identifiers, structure and values, and moved to A |
| C | Third-party RTL (CVA6, Ibex, OpenTitan and so on) | Not bundled. Referred to by scripts that fetch them, under their own licences |

Only category A may go in the published suite. When rewriting category B, do not rewrite while looking at the original example; first write down, in your own words, the rule to be checked, then write a new test.

## Source of expected values (`:oracle:`)

| Value | Meaning |
| --- | --- |
| `spec` | Decided against the relevant clause of the standard. `:spec_check:` says whether a person or an AI did the checking |
| `iverilog@<version>` | The result of that version of Icarus Verilog |
| `verilator@<version>` | The result of that version of Verilator (2-state only) |
| `consensus` | Agreement of several simulators, with grounds in a clause of the standard |

Rules:

- With several grounds, list them separated by spaces. Example: `:oracle: spec iverilog@13.0`.
- A test with neither `spec` nor `consensus` and only one simulator's result is taken as "needs checking". When it is checked in the standard, `spec` is added. The list of tests needing checking can be made mechanically from the headers.
- A test that writes `spec` writes `:spec_check:`. Its value is `ai` when an AI (Claude Code) read the clause of the standard and checked, and `human` when a person checked it in the clause. When a person checks it, `ai` is replaced by `human`. The expected values of `ai` tests are, as AI_POLICY.md says, the AI's proposals, not ones a person checked one by one. The published README says so too.
- Always write the version. Use the release versions fixed under "For the grounds of expected values" in "Simulators compared against" below.
- Output of commercial simulators is not used for published expected values.
- Results of unpublished simulators are not written in `:oracle:`. Such results are recorded only in a private inventory table. A test with no other grounds stays out of the published suite until a person checks it in the standard or with the published simulators above.
- Results of a simulator known not to follow the standard are not used as grounds. Differences are written in the test's comments with clause numbers.
- **The control simulators are one piece of evidence (approved 2026-10-16):** the results of Icarus Verilog and Verilator are not the correct answer but one piece of evidence. When the controls disagree with each other, or a control does not support the feature (Icarus's classes, Verilator's 4 states and so on), the clause of the standard decides. Examples in the standard that state their results are strong grounds, so they are made into tests by category B's procedure (rewritten in our own words).
- **Expected values are assumed to have errors (approved 2026-10-16):** whether an AI or a person wrote them, expected values can be wrong. `:spec_check: ai` tests are sampled by people and checked against the standard. The rate and procedure are set in "Gaps in the tests and the plan to fill them" (approved 2026-10-16).

## Bug tests, and checking that tests fail (approved 2026-10-16)

- **Mechanism matrix:** when fixing a bug, add not one reproducing example but a matrix test of the mechanism. The matrix's axes are: the place (module variables, variables of static and automatic subroutines, class properties, interface members, generate scopes, package variables, array elements, part-selects, structure members and so on); the way of reading or writing (reads, blocking assignments, NBAs, continuous assignments, force and release, hierarchical references, `ref` arguments, sensitivity of `@` and `wait`, `$monitor`, VPI, waveforms); the spelling of the name (short names, hierarchical names, `C::s` and `obj.s`, through a typedef and so on); the container (module procedures, class methods, automatic tasks, fork branches, interfaces, generate, packages). Matrices are not written by hand but made by generators. Generators and the sources of the expected values of the tests they make are managed together.
- **Mechanisms whose matrices are made first:** hierarchical names (23.6), lifetimes of variables (6.21), NBAs (10.4.2), event controls (9.4.2), bit- and part-selects (11.5.1), process control (9.7), generate loops (27.4), virtual interfaces (25.9), force and release (10.6.2), package references (26.3). All come down to where a variable is placed, what a name binds to, and when a process wakes. Each matrix is made in the stage that implements the feature.
- **Negative controls:** check that the judging tools and the tests really fail on a wrong implementation. A nightly job makes builds with chosen errors put, one at a time, into `valsem`, the kernel's word operations and the scheduler, and checks that each makes some test fail. An error that makes no test fail is recorded as a hole in the tests. The judging tools are tested to fail on a copy with one expected value changed.
- **No tests that do not run:** every `.sv` under `tests/suite/` is registered with CTest by glob. A `.expected` or `.stderr` with no matching `.sv` fails CI. The number of tests skipped as unsupported is always given next to the number passed.
- **Judging rules live in one tool:** only `tools/run_suite_test.py` decides pass or fail for this suite. The sv-tests comparison (`tools/svtests.py`) is a separate tool that judges by sv-tests' rules, and is not used to judge this suite.
- **Agreement of the two tiers (from P2):** once tier 2 exists, every test of this suite runs on both tier 1 and tier 2, and equal output is a required condition of CI. The number of tests that run on only one tier is given.

## Gaps in the tests and the plan to fill them (approved 2026-10-16)

### The present state (counted 2026-10-16)

The test suite repository (kirimasim-tests at `823ef30`, 1853 `simulation` tests) and `tests/suite/` of the main repository (21 tests) were counted by header items and by words in the source with comments and strings removed. Rows counted by words are rough.

| What was looked at | Count | Gap |
| --- | --- | --- |
| `:spec_check:` | All 1853 `ai`, 0 `human` | No expected value has been checked by a person. If the AI and the control simulator make the same error, it is not found |
| `:oracle:` | `spec iverilog@13.0` 1341, `spec verilator@5.052` 497, both 5, `spec` alone 10 | The 502 based on Verilator are all `:fourstate: no` and check only the 2-state range |
| `:fourstate: yes` | 163 (8.8%) | For a conformance suite of a simulator whose default is 4 states, few tests use X and Z |
| `:nondeterministic: yes` | 0 | No test checks the set of allowed results where the standard does not set the order (4.7) |
| Tests whose first clause is in chapter 4 (scheduling) | 13 | Almost no test checks how the regions proceed (each region of 4.4.2, 4.5). This is also the result of removing tests whose order the standard does not set |
| `$strobe`, `$monitor` (21.2.2, 21.2.3) | 2, 1 | Few tests check that values are read in the Postponed region |
| File I/O (21.3), `$readmem` and `$writemem` (21.4), `$dumpvars` (21.7), DPI-C import (35) | 0 each | No tests of features made in P1 |
| `$test$plusargs`, `$value$plusargs` (21.6) | 6 | Few |
| Net strengths, wand and wor (6.6.3), trireg (6.6.4), tri0 and tri1 (6.6.5), switches | 10, 17 (wand to tri1 together), 5 | Few combinations of net resolution (28.12) and strengths (28.11 to 28.15) |
| `:should_fail_because:` | 0 in the test suite repository, 1 in the main one | Almost no test checks that illegal input is rejected |
| Third-party RTL (category C) | 0 | Not checked whether it runs with the size and style of real designs |

How tests grow: the candidates usable with the way tests were collected so far were used up on 2026-10-15. From now on, tests are added by writing them from clauses of the standard, and by matrices made by generators (the section "Bug tests, and checking that tests fail" above).

### Sampling by people (checking `:spec_check: ai`)

The rate and procedure of the sampling by people in "Expected values are assumed to have errors" above are these:

- **Checked by people in full:** tests whose `:oracle:` is `spec` alone (no simulator backs them), tests whose first clause is in chapter 4, `:nondeterministic: yes` tests, and the tests in the list of required tests. As of 2026-10-16, 23 tests in the test suite repository and the 16 required tests of the main one, 39 in all.
- **Of the rest, 5% at random:** chosen with a fixed seed; the list of chosen tests and the seed are recorded. As of 2026-10-16, 92 of the other 1830 tests in the test suite repository.
- **When an error is found:** the sampling of that chapter rises to 20%, and every test of the same mechanism is reviewed. Corrections of expected values go, by the rules protecting expected values, in PRs separate from implementation.
- **Rules of generators:** for tests made by a generator, a person checks the generator's table of rules (truth tables, tables of order and so on) against the clauses of the standard. One check covers every test it makes.
- **Record:** tests checked by a person are changed to `:spec_check: human`. The results of the sampling (how many were checked, how many errors were found) go in the report at the end of P1 (M8). The material used for checking copies text of the standard, so it is not published.
- Newly added tests get the same rate.

### What is added during P1

Each is added in the stage that implements the feature, in the same PR as the implementation (they are new tests, so this does not break the rules protecting expected values).

1. **Scheduling matrix:** forms whose order the standard sets (the order of statements in one process; the order Active, Inactive, NBA, Observed, Postponed; moving to Inactive by `#0`; evaluations after NBA updates; `$strobe` and `$monitor` reading in Postponed; the point of final) are combined by a generator. Forms whose order the standard does not set (4.7) are written with `:nondeterministic: yes` and the set of allowed results. A person checks the set in the standard (the rule of "Expected standard output" above). Each section of SCHEDULER_SPEC.md gets at least one test.
2. **4-state matrix:** for operators (11.4), conditions (12.4), case matching (12.5), edges (9.4.2) and net resolution (6.6, 28.12), a generator makes matrices whose axes are the positions of X and Z, widths and signedness. The generator computes the expected values from the tables of the standard, and they are checked against Icarus 13.0's results. Differences are decided by the clause of the standard and written in the test's comments. Expected values are not made from kirimasim's `valsem` (that would make the answer from what is being judged).
3. **Tests of P1's features:** file I/O (21.3), `$readmem` and `$writemem` (21.4), plusargs (21.6), DPI-C import (35), waveforms. Whether sv-tests' format has items for the C source of DPI-C and for passing plusargs to a test is not confirmed (check by reading sv-tests' `tools/runner`). If not, a proposal for items of this suite is made in the stage that implements the feature. Waveform tests are written after the format of waveform summaries in "Not confirmed, to be decided later" below is set.
4. **Tests expected to fail:** for each feature implemented, add at least one form the standard makes illegal (`:should_fail_because:` and `:expected_error:`). Diagnostics of unsupported constructs are kirimasim's own rule, so they are checked in `tests/cli/`, not in this suite.
5. **Net matrix:** in the stage that implements instances, ports and nets, make a matrix whose axes are the kinds of net (6.6.1 to 6.6.6), the number of drivers, and strengths (28.11 to 28.15).
6. **Counting tool:** put the tool that makes the table above in `tools/`, and have a nightly job produce the table. The table also goes in the report at the end of P1 (M8).

Where they go and how their expected values are treated:

- Generators go in `tools/` of the main repository, and the tests they make are committed too. When changing a generator changes the expected values of existing tests, it goes, by the rules protecting expected values, in a PR separate from implementation.
- The header of a generated test writes the generator's name and the version of its table of rules in `:generator:` (the table of "Metadata header" above (approved 2026-10-16)). A generator can check with `--check` that the committed tests equal its output, and that check is registered with CTest.

### What is left for P2 and later

- Third-party RTL (category C): run Ibex, CVA6 and OpenTitan, referred to by scripts that fetch them, after checking their licences. This is the "real designs" layer (weekly) of the design specification.
- Differential fuzzing: compare small randomly generated designs between Icarus, tier 1 and tier 2. Whether to adopt a generator is decided in P2, as the design specification says.

### Relation to the passing conditions

- The numbers and populations of the P1 passing conditions (the section "How the P1 passing conditions are measured") do not change. The list of the population is fixed, so newly added tests do not enter the denominator of the 90%.
- Whether 1 to 6 above are done, and the results of the sampling by people, are written in the report at the end of P1 (M8). They are not part of the passing conditions.

## Rules protecting expected values

- A change that changes, removes or loosens the expected values of existing tests goes in a PR separate from changes of the implementation in `src/`. This covers:
  - existing items of `tests/perf_counts/expected.json`
  - existing lines of `tests/**/expected*`, `tests/**/*.expected` and `tests/**/*.stderr`
  - `:expected_stdout:` in `.sv` files, and the bodies of existing tests
  - removing or renaming existing test files
  - adding a test to `tests/suite/skips.txt` (added 2026-10-18, approved): allowing a skip loosens what the test expects, while taking one off the list does not
- Adding new tests and their expected values may be in the same PR as the implementation. A person checks the grounds.
- A PR that changes expected values states the reason and the clause numbers of the standard, and a person approves it.
- CI's `tools/check_expected_change.py` checks the diff from the merge-base of base and head to head for a pull request, and each pushed commit on its own for a push to `main`. Unit tests in `tests/unit/` may be fixed together with the implementation; then the changed assertions are shown as warnings, and a person looks at them in review.

## Simulators compared against (fixed versions)

There are two purposes of comparing, and each uses its own versions (checked 2026-10-14). Both are fixed by commit SHA, not by branch name. A version is raised by a PR that changes this section, after checking the effect on expected values and passing conditions.

### For passing conditions (sv-tests pass rate)

Passing conditions such as "as many as Icarus or more on sv-tests' simulation tags" and "as many as Verilator or more" are compared against the development commits sv-tests fixes as submodules, so that the conditions are the same as sv-tests' published results. Fixing the sv-tests commit fixes the Icarus Verilog and Verilator commits too. The three are always raised together.

| Simulator | Version | Commit | How checked |
| --- | --- | --- | --- |
| sv-tests | master as of 2026-10-14 | `15490e95fd978ada1c1dc59b7c4b53a7edb23b78` | The GitHub API. Licence ISC |
| Icarus Verilog | development (master, 2026-10-10) | `04cc493506bd4d75c861cfc9961e2a56517d7cc6` | `third_party/tools/icarus` at the sv-tests commit above |
| Verilator | development (master, 2026-10-05) | `cb3a916d0c8972a6011ecea386202604c593e0ea` | `third_party/tools/verilator` at the sv-tests commit above |

### For the grounds of expected values (`:oracle:`)

A test's `:oracle: iverilog@<version>` and `verilator@<version>` use release versions, so that people using the published suite can install the same version from a package and reproduce the results, and so that bugs in the middle of development are not taken into expected values.

| Simulator | Version | Commit | Value written in `:oracle:` |
| --- | --- | --- | --- |
| Icarus Verilog | 13.0 (tag `v13_0`, 2026-03-02, the latest release) | `dfeee909ed9f20b4870dd93423156c0170c0e1ff` | `iverilog@13.0` |
| Verilator | 5.052 (tag `v5.052`, 2026-09-05, the latest release) | `ea338be98e1e838d3518809ce8899f85a009963c` | `verilator@5.052` |

### Notes on use

- Icarus Verilog and Verilator are only run as external commands. Their source is not read (clean room). Development versions too are only built into executables from the fixed commit.
- A test whose result differs between the release and the development version may be one where either fixed or introduced a bug. Check it in the standard and write it in the test's comments with clause numbers. Neither result is used as grounds of expected values until it is checked.
- Icarus Verilog is started as `iverilog -g2012`, then `vvp -n`. When Homebrew has both 13.0 and a development version, the link may point to the development version. When making expected values, name 13.0's executable explicitly (for example `$(brew --cellar icarus-verilog)/13.0/bin/iverilog`).
- Icarus Verilog from apt on Ubuntu 24.04 is 12.0 (the version used in the design survey). For comparisons in CI, build it from the development commit in the table above with `tools/build_icarus.sh` (approved 2026-10-16; the section "How the P1 passing conditions are measured").
- Verilator is used only for comparisons in the 2-state range. It is not used as grounds of expected values for tests that use 4 states (`:fourstate: yes`).
- Checked example: `sched/nba_order_same_proc.sv` printed `q=2` on both Icarus Verilog 13.0 and Verilator 5.052 (`--binary`) (macOS arm64). Not confirmed at the development commits.

## List of required tests

Apart from pass rates, tests that must not fail are included in the passing conditions. The list below was made from the items named in the design and fixed on 2026-10-15 (approved). The P1 passing condition includes "every test in this list passes, except those that run in P3 (`cb/input_sample_1step`)" (approved 2026-10-15). The P3 passing condition includes that every test in the list passes. Adding or removing items or replacing tests is done by a PR that changes this section, and a person approves it.

| Group | Test | What it checks | Clause | State |
| --- | --- | --- | --- | --- |
| Scheduling | `sched/nba_order_same_proc` | NBAs of one process take effect in statement order | 4.6 | Exists |
| Scheduling | `sched/comb_order` | The order of statements in always_comb, and that it does not wake again on its own writes | 9.2.2.2, 9.2.2.2.1 (checked) | Exists |
| Scheduling | `sched/pulse_history` | A process waiting on a 0→1→0 in the middle of always_comb wakes | 4.3, 4.7, 9.4.2 (checked) | Exists |
| Scheduling | `sched/zero_delay_forever` | `forever #0` stops at the limit without advancing time, with a diagnostic | 12.7.6, 9.2.3 (checked; the limit is a rule of SCHEDULER_SPEC 16.2) | Exists (exit code and diagnostic checked in P1) |
| Scheduling | `sched/empty_forever` | An empty `forever` stops at the limit, with a diagnostic | 12.7.6, 9.2.3 (checked; the limit is a rule of SCHEDULER_SPEC 16.2) | Exists (exit code and diagnostic checked in P1) |
| 4-state | `values/cond_truth` | Conversion of a condition to true or false (with no 1 and some X or Z it is unknown, and `if` takes unknown as false) | 12.4, 11.4.5, 11.4.7 (checked; how truth is decided is the interpretation of VALUE_SEMANTICS 3.5) | Exists |
| 4-state | `values/known_inputs_x` | Division by 0 and out-of-range reads make X even with known inputs | 11.4.3, 7.4.5 (checked) | Exists |
| 4-state | `values/and_or_truth` | 4-state truth tables of the bitwise operators | 11.4.8 (checked) | Exists |
| 4-state | `values/cond_op_ambiguous` | How bits are combined when the condition is unknown (a bit z on both sides is x) | 11.4.11 (checked) | Exists |
| 4-state | `values/casez_expr_dontcare` | `casez` and `casex` also ignore z and x on the expression's side | 12.5.1 (checked) | Exists |
| 4-state | `values/edge_table` | The decision of posedge and negedge (between x and z is not an edge) | 9.4.2 (checked) | Exists |
| case | `values/unique_case_violation` | When a violation of unique case is reported | 12.5.3, 12.5.3.1, 12.4.2.1 (checked) | Exists (the report on standard error checked in P1) |
| Process control | `proc/fork_join_any`, `proc/disable_fork` | The three kinds of fork, and disable fork | 9.3.2, 9.6.3 (checked) | Exists |
| Clocking blocks | `cb/input_sample_1step` | Sampling of inputs with `#1step` | 14.4, 14.10, 14.13 (checked) | Exists (runs in P3) |
| Syntax | `syntax/missing_semicolon` | A syntax error is reported at the intended position | 6.8 | Exists |

The clauses marked "checked" were checked in the standard while writing VALUE_SEMANTICS.md or the tests.

## How the P1 passing conditions are measured (approved 2026-10-16)

The design specification sets the P1 passing conditions as "as many as Icarus or more on sv-tests' simulation tags", "90% or more of our own RTL tests pass" and "no differences in the cross-check with `valsem`". The list of required tests (the section above) is also part of the passing conditions. This section sets the populations of the first three and how they are judged.

### As many as Icarus or more on sv-tests

- **Population:** the handwritten tests under `tests/` of the fixed sv-tests commit (the table of "Simulators compared against") whose `:type:` contains `simulation`. Counted on 2026-10-16: 344 (of which 53 expected to fail, 79 with the UVM tag).
- **Left out:** tests that `generators/` makes from external repositories that are submodules (core RTL, UVM examples, ivtest of the Icarus repository and so on). They need fetching external repositories, and ivtest is part of the Icarus repository, so leaving it out keeps the line with the clean-room rule (do not read Icarus's source) clear. sv-tests' published results include them, so the counts do not match those results.
- **Judgment:** the same rules as sv-tests' `tools/runner` (checked at the fixed commit on 2026-10-16). An exit code of 126 or more fails (a crash). A test with `:should_fail_because:` passes when the exit code is not 0; one without it passes when it is 0. A `simulation` test further needs no false `:assert:` line in standard output and standard error together. The time limit is `:timeout:` seconds (default 30); exceeding it fails.
- **Condition added for kirimasim only:** a test expected to fail that kirimasim fails with an unsupported construct (an `unsupported` diagnostic) does not count as passed, since it cannot be said to fail for the intended reason.
- **Invocation:** kirimasim is `kirimasim [+incdir+<dir>]... [+define+<name>]... [--top=<name>] <file>...`. Icarus is run after `iverilog -g2012 -o <output> ...`, as sv-tests' Icarus runner does.
- **Icarus executable:** the one built with `tools/build_icarus.sh` from the development commit in the table of "Simulators compared against" (the source is only fetched and built, not read). The autoconf, bison, flex and gperf used for the build were checked to suffice as installed on the local macOS (2026-10-16). CI builds it by the same procedure and caches the result.
- **Pass:** over the whole population, kirimasim's passes are at least Icarus's. Counts per chapter (the first number of `:tags:`) are also tabled and reported, but not used to pass or fail.
- The tool is `tools/svtests.py` (it selects the population, runs both and makes the table).

### 90% of our own tests

- **Population:** the `simulation` tests at the fixed commit of the test suite repository (kirimasim-tests) that use no feature of P3. "RTL tests" is read as tests written only with features within P1 of the design specification (the direction approved 2026-10-15).
- **Deciding P3 features:** `tools/p1_population.py` searches the source, with comments and strings removed, for these words, and leaves out of the population any test with one of them: classes (`class`), constrained random (`rand`, `randc`, `randomize`, `constraint`), coverage (`covergroup`, `coverpoint`), concurrent assertions (`property`, `sequence`, `expect`), clocking blocks and cycle delays (`clocking`, `##`), `program`, `mailbox`, `semaphore`, the `process` class, dynamic data (`[$]`, `[]`, `new[`, index types of associative arrays), DPI-C export, `checker`, `let`.
- As of 2026-10-16 (kirimasim-tests at `823ef30`), 1466 of 1853 tests are in the population.
- **Fixed:** the list of the population (the sequence of test names) and the test suite commit it came from are kept in `tests/gates/p1_population.txt`. Changing the list is done by a PR that changes this section, and a person approves it. When the expected values of tests are corrected, the list stays as it is.
- **Judgment:** the same as "What is judged now" above (`:expected_stdout:`, `:expected_exit:`, `:expected_stderr:`). Tests skipped as unsupported count as failed.
- **Pass:** 90% or more of the population (1320 or more of 1466).

### Cross-check with `valsem`

- **Every CI run:** the fuzzer comparing `valsem` with the kernel's word operations (`W4Ops.*`), the example tests of `valsem` (`Valsem.*`), and the cross-check with the scheduler's reference model (`Scheduler.*`) pass with fixed seeds.
- **Nightly:** the same fuzzer and cross-check with the reference model run with seeds made from the date, 100 times as many iterations.
- **Pass:** no difference at all in 7 consecutive nightly runs before the judgment at the end of P1. When a difference appears, its seed is recorded, it is fixed, and the count starts again.

### When it is measured

- At the judgment at the end of P1 (M8), the three are measured and put together in a report. In intermediate stages too, a nightly job makes the same tables to watch progress.

## How to run

```sh
ctest --preset dev -L Suite      # the test suite only
```

Each `.sv` is one CTest test (`suite/<category>/<name>`). `tools/run_suite_test.py` judges.

## Not confirmed, to be decided later

- When to split the test suite into a separate repository.
- How to provide Verilator's development version in CI (decided before the P3 passing condition "as many as Verilator or more").
- The format of expected values for waveform summaries and sequences of observed events (added in P1; once added, they fall under the rules protecting expected values).
