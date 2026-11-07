# P1 exit report (M8)

[日本語](p1-m8-report.ja.md). Status: **draft for the maintainer; not approved, and P1 is not judged complete**. English is authoritative.

This draft fills in what a tool can measure. The judgment, and the review by people, are the maintainer's. Unless a row says otherwise, figures are from commit `98bcfea1` on 2026-11-07. Before the judgment, the measurements must be repeated on the revision being judged.

## Summary

Everything a tool measures meets its bar so far. **The open item is the review by people: 0 of 145 checks are done.** Preparing this report also [found a bug, now fixed](#found-while-preparing-this-report).

| [Closed exit list](../spec/P1_SCOPE.md#closed-exit-list-approved-2026-11-07) | Result | Draft status |
| --- | --- | --- |
| 1. sv-tests, at least the pinned Icarus's count | Linux 252 against 211, macOS 252 against 210, of 344 | Meets; measure again |
| 1. Fixed population, 90% | 1,456 of 1,466 (99.3%), 0 failed | Meets; measure again |
| 1. Required tests | All pass, apart from the one planned for P4 | Meets; measure again |
| 1. `valsem` cross-check, 7 nights in a row | 10 scheduled nights without a difference, through 2026-11-05 | Meets; read the latest 7 again |
| 1. M8 report and human review records | This draft; **0 of 145 checks** | **Open** |
| 2. Fixed probe list | `--gate --require-parity` exit 0, no required gap | Meets; measure again |
| 3. Named mechanisms | Tests pass; where each is described is listed below | Not judged |
| 4. Grounds of the reference mismatches | [All 25 written down](p1-reference-mismatch-grounds.md) | Not judged |

## sv-tests

sv-tests `15490e95fd97`, 344 simulation tests. The reference is Icarus 14.0 devel `04cc493`. From the nightly run of 2026-11-05 at `6f5c6984`.

| OS | kirimasim | Icarus |
| --- | --- | --- |
| Linux | 252 | 211 |
| macOS | 252 | 210 |

## Fixed population

`tests/gates/p1_population.txt`, kirimasim-tests `41085fe3432b`, gate 90% (1,320 of 1,466). 1,456 pass, 0 fail, and these 10 are unsupported, which counts as failed:

| Test | Diagnostic |
| --- | --- |
| `ch16/test_comb_stable_multiinstance` | system function `$stable` |
| `ch16/test_sampled_function`, `ch16/test_system_sampled` | system function `$sampled` |
| `ch18/test_randsequence` | statement `RandSequence` |
| `ch25/test_comb_interface_modport`, `ch25/test_comb_vif_array`, `ch25/test_interface_modport_corner` | an interface type |
| `ch7/test_assoc_foreach_struct_key` | an associative array |
| `ch7/test_multi_dim_assign` | type `int$[0:3]` |
| `ch7/test_struct_array_sort` | a dynamic array of a structure |

## Required tests

Of the [required list](../spec/TEST_SUITE.md#list-of-required-tests), every test that exists passes among the 12,965 tests of the dev preset.

- **`context_resume_thread`:** to be written in P4 (approved 2026-10-20).
- **The unit tests, shuffled:** a step of the nightly sanitizer jobs, which passed on 2026-11-05.
- **`cb/input_sample_1step`:** the list says it runs in P4. It passes now.

## `valsem` cross-check

The step of `p1-gates` that fuzzes against `valsem` and the reference scheduler, and the step for wide values. Both passed on every run below, as did the `fuzz` job. The rule is 7 nights in a row without a difference.

| Scheduled run (UTC) | Commit | Other jobs that failed |
| --- | --- | --- |
| 2026-10-27 | `ae5f9dcd` | None |
| 2026-10-28 | `d72e1809` | `mutants` (run cancelled) |
| 2026-10-29 | `51cda314` | `mutants`, `slang-main` |
| 2026-10-30 | `17b7192b` | `README counts`, `svtests-macos`, `mutants` |
| 2026-10-31 | `3550d774` | `README counts`, `svtests-macos`, `mutants` |
| 2026-11-01 | `2c81a591` | `README counts`, `svtests-macos`, `mutants` |
| 2026-11-03 00:59 | `786c94bb` | `sanitizers (ubuntu, ubsan)`, `slang-main`, `mutants` |
| 2026-11-03 23:25 | `91139559` | None |
| 2026-11-04 | `50a897de` | None |
| 2026-11-05 | `6f5c6984` | None |

None of these failures is a difference from `valsem`. Their individual causes were not read for this draft. No run covers `98bcfea1` yet.

## Fixed probe list

`tests/gates/p1_probes.txt`, against Icarus 13.0 on macOS arm64: 186 evaluated, 2 out of scope, no required gap, no independent failure.

| kirimasim | Icarus 13.0 | Probes |
| --- | --- | --- |
| passes | passes | 104 |
| passes | fails to compile or run | 47 |
| passes | runs, other output | 25 |
| fails | fails too | 10 |
| fails | passes | 1 (`real_net`, out of scope) |
| time limit | passes | 1 (`dense_udp_coverage`, out of scope) |

The 10 that neither runs: 5 class property NBAs, a class property `ref`, `->>` with an event control, `wire #d n;`, an associative array and `wait_order`. All are rejected with exit code 2.

## Named mechanisms

Condition 3 asks that each passes its mechanism matrix and is described in P1 Support. This table lists the evidence. **Whether it is enough is the judgment.**

| Mechanism | Tests, all passing | Described in |
| --- | --- | --- |
| Dynamic containers | 943 generated suite tests | [P1 Support](../P1_SUPPORT.md#dynamic-container-foundation) |
| Classes under the approved contract | 1,973 generated suite tests | [P1 Support](../P1_SUPPORT.md#class-static-properties-and-remaining-object-boundary) |
| Programs | 124 generated suite tests, `cli/program` | [P1 Support](../P1_SUPPORT.md#program-completion) |
| Ports and strengths | 1,353 generated suite tests, 110 `Strength*` unit tests | [P1 Support](../P1_SUPPORT.md#strength-resolution-kernel) |
| Switch-level modeling | 297 generated suite tests, `MosStrength` and `Passive*` unit tests | [MOS](source-mos-execution.md), [passive](source-passive-execution.md) |
| First VPI subset | `cli/vpi` and 6 more runners, `Vpi*` unit tests | [P1 Support](../P1_SUPPORT.md#first-vpi-execution-subset) |
| Scalar and state-dependent specify paths | `cli/specify`, `cli/specify_state`, `ModulePath*` unit tests | [scalar](specify-execution.md), [state-dependent](specify-state-execution.md) |
| VCD and FST | 35 generated suite tests, `Fst.*` unit tests | [P1 Support](../P1_SUPPORT.md#command-line-and-waveforms) |

## Review by people

| Item | Checks | Done | Errors found |
| --- | --- | --- | --- |
| Tests checked in full (TEST_SUITE: `spec` only, chapter 4, required) | 39 | 0 | Not checked |
| Tests drawn at 5% (seed 20261016) | 92 | 0 | Not checked |
| Generators drawn at 5%, weighted by tests (seed 20261107) | 14, covering 3,921 tests | 0 | Not checked |
| **Total** | **145** | **0** | |

No test of either suite carries `:spec_check: human` yet.

**Left without a review, by the amendment of 2026-11-07:** 1,372 hand-written tests that the full rule would cover, 174 hand-written tests that 5% would draw, and 252 generators.

The material used for checking copies the standard, so it is not published.

## Found while preparing this report

Checking item 7 below found that no test covered the time-0 order of [SCHEDULER_SPEC 6.1](../spec/SCHEDULER_SPEC.md#61-order-at-time-0), and that the implementation did not follow it: continuous assignments, `always` and `initial` procedures started in source order, so an `initial` written before an `assign` read x. The standard allows either order, so this broke the project's rule and not the standard. It was fixed in `2a922c6a`. Measured around the fix: one existing expected value changed (approved, `3b41cf0f`); the 8,830 simulation tests of kirimasim-tests, the fixed probes and the population did not.

Also, `tools/run_suite_test.py` never read `.choice` as TEST_SUITE says (fixed in `51398348`), and [three behaviours of Icarus](icarus-13-comparison.md#differences-from-the-standard-found-by-the-order-matrices-2026-11-07) differ from the standard.

## Items planned during P1

TEST_SUITE asks this report to say whether items 1 to 6 are done. Item 7 was added later, and is listed because of its finding.

| Item | State | Evidence |
| --- | --- | --- |
| 1. Scheduling matrix | Done | `scheduling` (kirimasim-tests, 325 tests), `tests/suite/sched/`, 48 `:nondeterministic: yes` tests |
| 2. 4-state matrix | Done; the check against Icarus 13.0 is not confirmed | `xz_propagation` (158 tests), `gen_logic_matrix.py`, `gen_select_matrix.py`, `gen_cond_op_matrix.py` |
| 3. Tests of P1's features | Done, with DPI-C in the test repository only | Counts below. DPI-C is a later-phase plan in P1 Support |
| 4. Tests expected to fail | Done | 1,930 in kirimasim-tests, 59 in `tests/suite/` |
| 5. Net matrix | Done | `gen_net_matrix.py` |
| 6. Counting tool | Done on 2026-11-07 | `tools/suite_table.py`, run by the nightly job |
| 7. Choices the standard leaves open | Done on 2026-11-07 | `gen_time0_order_matrix.py` (224 sources), `gen_region_order_matrix.py` (49), `gen_eval_order_matrix.py` (107). Other tests already covered the limits and check options, and TEST_SUITE now names them. `--max-scc-iter` has one test only |

### The table of item 6

From `tools/suite_table.py`, with kirimasim-tests at `d9b55bfb`. "Then" is the kirimasim-tests count of 2026-10-16 in TEST_SUITE.

| What was looked at | Then | kirimasim-tests | `tests/suite/` |
| --- | --- | --- | --- |
| Tests | 1,853 | 11,193 | 11,204 |
| Simulation tests | 1,853 | 8,995 | 11,145 |
| `:spec_check: human` | 0 | 0 | 0 |
| `:oracle:` `spec` alone | 10 | 3,220 | 10,759 |
| `:fourstate: yes` | 163 | 2,777 | 9,934 |
| `:nondeterministic: yes` | 0 | 0 | 396 |
| First clause in chapter 4 | 13 | 412 | 970 |
| `:should_fail_because:` | 0 | 1,930 | 59 |
| Made by a generator | Not counted | 4,758 | 11,098 |
| `$strobe` | 2 | 199 | 33 |
| `$monitor` | 1 | 105 | 39 |
| `$fopen` | 0 | 210 | 522 |
| `$readmem` | 0 | 54 | 77 |
| `$writemem` | 0 | 43 | 68 |
| `$dumpvars` | 0 | 23 | 34 |
| DPI-C import | 0 | 204 | 0 |
| `$test$plusargs`, `$value$plusargs` | 6 | 21 | 53 |

Third-party RTL is left for P3 and later by TEST_SUITE.

## Before the judgment

**The maintainer:**

1. Do the 145 checks. Where an error appears, the chapter's rate rises to 20% and every test of the mechanism is reviewed, as TEST_SUITE says.
2. Read the evidence of conditions 3 and 4.

**Claude Code, on the revision being judged:**

3. Measure sv-tests on both operating systems, the population, the required tests and the fixed probes again, and read the latest 7 nights.
4. Change the reviewed tests to `:spec_check: human`, and show a proposal for any error found.
5. Fill in this report and update the P1 table of the READMEs.

**The judgment:** the maintainer's. CONTRIBUTING plans the first release, 0.1.0, for the end of P1.

## Not confirmed

- The results on Linux of the fixed probes and of the population at `98bcfea1`. These were run on macOS arm64.
- sv-tests with the time-0 fix. The next nightly run measures it.
- The CI of the commits of 2026-11-07, which had not finished when this was written.
- The time one check takes.
- Whether the mechanism groups above are complete. They were gathered by generator and test names.
