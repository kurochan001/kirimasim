# Audit of the expected outputs of the P1 population

[日本語](oracle_audit.ja.md) · This English text is the main version; the Japanese version follows it, and where they differ, this text holds.

Status: **decided** (2026-10-21). The owner accepted the recommendations of section 4: no test is excluded, no expected output is changed, and the README counts stay. An AI model (Claude Code) made the verdicts below; no person has checked them one by one.

## 1. Why

Most expected outputs of the test suite were recorded with Icarus Verilog 13.0, and the rest, for tests that Icarus cannot compile, with Verilator 5.052 (`:oracle:`; kirimasim-tests README). Where one of them does not follow IEEE 1800-2023, its result can be in an expected output, and kirimasim, which passes the tests of the P1 population, may then do the same. This audit looks for such tests in the population of `tests/gates/p1_population.txt` (1466 tests of kirimasim-tests c983cf799b4d), and separates them from tests whose result the standard leaves open (4.7 and others).

## 2. How

`tools/oracle_audit.py` runs every test of the population with Icarus Verilog 13.0 (`iverilog -g2012`, `vvp -n`), with Verilator 5.052 (`--binary --timing --x-assign unique --x-initial unique`, run with `+verilator+rand+reset+2` and the seeds 1, 7, 42 and 2026, and with `+verilator+error+limit+1000000` as the runner of the suite passes now) and with kirimasim (main, 900dbf3), and writes [oracle_audit.csv](oracle_audit.csv) with one row per test. Only the programs were run; nothing of the source code of Icarus Verilog or Verilator was read.

```sh
python3 tools/oracle_audit.py --tests-repo <kirimasim-tests at c983cf799b4d> --out <dir> \
    --iverilog-bin <bin of Icarus Verilog 13.0> --verilator <Verilator 5.052> \
    --kirimasim build/release/src/driver/kirimasim
```

A test is a candidate when one of these holds:

| | Rule |
| --- | --- |
| a | its expected output was recorded with Verilator, and x or z takes part (in the expected output, in the code, or `:fourstate: yes`) |
| b | Icarus and Verilator both run it, and their outputs differ |
| c | Verilator's output differs between the seeds |
| d | its code uses a construct whose result can depend on the order of events (`#0`, nonblocking assignments, `fork`, named events, `$strobe` or `$monitor`, a variable written by more than one procedure) and its expected output was recorded with a simulator |

The detection for a and d works on the text, so it finds too much rather than too little: the CSV gives what it found. Each candidate was then read against the clauses: for a, b and c, every expected line that depends on x, z, strengths, net resolution, formatting or the simulator that recorded it; for d, every place where two processes, or two statements of one, may run in an order the standard leaves open, and whether a printed line depends on it.

Verdicts: **oracle bug** (the expected output follows a deviation of the simulator that recorded it), **unspecified** (a printed line depends on something the standard leaves open), **test error** (the test's own check or expected output contradicts the standard), **no issue**, and **needs a person** (no clause decides it).

## 3. Results

The runs reproduce the oracles: all 1238 tests that list `iverilog@13.0` give their expected output with Icarus Verilog 13.0, and all 226 that list `verilator@5.052` give it with Verilator 5.052 for all four seeds. kirimasim passes 1451 tests; 15 are unsupported.

| | Tests |
| --- | --- |
| Icarus: matches / does not compile or run / differs | 1238 / 223 / 5 |
| Verilator (seed 1): matches / does not build / differs | 1327 / 32 / 107 |
| Verilator's output differs between seeds | 33 |
| Icarus and Verilator both run it, outputs differ | 105 |
| Candidates a / b / c / d | 3 / 105 / 33 / 92 |
| Candidates in all (a test can be in more than one) | 191 |

| Verdict | Tests |
| --- | --- |
| Oracle bug | 0 |
| Unspecified | 2 |
| Test error | 0 |
| Needs a person | 0 |
| No issue | 189 |

No expected output of the 191 records a bug of Icarus 13.0 or Verilator 5.052: where the two differ, the expected output is the one the standard requires, and the other simulator deviates; most of the differences come from Verilator having no x, z or strengths. As a further check, no expected output of the population has a line in which the test reports a failure of its own check.

An earlier run of the tool, before it left out conditions of `for` loops and the implication operator `->`, found 34 more candidates for d; all were read as well and are no issue.

### 3.1 Tests that are not "no issue"

| Test | Cand. | Verdict | Clause | Reason | What the standard allows | kirimasim | Upstream issue |
| --- | --- | --- | --- | --- | --- | --- | --- |
| `ch4/test_event_delta_cycles` | bcd | unspecified | 4.7, 9.4.2 | always @(a or b or c) counts how often it wakes. The initial changes two or three of a, b and c in a row with no time control between them, and 4.7 lets a simulator suspend a procedure between such statements; each change can then wake the block once. The expected counts are those of a simulator that does not suspend there. See 4.1. | Lines 6, 7, 15 and 17 (the counter): 3 to 6, then that value plus 3, then 8 to 12 (with delayed_counter equal), then that value plus 5. Every other line is fixed. | pass | — |
| `ch9/test_sens_level` | d | unspecified | 4.7, 9.4.2 | Line 108 writes a = 0; b = 1; with no time control between them, both read by always @(a or b or c). Under 4.7 the initial may be suspended after a = 0, and the always block may run and be suspended too before it waits again; b = 1 then wakes nothing and out3 stays 00. This needs two suspensions, not only a choice of which process runs first. See 4.1. | Scenario 6 may print PASS (out3=0xbb) or FAIL (out3=0x00), with the counts and the last line to match. Every other line is fixed. | pass | — |

No upstream issue was looked for: neither is a bug of a simulator. Both stay in the population (4.1).

## 4. Decisions

The owner accepted the recommendations below on 2026-10-21. Each follows an interpretation that kirimasim's specifications had already approved.

### 4.1 Suspending a procedure between two statements (4.7)

4.7 says that statements with no time control in a procedure need not run as one event, and that a simulator may suspend a procedure at any point and run another process meanwhile. `ch4/test_event_delta_cycles` prints how often an always block woke, which depends on that directly; `ch9/test_sens_level` depends on it when the woken block is suspended too before it waits again, and so misses the second change. Both are **unspecified**. kirimasim's UNSPECIFIED_BEHAVIOR.md B2 already lists this point as one the standard leaves open, and records kirimasim's choice: a process is not switched until it waits.

Both **stay in the population**. Their expected outputs are the results of that choice, and TEST_SUITE.md judges kirimasim's run of a test whose result the standard leaves open against the result kirimasim chose (`choice`). Exclusion is for expected outputs that record a bug of a simulator, and there are none. The same holds for any other test of the population whose output depends on B2: a count finds 38 tests with a level-sensitive always block and two blocking assignments in a row somewhere, not checked one by one, and their result in the gate does not depend on that check. When the list moves to a newer commit, such tests could be marked `:nondeterministic: yes` in kirimasim-tests with the results the standard allows; that is a change of kirimasim-tests, outside this audit.

### 4.2 The case of the exponent letter of %E and %G (21.2.1.1)

Table 21-2 lists `%e or %E` and `%g or %G` as one specification each, and says that the real formats have the formatting of C, where `%E` and `%G` write `E`. kirimasim's SYSTEM_TASKS.md 3.6 reads 21.2.1.1 that way (approved 2026-10-17). `ch21/test_format_advanced` and `ch21/test_format_float` are therefore **no issue**: their expected `E` is what that reading gives, as Icarus 13.0 and kirimasim do and Verilator 5.052 does not. The hexadecimal digits a to f are lower case by a rule of SYSTEM_TASKS.md 3.2 with no sentence of the standard behind it; all three simulators print them in lower case, so no test was marked for it.

### 4.3 NaN

Five tests (`test_math_advanced`, `test_math_hyperbolic_special_values`, `test_math_special_values`, `test_realtobits_bitstoreal`, `test_system_functions_edge`) pass only when a NaN compares unequal to itself. The standard does not mention NaN. Those lines are taken as fixed, from 6.12 (real is a C double, represented as IEEE 754 by its footnote) and 20.8.2 (the math functions behave as those of C), the interpretation of kirimasim's VALUE_SEMANTICS.md 7. They are **no issue**.

### 4.4 Exclusions

None. No expected output records a bug of Icarus Verilog or Verilator, and the two unspecified tests stay (4.1), so no list of exclusions was made, no issue was opened against kirimasim, and the README counts do not change. Should an exclusion be needed later: the gate runs kirimasim-tests 179c55de9c83, the commit named in the header of the population list (list chosen at c983cf799b4d), so a correction in kirimasim-tests main does not reach the gate until the list moves, which TEST_SUITE.md makes a change approved by a person; a list of exclusions in this repository (tests/gates/) would take effect at once.

## 5. Comment errors that do not change the expected output

| Test | Line | What is wrong |
| --- | --- | --- |
| `ch5/test_ieee1800_2023_literal_edge` | 182 | It says the characters after `\0` may or may not be kept; 6.16 removes the `\0` and keeps the rest (the expected length 10 is right). |
| `ch20/test_math_edge_cases` | 51 | It calls `$clog2(0)` implementation-defined; 20.8.1 makes it 0 (the check and the expected output are right). |
| `ch6/test_x_value_bitwise` | 54 | It first writes `x010` for `4'bxx10 & 4'b1001`; its own working and the check give `x000`. Its header cites 6.6.7; the tables are in 11.4.8. |
| several tests of chapter 11 | — | Some cite 11.4.2 for arithmetic or division by zero (11.4.3), 11.4.6 for `===` (11.4.5), or 11.4.12 for reduction (11.4.9); `ch12/test_casex_casez_wildcard` prints 12.5.2 for a rule of 12.5.1. |

These are in kirimasim-tests and are not part of this proposal.

## 6. Deviations of Icarus Verilog and Verilator seen in the runs

These were seen in the outputs of this audit and have not been confirmed with a minimal design. None of them is in an expected output. They are candidates for docs/simulator-differences.md of kirimasim-tests (where differences are recorded, not reported).

| Release | Clause | Seen | Tests |
| --- | --- | --- | --- |
| Icarus 13.0 | 20.9 | `$countbits` stops the run on an internal assertion. | `ch20/test_countones_bits` |
| Icarus 13.0 | 6.16.12, 6.16.9 | `hextoa` of a negative int writes a minus sign. | `ch6/test_string_hextoa` |
| Icarus 13.0 | 21.2.1.2 | A field width pads hexadecimal with spaces instead of zeros. | `ch21/test_display_format` |
| Icarus 13.0, Verilator 5.052 | 16.3 | The pass statement of an immediate cover does not run. | `ch16/test_assert_immed_2` |
| Verilator 5.052 | 6.6.3 | wand, wor, triand and trior nets do not give the AND or OR of their drivers. | 7 tests, such as `ch6/test_net_wand` |
| Verilator 5.052 | 20.5 | `$shortrealtobits` and `$bitstoshortreal` give 0 or other bits. | 9 tests, such as `ch20/test_shortreal_ieee754` |
| Verilator 5.052 | 20.8.2, 6.12 | A NaN compares equal to itself. | the 5 tests of 4.3 |
| Verilator 5.052 | 4.4.2.3, 4.5 | After `#0`, continuous assignments and always_comb blocks woken before it show their old values (near the listed item of 4.9.1). | `ch4/test_delta_cycle`, `ch4/test_ieee1800_2023_delta_convergence`, `ch9/test_always_comb_cascade_propagation`, `ch9/test_sensitivity_edge_cases` |
| Verilator 5.052 | 21.2.1.1 | `%E` and `%G` write a lower-case `e` (against the reading of SYSTEM_TASKS.md 3.6). | `ch21/test_format_advanced`, `ch21/test_format_float` |
| Verilator 5.052 | 21.3.4.3 | `$sscanf` ignores a field width, and `%d` stops at an underscore. | `ch21/test_scanf_comprehensive`, `ch21/test_numeric_edge_cases` |
| Verilator 5.052 | 6.16.9 | `atoi` reads a sign and leading spaces. | `ch6/test_string_atoi`, `ch6/test_string_atoi_2` |
| Verilator 5.052 | 5.7.1 | An unsized decimal number over 32 bits loses its sign bit. | `ch6/test_int_types_edge_cases`, `ch11/test_ieee1800_2023_arithmetic_edge` |
| Verilator 5.052 | 11.6.1, 11.6.2 | `INT_MIN / -1` gives 0. | `ch11/test_ieee1800_2023_arithmetic_edge` |
| Verilator 5.052 | 9.4.2.2 | An `always @*` that writes a variable it reads stops with DIDNOTCONVERGE. | `ch9/test_implicit_event` |
| Verilator 5.052 | 29 | An edge-sensitive UDP gives another output. | `ch29/test_udp_seq` |
| Verilator 5.052 | — | Lines on known values fail too, some differently per seed (cause not known). | `ch11/test_case_equality_edge_cases`, `ch11/test_partselect_xz`, `ch11/test_slice_assign`, `ch12/test_control_flow`, `ch20/test_onehot_isunknown` and others |

`$printtimescale` without parentheses (Verilator) and `assign` evaluated once (Icarus) were seen too and are listed already.

## 7. All candidates

| Test | Cand. | Verdict | Clause | Reason | kirimasim |
| --- | --- | --- | --- | --- | --- |
| `ch10/test_assign_edge_cases` | d | no issue | 4.9.1, 4.4.2.2, 4.4.2.3, 4.5, 4.6 | Only one procedure writes a, b, sel. The continuous assignments at 26-27 are read either after #1 or, in Test 8, after `#0` (117). | pass |
| `ch10/test_assign_nba` | d | no issue | 4.6, 4.4.2.3, 10.4.2, 9.4.1 | Only one procedure reads or writes `a` (the `always #5 clk` toggles a clock nobody waits on). | pass |
| `ch10/test_assign_proc` | b | no issue | 10.6, 10.6.1, 4.9.2 | A procedural continuous assignment is evaluated again when an operand of its right-hand side changes, so wide_out follows data_in in Test 4b (0xDEBC). Icarus 13.0 keeps 0x55BC. | pass |
| `ch10/test_assignment_forms` | d | no issue | 10.3.2, 10.4.2, 4.4.2.4, 6.8 | Only one procedure writes `a`/`b`. `a = 50;` (l.45) runs at t=2, and the edges that `always_ff` (l.29-31) samples come later, at t=5 and t=15, so no write lands on an edge. | pass |
| `ch10/test_blocking_in_clocked_always` | d | no issue | 4.4.2.4, 4.5, 10.4.2, 9.4.2 | Both `always @(posedge clk)` blocks (l.44, l.70) wake on the same edge in no defined order, but they share only NBA targets: `resetn <= 1` (l.73) is read by l.48 and `reg_a`/`reg_b` (l.49-54) are read by l.78/l.90-103. | pass |
| `ch10/test_concat_lhs_assign` | d | no issue | 6.8, 9.4.2, 10.4.2 | The only NBA is `{nba_high, nba_low} <= 16'hDEAD` (97). The same process reads it one clock period later, after the next `@(posedge clk)` (98-99), so the NBA region of the earlier time step has run. clk starts at 0 from its declaration initializer (6.8), so time 0 has no posedge. | pass |
| `ch10/test_continuous_assign_2` | d | no issue | 4.9.1, 10.3.3, 4.4.2.3, 4.6 | Every check but Test 10 reads the wires `#1` after their operands changed. In Test 10 `assign #1 delayed_out = a` cannot take the new value before t=9 (10.3.3), so the check gives the same result in any order. | pass |
| `ch10/test_ct_assignment_complex` | d | no issue | 10.4.2, 4.4.2.4, 10.6.2, 6.21 | Apart from the clock (l.18), there is only one process. Each NBA (l.135, 145, 155-158, 169-171, 184) is made at one posedge and read at the next posedge, a separate time step 10 units later, after the NBA region has run. | pass |
| `ch10/test_edge_delta_cycle_chain` | d | no issue | 4.9.1, 10.3.2, 10.3.3, 4.4.2.3, 4.5 | `assign mid = source; assign #0 dest = mid;` are evaluated at time zero (4.9.1) and again on every change of `source`, and the #0 update finishes inside the same time slot. | pass |
| `ch10/test_force_deep_generate_scope` | d | no issue | 10.6.2, 4.6 | The several writers are continuous assignments and the force and release of the one initial; a force overrides the continuous assignment (10.6.2), and every check comes at least `#1` after the force or release it tests. | pass |
| `ch10/test_nba` | d | no issue | 4.6, 10.4.2 | The test has one procedure. The NBA swap and the multiple NBAs are checked `#1` later, and the NBAs are performed in execution order (4.6 b). | pass |
| `ch10/test_nba_assignment` | d | no issue | 10.4.2, 4.4.2.4 | There is one test process plus the clock. Every NBA is followed by `@(posedge clk)` (the next edge, 10 units later) before it is read, and the pipeline reads at l.68-69/82-83 deliberately take the old values (10.4.2). | pass |
| `ch10/test_nba_assignments` | d | no issue | 4.6, 10.4.2, 9.4.2 | One initial block does all the assigning and reading. The clock process only toggles `clk`, which is declaration-initialized (l.22), so there is no time-0 edge. | pass |
| `ch10/test_nba_basic` | d | no issue | 4.6, 10.4.2 | A single initial procedure. Each NBA is read after the next `@(posedge clk)`, 10 time units later. | pass |
| `ch10/test_nba_basic_2` | d | no issue | 6.8, 9.4.2, 10.4.2, 4.6 | Besides the clock, only the initial touches a and b. A read in the same time step as its NBA sees the old value (10.4.2), and every later check comes one clock period after its NBA. | pass |
| `ch10/test_nba_complex_lhs` | d | no issue | 10.4.2, 4.4.2.4 | There is one test process plus the clock. Each group of NBAs to bit selects, part selects or members is read after the next `@(posedge clk)`, a later time step. | pass |
| `ch10/test_nba_complex_lhs_extended` | d | no issue | 4.6, 10.4.2 | One process writes all the NBA targets and reads each one only after the next `@(posedge clk)`. | pass |
| `ch10/test_nba_edge_cases` | d | no issue | 4.7, 10.4.2, 9.4.2 | There are real races, but none reaches a printed line. In the loop at 68-69, and at 116-117 (`@(posedge clk); a = 8'h55;`), the initial writes `a` with a blocking assignment at the same posedge where the always at 28-29 samples it. That always may take the old or the new value (4.7). In every case another posedge follows before the check (69 and 118 respectively). | pass |
| `ch10/test_nba_eval` | d | no issue | 4.6, 10.4.2 | The test has one procedure and no other processes. Each NBA group is followed by `#1` before the check, and NBAs from one block are performed in execution order (4.6 b). | pass |
| `ch10/test_nba_nested_array` | d | no issue | 10.4.2, 4.4.2.4 | There is a single process with no clock. Each block of NBAs is followed by `#1` before the reads, so all updates have been applied in the NBA region of the earlier time slot. | pass |
| `ch10/test_nba_ordering` | d | no issue | 4.6, 10.4.2, 4.4.2.4 | This is one sequential process. `a <= 8'hAA` followed by an immediate read (l.38-40) must see the old 0 (10.4.2). | pass |
| `ch10/test_nba_part_select` | d | no issue | 10.4.2, 4.6 | A single process. The NBAs to non-overlapping part-selects are read after #1, when the NBA region has run. | pass |
| `ch10/test_nba_select` | d | no issue | 4.6, 10.4.2 | The test has one procedure. The bit and part-select NBAs touch disjoint bits and are checked after `#1`. | pass |
| `ch10/test_nba_verification` | d | no issue | 4.7, 10.4.2, 10.3.2 | There is a real race at l.36: `i.d = 1;` runs at the posedge at t=3, the same edge on which `always_ff @(posedge i.clk) i.q <= i.d;` (l.18) samples `i.d`. Depending on the order, `q` gets 0 or 1 at t=3. That edge does not affect anything printed. The checks run two posedges later (t=7), by which time the t=5 edge has sampled `d=1` with no race (d was written at t=3), so `i.q` and `local_q` (assign, l.29) are both 1 under either order. | pass |
| `ch10/test_nonblocking_assign` | d | no issue | 4.6, 10.4.2 | This is one sequential process. Each NBA is read either in the same step, which gives the old value as at l.111-113, or after a later `@(posedge clk)`. | pass |
| `ch10/test_nonblocking_assignment` | d | no issue | 10.4.2, 4.6 | A single process. The NBA right-hand sides read the pre-NBA values in the same process, and the checks run one posedge later. | pass |
| `ch10/test_stmt_nonblocking` | d | no issue | 6.8, 9.4.2, 10.4.2, 4.6 | The scan saw a-d written by two procedures. The initial (L23) writes them at t=0, while the `always @(posedge clk)` first runs at t=5, so the writes are separated by a nonzero delay. | pass |
| `ch11/test_bitwise_corner_cases` | bc | no issue | 11.4.8 (Tables 11-11 to 11-14) | 1 & x and 0 \| x are x, 0 & x is 0, and z is taken as x. Verilator, a two-state simulator, has no x or z and prints known values there. | pass |
| `ch11/test_case_equality` | bc | no issue | 11.4.5 | === and !== compare x and z bits as values and always give 0 or 1. Verilator, a two-state simulator, has no x or z and prints known values there. | pass |
| `ch11/test_case_equality_edge_cases` | b | no issue | 11.4.5 | x === x, z === z and patterns with the same x and z bits give 1, x !== z gives 1. Verilator, a two-state simulator, has no x or z and prints known values there. It also fails some lines with known values only (not confirmed why). | pass |
| `ch11/test_comparison_corner_cases` | b | no issue | 11.4.4, 11.4.5 | === matches x and z exactly, and == or < with an x or z bit gives x. Verilator, a two-state simulator, has no x or z and prints known values there. | pass |
| `ch11/test_comparison_ops` | bc | no issue | 11.4.4, 11.4.5 | The two lines that Verilator misses compare x values. Verilator, a two-state simulator, has no x or z and prints known values there. | pass |
| `ch11/test_div_zero` | b | no issue | 11.4.3, 21.2.1.3 | Division and modulus by zero give x in every bit. Verilator, a two-state simulator, has no x or z and prints known values there. | pass |
| `ch11/test_expression_edge_cases` | b | no issue | 11.4.3, 11.4.5 | Division and modulus by zero give x in every bit. Verilator, a two-state simulator, has no x or z and prints known values there. | pass |
| `ch11/test_generate_bitselect_literal` | d | no issue | 6.20, 4.7 | The only output comes from the initial in `ram_model`, which checks localparams that are elaboration-time constants and then calls `$finish` at t=0. | pass |
| `ch11/test_ieee1800_2023_arithmetic_edge` | b | no issue | 11.4.3, 11.6.1 (Table 11-21), 11.6.2, 5.7.1 | Division by zero gives x. INT_MIN / -1 is computed in 32 bits, so the carry is lost and the result is INT_MIN. 2147483648 needs 33 bits with its sign bit, so it is not negative. Verilator 5.052 is two-state and also gives 0 for INT_MIN / -1. | pass |
| `ch11/test_implication_edge_cases` | d | no issue | 11.4.7, 6.8 | The test has a single initial block and no events. The text scan matched the `->` operator, which here means implication, not an event trigger. | pass |
| `ch11/test_mixed_xz` | bc | no issue | 11.4.8, 11.4.3, 11.4.5, 11.4.9, 21.2.1.3 | x and z mixed with known bits give x per bit through bitwise, arithmetic and reduction operators. Verilator, a two-state simulator, has no x or z and prints known values there. | pass |
| `ch11/test_operator_edge_cases_2` | b | no issue | 11.4.5 | === of the same x pattern gives 1. Verilator, a two-state simulator, has no x or z and prints known values there. | pass |
| `ch11/test_operator_precedence` | b | no issue | 11.4.5 | === of the same x pattern gives 1. Verilator, a two-state simulator, has no x or z and prints known values there. | pass |
| `ch11/test_partselect_xz` | bcd | no issue | 11.5.1, 21.2.1.3 | The bits of a part-select that fall outside the vector read as x. Verilator, a two-state simulator, has no x or z and prints known values there. Its results also vary by seed on lines with known values. Event order: no printed line depends on it. | pass |
| `ch11/test_reduction_corner_cases` | bc | no issue | 11.4.9 (Tables 11-16 to 11-18) | The reduction of bits that are all x or z gives x; a known 1 decides \| and a known 0 decides &. Verilator, a two-state simulator, has no x or z and prints known values there. | pass |
| `ch11/test_reduction_xz_propagation` | bc | no issue | 11.4.9 | A reduction with an x bit and no deciding bit gives x. Verilator, a two-state simulator, has no x or z and prints known values there. | pass |
| `ch11/test_slice_assign` | bc | no issue | 11.5.1 | The part-select writes and reads are on known values, and the one x/z write is checked only through untouched bits. Verilator 5.052 loses the writes to wide_reg (not confirmed why). | pass |
| `ch11/test_streaming_nba_complex` | d | no issue | 9.4.2, 10.4.2, 11.4.14.3 | `clk` is written by `initial clk = 0` at t=0 and by the always from t=5 on; x->0 is not a posedge, and the streaming NBAs are checked at the next posedge, 10 units later. | pass |
| `ch11/test_ternary` | b | no issue | 11.4.11 (Table 11-20), 21.2.1.3 | With an x condition the result combines both operands bit by bit: equal bits stay, different bits become x, which prints XX for 0xFF against 0xAA. Verilator, a two-state simulator, has no x or z and prints known values there. | pass |
| `ch11/test_value_ops_edge_cases` | bc | no issue | 11.4.5, 11.4.8, 11.8.2 | The bitwise results are x per bit where x meets 1, as the tables give. Verilator, a two-state simulator, has no x or z and prints known values there. | pass |
| `ch11/test_x_propagation` | bc | no issue | 11.4.3, 11.4.8, 11.4.9, 11.4.11 | Every printed x value follows the operator tables. Verilator, a two-state simulator, has no x or z and prints known values there. | pass |
| `ch11/test_x_z_propagation` | bc | no issue | 11.4.3, 11.4.5, 11.4.8, 11.4.11, 12.5.1 | x == x is x, === is known, z is taken as x by the operators, and in casez an x is not a do-not-care. Verilator, a two-state simulator, has no x or z and prints known values there. | pass |
| `ch11/test_x_z_propagation_detailed` | bc | no issue | 11.4.3, 11.4.5, 11.4.8, 12.4 | All twelve checks hold with four-state values. Verilator, a two-state simulator, has no x or z and prints known values there. | pass |
| `ch11/test_z_propagation` | bc | no issue | 11.4.8, 11.4.10, 11.4.11, 11.5.1 | The operators take z as x, and a shift moves z bits as they are. Verilator, a two-state simulator, has no x or z and prints known values there. | pass |
| `ch12/test_case_statement` | c | no issue | 12.5, 12.5.1 | In casex, x and z bits of the expression and of the items are do-not-care. Verilator, a two-state simulator, has no x or z and prints known values there. | pass |
| `ch12/test_casex_casez_wildcard` | bc | no issue | 12.5.1 | A z in a casez expression is a do-not-care, so 10z1 matches the first item, 1001. Verilator, a two-state simulator, has no x or z and prints known values there. | pass |
| `ch12/test_control_flow` | b | no issue | 12.4, 12.5, 6.11.2 | An int keeps no x or z, so 4'b10x1 stored in it is 9. Verilator, a two-state simulator, has no x or z and prints known values there. It also fails lines with known values (not confirmed why). | pass |
| `ch12/test_statement_case` | bc | no issue | 12.5, 12.5.1 | 4'b1?00 is 1z00, and the case items match as the rules of casez give. Verilator, a two-state simulator, has no x or z and prints known values there. | pass |
| `ch12/test_stmt_casex` | c | no issue | 12.5, 12.5.1 | x and z in casex are do-not-care; 11x0 does not match 0000 because bit 3 differs. Verilator, a two-state simulator, has no x or z and prints known values there. | pass |
| `ch12/test_stmt_casez` | bc | no issue | 12.5.1 | The z bit of the casez expression and the x bit of the casex expression are do-not-care. Verilator, a two-state simulator, has no x or z and prints known values there. | pass |
| `ch15/test_event_named` | d | no issue | 15.5.2, 9.4.2, 4.6 | The five waiter initials reach their `@(evt...)` at t=0. The triggers (`-> evt1` L73 and the others) happen at t>=1, so every waiter is already waiting when its event fires (15.5.2). | pass |
| `ch15/test_event_ordering` | d | no issue | 15.5.1, 9.4.2, 4.4.2.2, 4.4.2.3, 4.5 | `clk = 1;` (l.59) wakes P1 as an Active event. P1's `-> ev1` unblocks P2, which is already waiting at `@(ev1)` (15.5.1), and so on through `ev2` and `ev3`; every step is an Active event at t=5. | pass |
| `ch15/test_event_triggered` | d | no issue | 15.5.3 | One process triggers and tests its own events. After `-> e1;` (l.32), `e1.triggered` is true for the rest of the time step (15.5.3). e2 is false before l.51 and true after it. e3 is never triggered. | pass |
| `ch16/test_comb_stable_multiinstance` | d | no issue | 16.9.3, 16.5.1, 10.4.2, 9.4.2 | The testbench changes `sig0` and `sig1` only at t=edge+1 (after `@(posedge clk); #1;`), never at a clock edge. | unsupported |
| `ch20/test_bit_query_functions_2` | b | no issue | 20.9 | $isunknown is 1 when a bit is x or z. Verilator, a two-state simulator, has no x or z and prints known values there. | pass |
| `ch20/test_bitop_edge_cases` | b | no issue | 20.9 | $onehot and $onehot0 count only bits that are 1; x bits are not 1. Verilator, a two-state simulator, has no x or z and prints known values there. | pass |
| `ch20/test_bitop_functions` | b | no issue | 20.9 | $isunknown is 1 when a bit is x or z. Verilator, a two-state simulator, has no x or z and prints known values there. | pass |
| `ch20/test_bitop_functions_2` | bc | no issue | 20.9 | Each function reads the value just assigned; $isunknown is 1 for x or z. Verilator, a two-state simulator, has no x or z and prints known values there. Its results also vary by seed on lines with known values. | pass |
| `ch20/test_bitop_system_tasks` | b | no issue | 20.9 | $isunknown is 1 when a bit is x or z. Verilator, a two-state simulator, has no x or z and prints known values there. | pass |
| `ch20/test_comb_sysfunc_edge` | b | no issue | 20.9 | $isunknown is 1 for x and for z. Verilator, a two-state simulator, has no x or z and prints known values there. | pass |
| `ch20/test_comb_system_functions` | b | no issue | 20.9 | $isunknown is 1 for an x bit. Verilator, a two-state simulator, has no x or z and prints known values there. | pass |
| `ch20/test_countones_2` | b | no issue | 20.9 | $isunknown is 1 for x or z bits. Verilator, a two-state simulator, has no x or z and prints known values there. | pass |
| `ch20/test_countones_bits` | b | no issue | 20.9 | $countbits counts the bits equal to one of the control bits: 4, 4 and 8. Icarus 13.0 stops on an internal assertion at the first $countbits call (exit code 134). | pass |
| `ch20/test_isunknown` | b | no issue | 20.9 | $isunknown is 1 for x or z bits. Verilator, a two-state simulator, has no x or z and prints known values there. | pass |
| `ch20/test_isunknown_2` | b | no issue | 20.9 | One x or z bit makes $isunknown 1; a known value gives 0. Verilator, a two-state simulator, has no x or z and prints known values there. | pass |
| `ch20/test_isunknown_3` | b | no issue | 20.9 | Both values have x bits, so $isunknown is 1. Verilator, a two-state simulator, has no x or z and prints known values there. | pass |
| `ch20/test_isunknown_basic` | b | no issue | 20.9 | $isunknown is 1 for all-x and partly-x values. Verilator, a two-state simulator, has no x or z and prints known values there. | pass |
| `ch20/test_isunknown_func` | b | no issue | 20.9 | $isunknown is 1 for x or z bits and 0 for known values. Verilator, a two-state simulator, has no x or z and prints known values there. | pass |
| `ch20/test_math_advanced` | b | no issue | 20.8.2, 6.12, 11.4.5 | A math function whose argument is outside its domain gives NaN as the C function does (20.8.2), and a NaN compares unequal to itself, as a C double does (6.12 and its footnote). Verilator 5.052 prints nan but takes v != v as false. See 4.3. | pass |
| `ch20/test_math_edge_cases` | b | no issue | 20.8.1, 20.9 | $isunknown is 1 for x and z, and $clog2(0) is 0. Verilator, a two-state simulator, has no x or z and prints known values there. The comment at line 51 is wrong (see 5). | pass |
| `ch20/test_math_hyperbolic_special_values` | b | no issue | 20.8.2, 6.12, 11.4.5 | A math function whose argument is outside its domain gives NaN as the C function does (20.8.2), and a NaN compares unequal to itself, as a C double does (6.12 and its footnote). Verilator 5.052 prints nan but takes v != v as false. See 4.3. | pass |
| `ch20/test_math_special_values` | b | no issue | 20.5, 20.8.2, 6.12, 11.4.5 | The values are the IEEE 754 single-precision patterns that the conversion has to give (20.5). Verilator 5.052 returns 0 or other bits from $shortrealtobits and $bitstoshortreal. A math function whose argument is outside its domain gives NaN as the C function does (20.8.2), and a NaN compares unequal to itself, as a C double does (6.12 and its footnote). Verilator 5.052 prints nan but takes v != v as false. See 4.3. | pass |
| `ch20/test_onehot_isunknown` | b | no issue | 20.9 | $onehot and $onehot0 count the 1 bits, and $isunknown is 1 for xxxx0000 and 0000zzzz. Verilator, a two-state simulator, has no x or z and prints known values there. It also fails $onehot of known values (not confirmed why). | pass |
| `ch20/test_printtimescale` | b | no issue | 20.4.2 | The standard gives the form of the line, with the name of the design element in parentheses. Verilator 5.052 leaves out the parentheses (already in the list of differences). | pass |
| `ch20/test_real_bits_simple` | b | no issue | 20.5 | The values are the IEEE 754 single-precision patterns that the conversion has to give (20.5). Verilator 5.052 returns 0 or other bits from $shortrealtobits and $bitstoshortreal. | pass |
| `ch20/test_real_literal_type` | b | no issue | 20.5, 5.7.2 | The values are the IEEE 754 single-precision patterns that the conversion has to give (20.5). Verilator 5.052 returns 0 or other bits from $shortrealtobits and $bitstoshortreal. | pass |
| `ch20/test_realtobits_bitstoreal` | b | no issue | 20.5, 6.12, 11.4.5 | The values are the IEEE 754 single-precision patterns that the conversion has to give (20.5). Verilator 5.052 returns 0 or other bits from $shortrealtobits and $bitstoshortreal. | pass |
| `ch20/test_realtobits_corner_cases` | b | no issue | 20.5, 21.2.1.2 | The values are the IEEE 754 single-precision patterns that the conversion has to give (20.5). Verilator 5.052 returns 0 or other bits from $shortrealtobits and $bitstoshortreal. | pass |
| `ch20/test_shortreal_ieee754` | b | no issue | 20.5 | The values are the IEEE 754 single-precision patterns that the conversion has to give (20.5). Verilator 5.052 returns 0 or other bits from $shortrealtobits and $bitstoshortreal. | pass |
| `ch20/test_system_bitstoshortreal` | b | no issue | 20.5 | The values are the IEEE 754 single-precision patterns that the conversion has to give (20.5). Verilator 5.052 returns 0 or other bits from $shortrealtobits and $bitstoshortreal. | pass |
| `ch20/test_system_funcs_bits` | b | no issue | 20.9, 5.7.1 | An X digit in a literal is an x bit, so $isunknown is 1. Verilator, a two-state simulator, has no x or z and prints known values there. | pass |
| `ch20/test_system_function_calls` | b | no issue | 20.9 | $isunknown is 1 for an x bit. Verilator, a two-state simulator, has no x or z and prints known values there. | pass |
| `ch20/test_system_functions_edge` | b | no issue | 20.9, 20.8.2, 6.12, 11.4.5 | $isunknown is 1 for x and z. Verilator, a two-state simulator, has no x or z and prints known values there. A math function whose argument is outside its domain gives NaN as the C function does (20.8.2), and a NaN compares unequal to itself, as a C double does (6.12 and its footnote). Verilator 5.052 prints nan but takes v != v as false. See 4.3. | pass |
| `ch20/test_system_isunknown` | b | no issue | 20.9 | $isunknown is 1 for x or z bits. Verilator, a two-state simulator, has no x or z and prints known values there. | pass |
| `ch20/test_system_shortrealtobits` | b | no issue | 20.5 | The values are the IEEE 754 single-precision patterns that the conversion has to give (20.5). Verilator 5.052 returns 0 or other bits from $shortrealtobits and $bitstoshortreal. | pass |
| `ch20/test_type_conversion` | b | no issue | 20.5 | The values are the IEEE 754 single-precision patterns that the conversion has to give (20.5). Verilator 5.052 returns 0 or other bits from $shortrealtobits and $bitstoshortreal. | pass |
| `ch21/test_display_format` | b | no issue | 21.2.1.2 | A field width wider than the value pads hexadecimal with zeros on the left, so %4h of 8'hAB prints 00ab. Icarus 13.0 pads with spaces. :oracle: is spec alone. | pass |
| `ch21/test_format_advanced` | b | no issue | 21.2.1.1 (Table 21-2) | Lines 8 and 16 print a real with %E and %G. The real formats have the formatting of C, where %E and %G write an uppercase E; kirimasim's SYSTEM_TASKS.md 3.6 reads 21.2.1.1 that way. Icarus 13.0 and kirimasim print E, Verilator 5.052 e. See 4.2. | pass |
| `ch21/test_format_binary` | bc | no issue | 21.2.1.3 | %b prints each bit as 0, 1, x or z. Verilator, a two-state simulator, has no x or z and prints known values there. Its results also vary by seed on lines with known values. | pass |
| `ch21/test_format_float` | b | no issue | 21.2.1.1 (Table 21-2) | The checks at lines 64-65 and 82-83 compare the result of %E with an uppercase E, as in test_format_advanced; Verilator 5.052 prints e. See 4.2. | pass |
| `ch21/test_format_octal` | bc | no issue | 21.2.1.3 | An octal digit whose three bits are all x prints x, all z prints z, and a mix prints X or Z. Verilator, a two-state simulator, has no x or z and prints known values there. | pass |
| `ch21/test_monitor_strobe` | d | no issue | 21.2.2, 21.2.3, 4.4.2.9, 20.3.1, 20.4.3 | The writes to `counter` never meet the clock edge; `$display` runs in Active and `$monitor` and `$strobe` in Postponed (4.4.2.9), and the monitor is off before `$strobe` runs, so no two Postponed lines meet. | pass |
| `ch21/test_numeric_edge_cases` | b | no issue | 21.3.4.3 (Table 21-7) | A %d field of $sscanf takes digits and underscores, so 1_000_000 reads as 1000000. Verilator 5.052 stops at the underscore. | pass |
| `ch21/test_scanf_comprehensive` | b | no issue | 21.3.4.3 | A field width limits how many characters a conversion reads, so %4d%4d reads 1234 and 5678. Verilator 5.052 ignores the width. | pass |
| `ch21/test_scanf_functions` | b | no issue | 21.3.4.3 (Table 21-7), 21.2.1.3 | An h field takes x and X as digits too, so the x of 0xCAFE is read as an x digit and the value prints 000xcafe. Verilator, a two-state simulator, has no x or z and prints known values there. | pass |
| `ch23/test_config_block` | d | no issue | 9.4.2, 4.7 | `dut1.clk = 1'b1; dut2.clk = 1'b0;` (l.42-43) may or may not wake the always_ff blocks at time 0 (4.7), but the `data` variables they write are never printed; the printed clocks are read after `#1`. | pass |
| `ch23/test_config_eval` | d | no issue | 9.4.2, 10.4.2, 4.9.6 | The x->0 of clk at time 0 (33) is a negedge, so neither the always_ff nor the waiting initial sees a posedge then, whichever starts first. | pass |
| `ch23/test_inst_array_edge_cases` | bd | no issue | 6.8 (Table 6-7) | q is a logic vector with no initial value, so it is x until written. Verilator, a two-state simulator, has no x or z and prints known values there. Event order: no printed line depends on it. | pass |
| `ch23/test_instantiate_edge_cases` | d | no issue | 4.7, 10.4.2, 10.3.2 | There is a real race at l.132: `data_in = 8'hAA;` runs at the posedge at t=25, the same edge on which `always_ff` (l.21-23) samples `data_in`, so `data_out` becomes 55 or AA at t=25. The check at l.135 is at t=40, after the t=35 edge has sampled AA with no race, so "Chain = aa" is fixed. Every other input write happens at t=2, at a negedge or at a non-edge time, and each read waits for the following negedge or for `#1`. | pass |
| `ch23/test_module_parameter_corner` | d | no issue | 4.7, 10.4.2, 23.3.3.2, 4.5 | Each task drives `dataN_in` with a blocking assignment at the posedge that woke it, so the first sample may take the old or the new value (4.7). The check waits two edges, and the second edge samples the settled input. | pass |
| `ch23/test_module_ports` | d | no issue | 4.7, 9.4.2, 10.4.2, 4.9.6 | There is a time-0 race. `reset = 1` (40) is an x->1 posedge that the always at 94 sees only if it is already waiting (4.7). Either way, the posedge of clk at time 10 (47), with reset=0 since time 5, sets data_out to 0, and the check runs at 15. After that, every input changes at least 5 units before the clk edge, and every check runs 5 units after it. | pass |
| `ch23/test_port_edge_cases` | b | no issue | 6.6.1, 23.3.3.3 | The only driver of the net gives z, so the net is z. Verilator, a two-state simulator, has no x or z and prints known values there. | pass |
| `ch23/test_port_output` | d | no issue | 4.9.1, 4.9.6, 9.4.2, 10.4.2 | Each combinational check reads the outputs `#1` after the input change. | pass |
| `ch23/test_x_propagation_child` | bc | no issue | 23.3.3.3, 6.8 | An unconnected input net is z, and the logic fed from it gives x. Verilator, a two-state simulator, has no x or z and prints known values there. | pass |
| `ch25/test_interface_basic_2` | d | no issue | 9.4.2, 10.4.2, 4.7 | The main initial makes the posedges of `bus.clk` itself and then waits `#5`; the data it samples were written at another time, and its NBA result is read 5 units later. | pass |
| `ch25/test_interface_modport_corner` | d | no issue | 4.5, 4.4.2.4, 10.4.2, 9.4.2, 23.3.3 | Reset is released with `rst_n <= 1` at t=25 (l.164). The master and slave always blocks woken by that edge (through `assign bus_if.clk = clk`) run in the Active region before the NBA region, so they still see rst_n=0 (4.5). | unsupported |
| `ch25/test_interface_parameter_corner` | d | no issue | 4.4.2.2, 4.4.2.4, 4.5, 4.9.1, 10.4.2 | The testbench drives every bus signal with NBAs. On a posedge, the memories (woken through `assign busN.clk = clk`, an Active event) sample the values from before this step's NBAs, whatever the process order. | pass |
| `ch25/test_modport_edge_cases_2` | d | no issue | 9.4.2, 10.4.2, 4.4.2.3, 4.6 | The testbench makes every clock edge itself with a blocking `intf.clk = 1`, followed by `#1` before any further write or read. | pass |
| `ch27/test_generate` | d | no issue | 4.4.2.2, 4.9.1, 9.2.1 | The four generated initials each write a different element of data at time 0. | pass |
| `ch27/test_generate_cond` | d | no issue | 27.5 | The scan saw three `initial use_narrow = ...` blocks, but they are alternatives of one if-generate, and only `medium_config` (L76) is elaborated (DATA_WIDTH=32). | pass |
| `ch27/test_generate_cond_2` | d | no issue | 27.5, 4.7 | Only `gen_mode1` is generated (MODE=1), so `data` has a single writer, `initial data = 16'h1111` at t=0. | pass |
| `ch27/test_generate_corner_2` | d | no issue | 4.7, 10.4.2, 4.9.1 | The blocking writes to stage_in at lines 77, 91 and 147 come right after a posedge at which the stage-0 always also samples it (4.7), but stage_in stays the same through the next posedge before anything is checked. | pass |
| `ch27/test_generate_edge_cases` | d | no issue | 27.4, 27.5, 6.8 | The several writers are branches of an if-generate and a case-generate, of which one each is elaborated; the generated initials write distinct elements, and everything is read after `#1`. | pass |
| `ch27/test_generate_if` | d | no issue | 4.7, 10.4.2, 10.3.2 | There is a real race at l.89-90: `a = 50; b = 100;` run at the posedge at t=25, the edge on which `always_ff` (l.24-26) samples `a`/`b`. Only `a_reg`/`b_reg` depend on that order, and they are never printed afterwards. `sum` at t=25 uses the pre-NBA values of `a_reg`/`b_reg` (10, 20), which gives 30 either way. `sum_pipe` is read at t=25 in Active and holds 30 from the t=15 NBA. | pass |
| `ch29/test_udp_seq` | b | no issue | 29.5, 29.6, 29.7 | The sequential UDPs follow their tables from their initial values. Verilator 5.052 gives another output for the edge-sensitive UDP (not confirmed why). | pass |
| `ch4/test_array_element_noop_nba_keeps_override_order` | d | no issue | 4.6, 10.4.2 | One process issues `arr[0] <= 8'h5a; arr[0] <= arr[1];` (l.40-41). 4.6 b) and 10.4.2 require these NBAs to apply in execution order, so arr[0] = 34. | pass |
| `ch4/test_delta_cycle` | bd | no issue | 4.4.2.3, 4.5, 4.9.1, 10.4.2 | #0 resumes the process only when the Active region is empty (4.4.2.3, 4.5), so the continuous assignments and always_comb blocks woken before it have run. Verilator 5.052 shows the values from before. Event order: no printed line depends on it. | pass |
| `ch4/test_delta_cycles` | d | no issue | 4.4.2.3, 4.6, 20.3.1, 20.4.3 | The test has one procedure and no other process, so the `#0`s order nothing. | pass |
| `ch4/test_delta_scheduler` | d | no issue | 9.2.2.2, 4.4.2.3, 10.4.2 | The only value checks (l.38, l.74) run `#1` after the change to `a`, so the always_comb chain has settled. always_comb also runs once at time 0, so the start order at t=0 does not matter (9.2.2.2). | pass |
| `ch4/test_edge_event_regions` | d | no issue | 4.4.2.3, 4.4.2.4, 4.5, 21.2.2 | This is a single process. After `a <= 1` (l.23), the `#0` (l.32) puts the process in the Inactive region. | pass |
| `ch4/test_event_delta_cycles` | bcd | unspecified | 4.7, 9.4.2 | always @(a or b or c) counts how often it wakes. The initial changes two or three of a, b and c in a row with no time control between them, and 4.7 lets a simulator suspend a procedure between such statements; each change can then wake the block once. The expected counts are those of a simulator that does not suspend there. See 4.1. | pass |
| `ch4/test_ieee1800_2023_delta_convergence` | bcd | no issue | 4.4.2.3, 4.5, 4.9.1 | #0 resumes the process only when the Active region is empty (4.4.2.3, 4.5), so the continuous assignments and always_comb blocks woken before it have run. Verilator 5.052 shows the values from before. Its results also vary by seed. Event order: no printed line depends on it. | pass |
| `ch4/test_nba_after_zero_delay` | bcd | no issue | 6.8 (Table 6-7), 4.4.2.3, 10.4.2 | The variable has no initial value, so it is x, and after #0 the nonblocking update has not happened yet. Verilator, a two-state simulator, has no x or z and prints known values there. Event order: no printed line depends on it. | pass |
| `ch4/test_nba_cross_module` | d | no issue | 4.5, 10.4.2, 23.3.3.2, 9.4.2 | Three `@(posedge clk)` blocks wake at each edge in no defined order, but all the state they share is written by NBA, so the display at l.98 always sees the values from before the edge (10.4.2). | pass |
| `ch4/test_nba_cross_module_timing` | d | no issue | 4.4.2.2, 4.4.2.4, 4.9.6, 10.4.2, 9.4.2 | Both always blocks wake on the same posedge, but every value that crosses between them is written by NBA, so each sees the previous edge's values in either order (10.4.2). | pass |
| `ch4/test_nba_port_propagation` | d | no issue | 6.8, 10.4.2, 4.9.6, 4.9.1 | The scan saw `valid` written by two procedures. The initial writes it at t=0, and the always first writes it (by NBA) at t=5, so the writes are separated in time. | pass |
| `ch5/test_attrib_module` | b | no issue | 6.8 (Table 6-7) | sub.data is a logic variable that is never written, so it is x. Verilator, a two-state simulator, has no x or z and prints known values there. | pass |
| `ch5/test_ieee1800_2023_literal_edge` | b | no issue | 5.7.1, 6.16 | x and z literals keep their bits, and the string with \0 in the middle has length 10 because the \0 is removed. Verilator, a two-state simulator, has no x or z and prints known values there. The comment at line 182 is wrong (see 5). | pass |
| `ch5/test_lexical_numbers` | b | no issue | 5.7.1, 20.9 | 8'b1010_xxxx has x bits, so $isunknown is 1. Verilator, a two-state simulator, has no x or z and prints known values there. | pass |
| `ch6/test_core_types` | b | no issue | 6.11.2, 20.9 | A logic variable keeps 1'bx, so $isunknown is 1. Verilator, a two-state simulator, has no x or z and prints known values there. | pass |
| `ch6/test_edge_wor_wand` | bd | no issue | 6.6.3 (Tables 6-3, 6-4), 4.9.1 | A wand or triand net takes the AND of its drivers and a wor or trior net the OR (Tables 6-3, 6-4). Verilator 5.052 prints the value of one driver. Event order: no printed line depends on it. | pass |
| `ch6/test_generate_string_localparam` | d | no issue | 9.2.1, 4.6 | NAME is a localparam (a constant, not a variable written by procedures). | pass |
| `ch6/test_ieee_compliance_dimensions_twostate` | bc | no issue | 6.11.2, 6.24.1 | An x or z bit assigned or cast to a two-state type becomes 0. Verilator 5.052 has random bits there under --x-initial unique. | pass |
| `ch6/test_int_types_edge_cases` | b | no issue | 5.7.1 | 1000000000000 needs more than 32 bits, so the literal is as wide as its value with a sign bit. Verilator 5.052 makes it negative. | pass |
| `ch6/test_integer_edge_cases` | bc | no issue | 6.8 (Table 6-7), 6.11 | An integer starts as x. Verilator, a two-state simulator, has no x or z and prints known values there. | pass |
| `ch6/test_integer_types` | bc | no issue | 6.11.2, 20.9 | logic and integer keep x, so $isunknown is 1. Verilator, a two-state simulator, has no x or z and prints known values there. | pass |
| `ch6/test_net_logic` | b | no issue | 6.3.1, 6.11.2 | A four-state vector keeps 2'b01 and 2'bxz. Verilator, a two-state simulator, has no x or z and prints known values there. | pass |
| `ch6/test_net_resolution` | b | no issue | 6.6.1 (Table 6-2), 6.6.3 (Tables 6-3, 6-4) | A wand or triand net takes the AND of its drivers and a wor or trior net the OR (Tables 6-3, 6-4). Verilator 5.052 prints the value of one driver. A wire whose driver gives z is z. | pass |
| `ch6/test_net_resolution_multi` | b | no issue | 6.6.1 (Table 6-2), 6.6.3, 28.10 | Two strong drivers of 1 and 0 on a wire give x. Verilator, a two-state simulator, has no x or z and prints known values there. | pass |
| `ch6/test_net_triand` | b | no issue | 6.6.3 (Table 6-3) | A wand or triand net takes the AND of its drivers and a wor or trior net the OR (Tables 6-3, 6-4). Verilator 5.052 prints the value of one driver. | pass |
| `ch6/test_net_trior` | b | no issue | 6.6.3 (Table 6-4) | A wand or triand net takes the AND of its drivers and a wor or trior net the OR (Tables 6-3, 6-4). Verilator 5.052 prints the value of one driver. | pass |
| `ch6/test_net_types_resolution` | b | no issue | 6.6.3 (Tables 6-3, 6-4) | A wand or triand net takes the AND of its drivers and a wor or trior net the OR (Tables 6-3, 6-4). Verilator 5.052 prints the value of one driver. | pass |
| `ch6/test_net_wand` | b | no issue | 6.6.3 (Table 6-3) | A wand or triand net takes the AND of its drivers and a wor or trior net the OR (Tables 6-3, 6-4). Verilator 5.052 prints the value of one driver. | pass |
| `ch6/test_net_wor` | b | no issue | 6.6.3 (Table 6-4) | A wand or triand net takes the AND of its drivers and a wor or trior net the OR (Tables 6-3, 6-4). Verilator 5.052 prints the value of one driver. | pass |
| `ch6/test_parameter_edge_cases` | d | no issue | 4.7, 9.4.2, 10.4.2 | There are real races, but nothing prints their results. Several `din*` writes happen at a posedge (for example L146 `din16 = 16'h0000` at t=25), so the always_ff may sample the old or the new value at that edge. Each printed `dout*` is read at the second posedge after the write, in Active, so it holds the NBA from the first posedge after the write, when the input was stable. L134 `din8 = 8'h00` writes the value `din8` already has, so it causes no change. | pass |
| `ch6/test_reg_edge_cases` | bc | no issue | 6.8 (Table 6-7), 6.11 | A reg starts as x and keeps x and z. Verilator, a two-state simulator, has no x or z and prints known values there. | pass |
| `ch6/test_string_atoi` | b | no issue | 6.16.9 | atoi reads the leading digits and underscores and no sign, so a string that starts with - or a space gives 0. Verilator 5.052 reads them as C's atoi does. | pass |
| `ch6/test_string_atoi_2` | b | no issue | 6.16.9 | atoi reads the leading digits and underscores and no sign, so '-9876' and '   42' give 0. Verilator 5.052 reads them as C's atoi does. | pass |
| `ch6/test_string_hextoa` | b | no issue | 6.16.12, 6.16.9 | hextoa is the inverse of atohex, which reads no sign, so 32'hDEADBEEF gives deadbeef (the test takes either case). Icarus 13.0 gives -21524111. | pass |
| `ch6/test_string_methods_corner` | a | no issue | 6.16 | Found by the text scan only: the X is a letter of a string (putc gives XBCDE). No x or z value takes part. | pass |
| `ch6/test_string_methods_extended` | a | no issue | 6.16 | Found by the text scan only: the X is a letter of a string ('AXC'). No x or z value takes part. | pass |
| `ch6/test_tri_edge_cases` | b | no issue | 6.6.1, 6.6.5 | A tri net whose only driver gives z is z. Verilator, a two-state simulator, has no x or z and prints known values there. | pass |
| `ch6/test_value_logic` | bc | no issue | 6.3.1 | logic keeps 1'bx and 1'bz, so the === checks hold. Verilator, a two-state simulator, has no x or z and prints known values there. | pass |
| `ch6/test_wire_edge_cases` | b | no issue | 6.6.1 (Table 6-2) | Two drivers of z give z. Verilator, a two-state simulator, has no x or z and prints known values there. | pass |
| `ch6/test_x_value_bitwise` | bc | no issue | 11.4.8 (Tables 11-11, 11-12) | 4'bxx10 & 4'b1001 is x000 and the other lines follow the same tables. Verilator, a two-state simulator, has no x or z and prints known values there. | pass |
| `ch7/test_array_nba_part_select_index` | d | no issue | 4.7, 9.4.2, 10.4.2, 4.5 | Two orders are open, and neither affects the output. First, at time 0 `rst_n = 0` (l.61, x->0, a negedge) may or may not be caught by the always_ff, depending on start order (4.7). Either way the posedge at t=5 with rst_n=0 resets the pointers. Second, at t=15 `rst_n = 1` (l.66) is written by blocking assignment right at the posedge the always_ff samples. | pass |
| `ch7/test_array_reduction` | a | no issue | 7.12.3 | Found by the text scan only: the x is the name of the iterator in the text sum(x * 2). No x or z value takes part. | pass |
| `ch7/test_assignment_forms` | d | no issue | 4.7, 10.4.2, 10.6.2, 4.6 | `ff_in = 8'h77` (l.218) is written at a posedge at which the always_ff samples it (4.7), but the check follows two posedges, so ff_out is 77 either way. The other checks follow a posedge or `#1`. | pass |
| `ch9/test_always_blocks` | d | no issue | 6.8, 9.4.2, 9.2.2.2, 10.4.2 | Declaration initializers run before processes start (6.8), so time 0 has no edge, and L45 prints `counter`=0 at t=1. | pass |
| `ch9/test_always_comb_cascade_propagation` | bd | no issue | 9.2.2.2, 4.4.2.3, 4.5 | #0 resumes the process only when the Active region is empty (4.4.2.3, 4.5), so the continuous assignments and always_comb blocks woken before it have run. Verilator 5.052 shows the values from before. Event order: no printed line depends on it. | pass |
| `ch9/test_always_comb_latch` | bcd | no issue | 6.8 (Table 6-7), 9.2.2.3, 12.4 | The latch's condition is never true before the check, so latch_out keeps its initial x. Verilator, a two-state simulator, has no x or z and prints known values there. Event order: no printed line depends on it. | pass |
| `ch9/test_always_comb_latch_ff` | d | no issue | 9.2.2.2.2, 9.4.2, 10.4.2, 4.6 | rst_n is released at a negedge (77, t=20), so the five following posedges (25..65) each increment counter, and the read at the t=70 negedge gives 5. | pass |
| `ch9/test_always_ff_async` | d | no issue | 4.7, 9.4.2, 10.4.2 | At t=0, `clk` x->0, `rst_n`/`set_n` x->1 match none of the always_ff edge lists, so the t=0 order is irrelevant, and the reset is applied at t=1. | pass |
| `ch9/test_always_ff_corner` | bd | no issue | 9.2.2.4, 6.8 | q_enable is never written while en is 0, so it is still x. Verilator, a two-state simulator, has no x or z and prints known values there. Event order: no printed line depends on it. | pass |
| `ch9/test_always_ff_edge_cases` | d | no issue | 4.6, 9.4.2, 10.4.2 | Only the initial block makes edges on clk and rst_n. In each test it sets `rst_n`, `d` and `load` before the statement that creates the edge, for example `rst_n = 1; d = 8'hAA; clk = 1;` (l.65-67). | pass |
| `ch9/test_always_ff_simple` | d | no issue | 10.4.2, 4.6 | data_in changes 1 time unit before each `clk = 1`, and data_out is read 2 units after. | pass |
| `ch9/test_always_latch_edge_cases` | bc | no issue | 9.2.2.3, 6.8 | q is never assigned while en is 0, so it keeps x. Verilator, a two-state simulator, has no x or z and prints known values there. | pass |
| `ch9/test_assign_delay_edge_cases` | d | no issue | 9.4.1, 9.4.5, 9.4.2, 4.6 | Only the initial reads or writes `data`, `t1` and `t2`. The clock process only toggles `clk`. | pass |
| `ch9/test_clocked_value_timing` | d | no issue | 10.4.2, 4.4.2.4, 9.4.2, 20.4.3 | `value` is read `#1` after the edges (t=6, t=36), after the NBA region of the edge has run, which gives 1 and 4. | pass |
| `ch9/test_disable_wait` | d | no issue | 4.6, 9.6.2, 9.4.3 | A single process. The named event done_event is declared but never triggered or waited on. | pass |
| `ch9/test_edge_detection` | d | no issue | 6.8, 9.4.2, 10.4.2 | The counters are written only by the edge-triggered always blocks and are read `#1` after the edges (t=1, 6, 11, 56, 66). | pass |
| `ch9/test_event_ctrl` | d | no issue | 9.4.2, 4.7 | The only same-time pair is t=0: `clk = 0` (l.21, x->0) against the main initial reaching `@(posedge clk)` (l.30). x->0 is not a posedge, so in either order the first wake-up is at t=5. | pass |
| `ch9/test_implicit_event` | b | no issue | 9.4.2.2, 9.4.2 | The block reads and writes trigger_count, but a write made while it runs does not wake it, because it is not waiting then. Verilator 5.052 stops with DIDNOTCONVERGE. | pass |
| `ch9/test_initial_block` | d | no issue | 6.8, 4.5, 4.7 | The writers run at t=0 (v1), t=2 (v2) and t=3 (v3). The reader runs at t=1 and t=4, so no read shares a time step with another process's write. | pass |
| `ch9/test_initial_edge_cases` | d | no issue | 9.2.1, 6.8, 6.21, 4.6 | The scan saw `a` written by several procedures. The three small initials write a/b/c at t=0, and the main initial reads after `#1` (L40). | pass |
| `ch9/test_initial_final` | d | no issue | 4.7, 9.2.1, 9.2.3 | The two t=0 initials write disjoint variables, so their relative order does not matter, and the main initial reads them after `#1` (l.45). | pass |
| `ch9/test_multi_sensitivity_list` | d | no issue | 6.8, 4.7, 9.4.2, 10.4.2 | The declaration initializers (l.17-53) apply before procedures start and create no events (6.8), so Test 1 reads q=0, counter=0. | pass |
| `ch9/test_scope_nba` | d | no issue | 6.8, 10.4.2, 4.6 | A single process. The block-scoped static variables are initialized before time 0 (6.8). | pass |
| `ch9/test_sens_level` | d | unspecified | 4.7, 9.4.2 | Line 108 writes a = 0; b = 1; with no time control between them, both read by always @(a or b or c). Under 4.7 the initial may be suspended after a = 0, and the always block may run and be suspended too before it waits again; b = 1 then wakes nothing and out3 stays 00. This needs two suspensions, not only a choice of which process runs first. See 4.1. | pass |
| `ch9/test_sens_multi` | d | no issue | 9.4.2, 4.7, 10.4.2 | At t=0 the start order of the procedures is open, but the initial writes are x->1 for `rst_n` and x->0 for `clk` and `sig1`-`sig3`, so none matches the edge lists, and `comb_out` is recomputed from all of a, b and c on the later change at l.101. | pass |
| `ch9/test_sensitivity_edge_cases` | bd | no issue | 9.2.2.2, 4.4.2.3, 4.5 | After a = 1; b = 2; #0; a = 3; b = 4; the always_comb runs again and gives 7: it ran and waited again before #0 resumed the initial. Verilator 5.052 gives 3. Event order: no printed line depends on it. | pass |
| `ch9/test_sequential_logic` | d | no issue | 9.4.2, 10.4.2 | The initial changes rst_n and data_in only at negedges (54 at t=10, 77 at t=50, 89 at t=60), and checks at negedges. | pass |
| `ch9/test_stmt_disable_2` | d | no issue | 9.6.2, 4.6 | The test has one procedure. The named event `done_event` (L17) is declared but never triggered or waited on. | pass |
| `ch9/test_timing_edge_cases` | d | no issue | 4.7, 9.4.1, 9.4.5 | Only the main block writes and reads `data`, so the `#0` statements (l.33, l.44-46) and the intra-assignment delay (l.70) are plain sequential behaviour. | pass |
