# Reanalysis of 176 kirimasim failures — responsibility and priority

[日本語](kirimasim_failure_analysis_2026-10-28.ja.md)

> Historical reanalysis. Later work corrects some judgments/counts below. Before treating them as current remaining work,
> consult the [criteria audit](P1_CRITERIA_GAPS_2026-10-28.md) and [current support table](P1_SUPPORT.md).

Created: 2026-10-28 / baseline: `main` `a2420294` (`kirimasim 0.0.0`).
Original: [failure handoff](kirimasim_failure_handoff_2026-10-28.md).
Results: `/tmp/opencode/kirimasim-final.json` (7,740 tests, 462 fail).

> This is an AI reanalysis without independent human confirmation.
> The original analysis checked relevant IEEE 1800-2023 clauses in a local PDF;
> only clause numbers and paraphrases are recorded, without quotations.

---

## 0. Conclusion: corrections to the previous handoff

The handoff can be read as “fix all 176 in kirimasim”, but **responsibility splits three ways**.

| Responsibility | Count | Description |
|---|---:|---|
| **slang (frontend)** | **111** | Compile-time diagnostics; slang is pinned and unmodified, so these cannot be fixed in this repository’s `src/` |
| **kirimasim (runtime/driver)** | **25** | 20 runtime outputs + 2 diagnostics/exit codes + 3 config selection cases |
| **kirimasim-tests (test side)** | **40** | Mistaken expectations, regexes, illegal designs and syntax; 39 confirmed by explicit clauses + 1 likely |

Thus **25 cases are the actual kirimasim bugs to prioritize**. Of the other 151, 111 concern upstream slang and 40 test corrections.
**Eight of the 25 share one cause** (config is not applied with `--top`), reducing independent kirimasim causes to about **18**.

### Particularly important corrections

1. **The five ch33 config runtime tests have correct expectations; kirimasim's `--top` handling is responsible.**
   The runner always passes `--top=<module>` for `:top_module:`. `chooseConfigTop` returns immediately if `topModules`
   is nonempty, so no config applies. Removing `--top` makes all five pass on the same binary (§3.1).
2. **The seven ch23 uwire/inout tests are correct; slang is too strict.** 6.6.2 forbids multiple drivers and connection
   to a bidirectional terminal of a bidirectional pass switch, not inout ports. Icarus 13.0 and installed Verilator 5.050
   accept them. Test comments say 5.052 rejects them; 5.052 behavior is unconfirmed.
3. **Five ch17/ch6 checker tests violate the standard.** 17.5 restricts initial bodies in checkers to let, assertions and
   event controls; 17.7.1 forbids assigning checker variables in initial. They contradict suite tests such as
   `test_illegal_checker_variable_initial_assignment`.
4. **The ch21 readmemh expectation is wrong.** Under 21.4/21.4.3, start/finish name addresses in the highest dimension.
   2–5 for `m[0:1][0:2]` is out of range; kirimasim's warning and unchanged values conform.
   The suite already lists this as a candidate in `docs/simulator-differences.md`.
5. **Four ch20 `$fatal` / severity tests have format-dependent regexes.** kirimasim prints `file:line: fatal: msg`,
   Icarus/Verilator `FATAL: file:line: msg`. 20.10 leaves formatting to tools, so accept either order.
   Two tests were corrected on 2026-10-25; these four were missed.

### Breakdown of 286 cases without `:expected_stdout:`

The handoff calls all 286 test maintenance; actually there are four groups.

| Kind | Count | Treatment |
|---|---:|---|
| `:type: formal` (formal verification) | 268 | Exclude from simulation runner (handled by `run_sby.py`) |
| kirimasim returns `error: unsupported:` | 16 | Unsupported rather than fail |
| Only `:expected_stdout_verilator:` is present | 1 | Exclude for kirimasim, or add a generic expectation |
| Neither output nor expectation | 1 | Add an empty `.expected` |

The corrected aggregate is:

| Status | Count |
|---|---:|
| pass | 5,053 |
| Actual failures | 176 |
| No expected output (test maintenance) | 2 |
| formal (excluded) | 268 |
| unsupported | 2,241 (2,225 + 16) |
| **Total** | **7,740** |

The executable denominator 5,229 and 96.63% match rate stay unchanged, but 268 of the 462 failures are formal and 16 unsupported.

---

## 1. Types of evidence

| Evidence | Use |
|---|---|
| 462 failures in `kirimasim-final.json` | Extract 176 actual failures (exclude 286 lacking `:expected_stdout:`) |
| `coverage/results/refresh-20261027-icarus13.json` (7,613 cases) | Icarus 13.0 results, supporting tests whose `:oracle:` names iverilog |
| `coverage/results/verilator.json` (3,912 cases) | Verilator 5.052 results, supporting tests whose `:oracle:` names verilator |
| `coverage/results/ghost-local-20261028.json` (7,629 cases) | Reference only (weak evidence; may not be an independent implementation) |
| Suite’s `docs/simulator-differences.md` | Known Icarus/Verilator differences from the standard; readmemh already listed |
| Suite’s `docs/lyra-disagreement-review-20261025.md` | Rechecked 194 spec-only tests; concluded zero expectation errors |
| IEEE 1800-2023 PDF (local) | Checked 6.6.2, 6.20.5, 12.7.3, 16.4.1, 16.10, 17.5, 17.7.1, 21.4, 21.4.3, 24.3, etc. |
| Execution experiments | With/without `--top`; minimal reproducers on Icarus/Verilator |

`:oracle:` breakdown for the 176:

| oracle | Count |
|---|---:|
| `spec` only (not simulator-verified) | 114 |
| Icarus/Verilator named and that tool passes | **38** |
| Other (oracle named, result missing/known) | 24 |

**38 have the named oracle tool matching expectations**, strong support for those expectations.
The 114 spec-only cases have AI-derived expectations and need individual clause checks.

---

## 2. Breakdown by responsibility

### 2.1 By chapter

| Chapter | fail | slang | kirimasim | Test |
|---|---:|---:|---:|---:|
| ch23 | 20 | 16 | 2 | 2 |
| ch16 | 18 | 9 | 2 | 7 |
| ch33 | 16 | 8 | 8 | 0 |
| ch6 | 15 | 13 | 0 | 2 |
| ch20 | 13 | 5 | 1 | 7 |
| ch11 | 12 | 5 | 4 | 3 |
| ch17 | 12 | 4 | 0 | 8 |
| ch21 | 9 | 7 | 1 | 1 |
| ch25 | 9 | 6 | 0 | 3 |
| ch7 | 8 | 5 | 2 | 1 |
| ch19 | 7 | 6 | 0 | 1 |
| ch22 | 7 | 6 | 0 | 1 |
| ch12 | 5 | 2 | 1 | 2 |
| ch5 | 5 | 4 | 0 | 1 |
| ch18 | 4 | 3 | 0 | 1 |
| ch13 | 3 | 2 | 1 | 0 |
| Other | 13 | 10 | 3 | 0 |
| **Total** | **176** | **111** | **25** | **40** |

### 2.2 By failure kind

| Kind | Count | slang | kirimasim | Test |
|---|---:|---:|---:|---:|
| Compilation errors (exit 2) | 104 | 67 | 3 (config selection) | 34 |
| Accepted illegal designs | 44 | 44 | 0 | 0 |
| Runtime output mismatches | 21 | 0 | 20 | 1 |
| Diagnostic text / exit codes | 7 | 0 | 2 | 5 |

---

## 3. The 25 cases to fix in kirimasim

### 3.1 ch33 config selection (8 cases, one cause)

Affected:

- Five runtime cases: `test_config_cell_parameter`, `test_config_instance_over_cell`,
  `test_config_binding_descends_through_children`, `test_config_library_cell_and_topmodule_names`,
  `test_config_rules_bind_each_instance`.
- Three compilation cases: `test_config_hierarchical`, `test_config_name_disambiguation_config_suffix`,
  `test_config_use_config_suffix`.

Cause: `chooseConfigTop` in `src/front/front.cpp`:

```cpp
bool chooseConfigTop(slang::driver::Driver& driver, std::string& diagnostics) {
    if (!driver.options.topModules.empty()) return true;   // This return
    ...
}
```

The runner `tools/run_kirimasim.py` passes `--top=<module>` for `:top_module:`, so config selection is skipped
and the unconfigured module is elaborated.

Reproducer (same binary, changing only `--top`):

```sh
kirimasim tests/ch33/test_config_cell_parameter.sv
# u1=5 u2=5   (expected)

kirimasim --top=test_config_cell_parameter tests/ch33/test_config_cell_parameter.sv
# u1=1 u2=1   (config not applied)
```

The comment in `test_config_library_cell_and_topmodule_names.sv` also says that the config design names the test module
and `:top_module:` names it. The test intent is clear.

**Proposed fix:**

1. With `--top=<module>`, find a config whose design names `<module>` and put `"<config>:config"` in `topModules`.
2. Without `--top`, do not reject multiple configs. Choosing the last matches test expectations.
3. If `--top` names a config directly, preserve current `:config` handling.

This one change should pass eight cases. The three “second config” errors use the same function and belong to the same issue.

### 3.2 defparam application order and precedence (2 cases)

Affected: `ch23/test_defparam_last_in_source_text_wins`, `ch23/test_parameter_override_type_and_range`.
23.10.1 makes the last source-text defparam to a parameter win;
23.10.4 defines resolution against instance parameter assignments.

Observed kirimasim behavior:

- A stays 1 (first defparam); B stays 3 (first in the list).
- E takes 7 from the earlier module; only C takes 6 from the later module.
- u3 takes instance assignment 8'hF0 rather than defparam 2.5.

It overwrites in module-processing order instead of source order, inconsistently keeping first or last assignments.
Unify defparam resolution around source-text order.

### 3.3 Expression typing and precedence (4 cases)

| Test | Actual / expected | Root cause |
|---|---|---|
| `ch11/test_conditional_real_shortreal_branch_promotion` | `16777217.0` / `16777216.0` | Single-precision rounding of shortreal branch missing |
| `ch11/test_expr_conditional_ambiguous` | `arr=1,2` / `arr=1,0` | Element-wise aggregate merge for x/z predicate (11.4.11) missing |
| `ch11/test_precedence_distinguishing_values` | `conditional_before_implies=0` / `1` | `?:` / `->` precedence reversed (Table 11-2 places `?:` above) |
| `ch11/test_precedence_logical_conditional_implication_levels` | `1 ? 0 : 1 -> 0 = 0` / `1` | Same as above |

The last two share one precedence bug. The first two need separate 11.4.11 type-determination fixes.

### 3.4 SVA deferred assertions (2 cases)

Affected: `ch16/test_deferred_flush_points`, `ch16/test_syntax_deferred_immediate_assertions`.

- `test_deferred_flush_points`: always_latch updates do not flush pending reports, producing `latch fail`.
  Missing 16.4.2 flush points include always_comb/always_latch resume, event-control resume and outermost-scope disable.
- `test_syntax_deferred_immediate_assertions`: first line is `wrong`; `assert #0` evaluation order/sample values differ.

Both concern 16.4's pending queues/flush mechanism and should be addressed together.

### 3.5 Programs and Reactive (2 cases)

Affected: `ch4/test_reactive_region_set_order`, `ch24/test_program_port_reactive_variable_and_net`.

- ch4: the program records a value before the module. 4.5 processes the Active set first, so the module should appear first.
- ch24: program-port net `mw` stays 0; ordering between Reactive program execution and port updates is involved.

These likely share the program-in-Reactive mechanism and merit joint investigation.

### 3.6 Other individual cases (7, including two diagnostics)

| Test | Actual / expected | Suspected cause |
|---|---|---|
| `ch9/test_always_comb_longest_static_prefix_and_locals` | `runs=3` / `2` | Reading `vec[2]` makes all of `vec` sensitive; longest static prefix of 9.2.2.2.1 |
| `ch12/test_syntax_loop_statement_forms` | `both=1` / `0` | `foreach (m2[, ])` runs once; 12.7.3 requires no iteration when all loop variables are omitted |
| `ch7/test_queue_method_prototypes_and_invalid_indices` | `size=5` / `6` | Index of `insert(64'h1_0000_0002, 7)` not truncated to integer, treated as out of range |
| `ch7/test_queue_invalid_index_write_ignored_warning` | No warning | 7.10.1 requires a runtime warning for out-of-range/unknown-index writes (not optional) |
| `ch13/test_constant_function_call_fresh_initial_values` | `P1=1 P2=2` / `11 12` | Static local initial value 10 absent from constant-function calls; 13.4.3 |
| `ch21/test_vcd_scope_commands_begin_fork_task` | fork scope 0 / 1 | Named fork’s `$scope` absent from VCD; 21.7.2.3 |
| `ch20/test_illegal_timeformat_unit_out_of_range` | exit 3 / 2, `internal error` | Out-of-range unit produces runtime internal error; use ordinary error for violation of 20.4.3 |

Both Icarus and Verilator reproduce ch13's expectation, confirming a kirimasim bug.

---

## 4. Forty test-side errors

### 4.1 Thirty-nine checked against explicit standard wording

| # | Test | Clause | What the test does / correct treatment |
|---|---|---|---|
| 1 | `ch11/test_A_extra_specparam_range` | 6.20.5 | `localparam lo = sp[1:0]`; specparam cannot initialize parameter/localparam; compilation error is correct |
| 2 | `ch17/test_checker_body_procedural_and_generate_items` | 17.5, 17.7.1 | Checker initial uses `side = 7`; only let, assertions and event controls are allowed there |
| 3 | `ch17/test_checker_declaration_and_port_forms` | 17.5, 17.7.1 | `initial none_n = 1`, `initial empty_n = 2`, `initial y2 = y2 + 1`; all forbidden |
| 4 | `ch17/test_checker_rand_and_nested_checker` | 17.7.1 | `initial rf_out = rf`; checker variables cannot be assigned in initial |
| 5 | `ch6/test_design_element_static_lifetime` | 17.5, 17.7.1 | Checker `initial cv = 6` and plain `always ... $display`; both forbidden |
| 6 | `ch23/test_scope_creators_all_kinds` | 17.5 | Checker `initial #9 $display`; # delay and $display are not allowed in initial |
| 7 | `ch17/test_checker_assertion_placement` | 16.4.1 | Deferred-assertion action is `def_fail = def_fail + 1`; it must be a subroutine call |
| 8 | `ch17/test_checker_body_items` | 16.4.1 | Same as above |
| 9 | `ch17/test_checker_procedural_deferred_assertion` | 16.4.1 | Same as above |
| 10 | `ch17/test_checker_declaration_scope_kinds` | 24.3 | Module reads `p.hit`; referencing a program signal from outside a program is an error |
| 11 | `ch21/test_readmemh_address_range` | 21.4, 21.4.3 | start=2, finish=5 for `m[0:1][0:2]` is outside the highest dimension; kirimasim’s warning is correct |
| 12 | `ch20/test_fatal_form_finish_number_and_arguments` | 20.10 | Regex requires `fatal ... file:line:` order; format is tool-defined |
| 13 | `ch20/test_fatal_form_finish_number_only` | 20.10 | Same as above |
| 14 | `ch20/test_fatal_form_without_parentheses` | 20.10 | Same as above |
| 15 | `ch20/test_severity_tasks_report_contents` | 20.10 | Requires info/warn/error/fatal ordering; content matches |
| 16 | `ch16/test_ch16_sim_clocking_single` | 16.16 | Requires `.*multiclock`, but same line gets explicit-clock-in-clocking-block error; both are valid violations |
| 17 | `ch11/test_conditional_predicate_and_integral_branch` | 12.6.3 | Parenthesizes `(x matches ...)` as an expression; matches is a cond_pattern, not an expression via parentheses; remove parentheses |
| 18 | `ch12/test_pattern_conditional_predicate` | 12.6.3 | Same as above |
| 19 | `ch12/test_pattern_match_known_result` | 12.6 | Assigns `matches` as a general expression; allowed only in a predicate or `case ... matches` |
| 20 | `ch19/test_cg_bin_syntax_all_forms` | 19.5 | `bins wc = wildcard {...}`; `wildcard` belongs before `bins` |
| 21 | `ch23/test_syntax_port_expression_and_explicit_ports` | 23.2.2.2, Annex B | Port name `cell` is reserved; rename it |
| 22 | `ch11/test_tagged_union_expression_and_matches_precedence` | 12.6 | Uses `matches` in a `$display` expression; allowed only in a predicate or `case ... matches` |
| 23 | `ch16/test_nexttime_always_eventually_alignment` | 16.13.1 | Combines differently clocked operands with `and`; only `##1`/`##0` allowed |
| 24 | `ch16/test_triggered_matched_multiclock` | 16.13.1 | Combines multiclocked operands with `##[1:2]`; only `##1`/`##0` allowed |
| 25 | `ch22/test_22_extra_include_syntax` | 22.4 | Expects angle-bracket includes to search `:incdirs:` |
| 26 | `ch25/test_interface_extern_and_generate_items` | 25.3 | Instantiates a module inside an interface |
| 27 | `ch16/test_ch16_sim_checker_history` | 16.9.4 | Calls `$future_gclk` in an assertion action block |
| 28 | `ch16/test_sequence_formal_arguments_substitution` | 16.8.1 | `input int z` lacks `local` |
| 29 | `ch16/test_sequence_local_formal_assignment` | 16.8.1 | Actual `w` for inout local formal is not a local variable |
| 30 | `ch25/test_virtual_interface_data_type_forms` | 25.9 | Uses a virtual interface as a port |
| 31 | `ch6/test_void_return_and_tagged_union_void_member` | 6.13 | Uses `out` as direction keyword; correct keyword is `output` |
| 32 | `ch7/test_union_grammar_forms` | 7.3.1 | Non-integral `shortreal` in soft packed union |
| 33 | `ch20/test_array_query_dimension_counts` | 20.7 | real/shortreal/event are neither arrays nor integral, outside function domain |
| 34 | `ch20/test_array_query_functions` | 20.7 | Same as above |
| 35 | `ch20/test_array_query_invalid_dimension_all_x` | 20.7 | Same as above |
| 36 | `ch18/test_uniqueness_constraint_forms` | 18.5.4 | Uniqueness-constraint variables lack `rand`; randomize requires it to assign values |
| 37 | `ch25/test_interface_abstract_methods_and_broadcast` | 25.5 | Reads `b.value`, absent from modport |
| 38 | `ch5/test_attribute_prefix_positions` | 6.5 | Uses `ub` before `wire ub;` declaration |
| 39 | `ch16/test_A_extra_repetitions` | 16.12.22 | Uses `1'b1[*]` as property; forbidden because it admits an empty match |

### 4.2 One likely case

| # | Test | Clause | Reason |
|---|---|---|---|
| 1 | `ch17/test_checker_always_statement_kinds` | 17.5 | `int acc;` in always_comb is absent from 17.5’s allowed list; block declarations may be read as not statements, so moving the declaration to checker body is safe |

### 4.3 Rules for test corrections

Under CLAUDE.md, expectation/test changes must not share a PR with `src/` changes.
Each correction PR must name the clause and explain why the current expectation contradicts it.
For #2–#5 especially, retain the purpose of testing checker static lifetime through legal declaration initialization
or `$display` in `always_ff`, rather than turning a legal-intent test into an illegal-design test.

---

## 5. The 111 upstream slang cases

### 5.1 Position

`CLAUDE.md` requires pinned, unmodified slang and forbids relaxing its diagnostics.
Thus these **cannot be fixed in kirimasim's `src/`**. Options:

1. Report upstream and adopt a fixed pin.
2. Correct tests that violate the standard (§4).
3. Classify unimplemented constructs as `unsupported`, rather than fail.

### 5.2 Families, most common first

| Family | Count | Assessment |
|---|---:|---|
| Accepted illegal designs (missing slang validation) | 44 | All 44 checked against standard text; Icarus also rejects 19 at expected locations |
| Rejects uwire inout | 7 | slang too strict (6.6.2 has no inout prohibition; Icarus/Verilator accept) |
| Rejects interconnect references | 5 | slang too strict (6.6.8 permits terminal connections/net_alias) |
| Rejects user-nettype connections/force | 5 | slang too strict (suite Lyra review confirms tests) |
| Rejects implicit integral-to-string conversion | 5 | $fopen/$dumpvars/plusargs keys; 21.3.1 permits integral; slang too strict |
| String / constant out-of-range array queries | 2 | 20.7; three real cases are outside domain, test-side (#18) |
| Rejects hierarchical checker-member reads | 2 | 17.7.1 forbids assignments only; $display reads legal; slang too strict |
| Rejects nonlocal sequence match-item assignments | 2 | 16.10 does not explicitly forbid it; needs confirmation (possibly too strict) |
| Rejects string/integral concatenation | 2 | 11.4.12; suite review confirms tests |
| Rejects non-bit-stream types in streams | 2 | 11.4.14.1; tests confirmed correct |
| Rejects program-parameter defparam/override | 2 | 23.10.1 permits program-instance parameters; slang too strict |
| Parser omissions (`)` `member` `;` `identifier` `?`, etc.) | 6 | Syntax; upstream slang parser bugs. Five separate cases are test syntax errors (§4) |
| Assertion restrictions (match item, port direction, multiclock, empty match, clock inference, etc.) | 12 | Mostly slang checks; compare individually with standard (overlaps two match-item cases above) |
| Ref type equivalence (`bit[7:0]` → `int`) | 2 | 13.5.2; possibly correct rejection; test needs checking |
| Other individual cases | About 15 | Check each against standard (overlaps mean table does not sum to 111) |

### 5.3 Forty-four accepted illegal designs

The suite itself prioritizes these for review because no simulator had rejected at the expected location,
leaving only AI verification. Cross-checking Icarus results confirmed **19** expected-location rejections.
The remaining 25 were checked against the standard on 2026-10-28; the analysis judged **all 44 tests correct
and slang checks missing**. Each clause is recorded in the ledger's `note`.

These need additional slang validation: group upstream issues by syntax/semantic check and prioritize checks
that fix multiple tests.

---

## 6. Revised priority

| Order | Cases | Count | Reason |
|---|---|---:|---|
| 1 | ch33 config selection with `--top` | 8 | One cause, eight cases: five runtime + three compilation |
| 2 | ch11 expressions (two precedence, two conditional) | 4 | Direct language semantics; one precedence fix affects two cases |
| 3 | ch23 defparam order/precedence | 2 | One mechanism, two cases; affects other defparam tests too |
| 4 | ch16 deferred-assertion flush | 2 | Pending-queue mechanism of 16.4 |
| 5 | ch4/ch24 program Reactive execution | 2 | Execution regions of 4.5/24.3 |
| 6 | Individual ch9/ch12/ch7 (queue-index truncation)/ch13/ch21 cases | 5 | Independent mechanisms; one case each |
| 7 | Two diagnostics (timeformat, queue-write warning) | 2 | Inexpensive |
| 8 | 40 test-side cases | 40 | Separate PR from `src/`; correct using standard clauses |
| 9 | 111 upstream slang cases | 111 | Upstream issues + pin update; not fixed here |
| 10 | 2,225 unsupported cases | — | Not failures; move to pass when implemented |

### Expected effect

- Steps 1–7 pass **25** cases (176 → 151).
- Step 8 corrects **40** tests (151 → 111, effectively waiting upstream).
- Reporting roughly 30 overly strict cases first could substantially reduce the upstream count at the next pin update.

---

## 7. Verification

Rerun by family:

```sh
python3 tools/run_kirimasim.py \
  --kirimasim build/release/src/driver/kirimasim \
  -j 8 --json /tmp/kirimasim-family.json \
  tests/ch33/test_config_*.sv
```

Confirm the `--top` cause:

```sh
kirimasim tests/ch33/test_config_cell_parameter.sv
kirimasim --top=test_config_cell_parameter tests/ch33/test_config_cell_parameter.sv
# First: u1=5 u2=5; second: u1=1 u2=1 confirms the cause described here
```

Check regressions:

```sh
python3 - <<'PY'
import json
old = json.load(open('/tmp/opencode/kirimasim-final.json'))['tests']
new = json.load(open('/tmp/kirimasim-family.json'))['tests']
reg = [k for k in old if k in new and old[k]['status'] == 'pass' and new[k]['status'] != 'pass']
imp = [k for k in old if k in new and old[k]['status'] != 'pass' and new[k]['status'] == 'pass']
print('regressions', len(reg), 'improvements', len(imp))
PY
```

Run the full suite for regressions and record the fixed family, before/after counts and clauses in the commit message.

---

## 8. Cautions

- **Do not rewrite expectations to match kirimasim output.** Correct tests from standard clauses/block IDs and update the kirimasim-tests ledger.
- Of the 286 in §0, 268 are formal, 16 unsupported and two test maintenance (kirimasim-tests #13/#14).
- The 2,225 unsupported cases are not failures; separate as `known-limitation` / `unsupported`.
- These classifications are AI-authored. In particular, have a person check the standard for §4.2 and §5.3's remaining 14 cases before fixes.
- When updating slang's pin, update the SHA in `cmake/Dependencies.cmake` and the records in `docs/notes/slang-*.md` together.

---

## 9. Registered issues

The following issues were registered from this analysis on 2026-10-28.

### kirimasim (runtime/driver)

| Issue | Description | Cases |
|---:|---|---:|
| [#58](https://github.com/kurochan001/kirimasim/issues/58) | `--top` skips config selection | 8 |
| [#59](https://github.com/kurochan001/kirimasim/issues/59) | defparam uses elaboration order rather than source order | 2 |
| [#60](https://github.com/kurochan001/kirimasim/issues/60) | x/z `?:` does not merge aggregates element-wise | 1 |
| [#61](https://github.com/kurochan001/kirimasim/issues/61) | shortreal branch not rounded to single precision | 1 |
| [#62](https://github.com/kurochan001/kirimasim/issues/62) | Missing deferred-assertion flush points | 2 |
| [#63](https://github.com/kurochan001/kirimasim/issues/63) | Program runs before module Active update | 2 |
| [#64](https://github.com/kurochan001/kirimasim/issues/64) | always_comb sensitive to entire vector for constant bit select | 1 |
| [#65](https://github.com/kurochan001/kirimasim/issues/65) | foreach with all loop variables omitted runs once | 1 |
| [#66](https://github.com/kurochan001/kirimasim/issues/66) | Queue-method index not converted to integer | 1 |
| [#67](https://github.com/kurochan001/kirimasim/issues/67) | No warning for invalid queue-index write | 1 |
| [#68](https://github.com/kurochan001/kirimasim/issues/68) | Constant call ignores static local initial value | 1 |
| [#69](https://github.com/kurochan001/kirimasim/issues/69) | Named fork absent from VCD `$scope` | 1 |
| [#70](https://github.com/kurochan001/kirimasim/issues/70) | Out-of-range `$timeformat` unit gives internal error / exit 3 | 1 |
| [#54](https://github.com/kurochan001/kirimasim/issues/54) | `?:` groups below `->` (existing) | 2 |

### kirimasim-tests (test side)

| Issue | Description | Cases |
|---:|---|---:|
| [#6](https://github.com/kurochan001/kirimasim-tests/issues/6) | Checker initial assignments / declarations in always_comb | 5 |
| [#7](https://github.com/kurochan001/kirimasim-tests/issues/7) | Deferred-assertion action is an assignment | 3 |
| [#8](https://github.com/kurochan001/kirimasim-tests/issues/8) | Severity / `$fatal` regex depends on word order | 5 |
| [#9](https://github.com/kurochan001/kirimasim-tests/issues/9) | readmemh start/finish treated as linear addresses | 1 |
| [#10](https://github.com/kurochan001/kirimasim-tests/issues/10) | specparam / program signal / checker delay | 3 |
| [#15](https://github.com/kurochan001/kirimasim-tests/issues/15) | Six parser-family cases use syntax outside the grammar | 6 |
| [#16](https://github.com/kurochan001/kirimasim-tests/issues/16) | Seven §4.3 cases use prohibited syntax/semantics | 7 |
| [#17](https://github.com/kurochan001/kirimasim-tests/issues/17) | Six §4.3 cases use prohibited syntax/semantics | 6 |
| [#18](https://github.com/kurochan001/kirimasim-tests/issues/18) | Array queries receive real/shortreal/event | 3 |
| [#19](https://github.com/kurochan001/kirimasim-tests/issues/19) | `1'b1[*]` used as a property | 1 |

### slang (waiting upstream)

| Issue | Description | Cases |
|---:|---|---:|
| [#71](https://github.com/kurochan001/kirimasim/issues/71) | Rejects uwire inout | 7 |
| [#72](https://github.com/kurochan001/kirimasim/issues/72) | Rejects interconnect terminals/net_alias/nested ports | 5 |
| [#73](https://github.com/kurochan001/kirimasim/issues/73) | Rejects user-nettype connections/force/partial drive | 5 |
| [#74](https://github.com/kurochan001/kirimasim/issues/74) | Rejects implicit integral-to-string conversion | 5 |
| [#75](https://github.com/kurochan001/kirimasim/issues/75) | Rejects real/dynamic array-query types | 5 |
| [#76](https://github.com/kurochan001/kirimasim/issues/76) | Rejects program/block/task/function parameters | 3 |
| [#77](https://github.com/kurochan001/kirimasim/issues/77) | Rejects checker hierarchical reads/covergroups/empty ports | 5 |
| [#78](https://github.com/kurochan001/kirimasim/issues/78) | Parser omissions | 11 |
| [#79](https://github.com/kurochan001/kirimasim/issues/79) | Missing preprocessor/top/package/config checks | 16 |
| [#80](https://github.com/kurochan001/kirimasim/issues/80) | Missing interface/modport checks | 7 |
| [#81](https://github.com/kurochan001/kirimasim/issues/81) | Missing covergroup checks | 4 |
| [#82](https://github.com/kurochan001/kirimasim/issues/82) | Missing net/type/constant/force checks | 17 |
| [#83](https://github.com/kurochan001/kirimasim/issues/83) | Other excessive checks (needs splitting) | 44 |
| [#98](https://github.com/kurochan001/kirimasim/issues/98) | `$bits` on unpacked union | 1 |
| [#99](https://github.com/kurochan001/kirimasim/issues/99) | Requires `virtual` for interface-class method implementation | 1 |

51 new issues (52 including existing #54) cover the 176 tests.

### Remaining 14 cases (needs-review)

| Issue | Description | Cases |
|---:|---|---:|
| [#85](https://github.com/kurochan001/kirimasim/issues/85) | `[*]` empty match | 1 |
| [#86](https://github.com/kurochan001/kirimasim/issues/86) | Recursive property after `##1` | 1 |
| [#87](https://github.com/kurochan001/kirimasim/issues/87) | Concurrent assertion in automatic task | 1 |
| [#88](https://github.com/kurochan001/kirimasim/issues/88) | Nonlocal match-item assignment | 2 |
| [#89](https://github.com/kurochan001/kirimasim/issues/89) | `$timeunit` in constant expression | 1 |
| [#90](https://github.com/kurochan001/kirimasim/issues/90) | `ref` actuals in task call | 1 |
| [#91](https://github.com/kurochan001/kirimasim/issues/91) | string → unpacked byte-array cast | 1 |
| [#92](https://github.com/kurochan001/kirimasim/issues/92) | Constant out-of-range array index | 1 |
| [#93](https://github.com/kurochan001/kirimasim/issues/93) | `local::` name resolution | 1 |
| [#94](https://github.com/kurochan001/kirimasim/issues/94) | String comparison in constraint | 1 |
| [#95](https://github.com/kurochan001/kirimasim/issues/95) | Derived-covergroup cross redefinition | 1 |
| [#96](https://github.com/kurochan001/kirimasim/issues/96) | Cross select expression | 1 |
| [#97](https://github.com/kurochan001/kirimasim/issues/97) | Module-path expression in state-dependent path | 1 |

### Classification of 286 cases outside the 176

| Issue | Description |
|---:|---|
| [#13](https://github.com/kurochan001/kirimasim-tests/issues/13) | Simulation runner executes `:type: formal` (268 cases) |
| [#14](https://github.com/kurochan001/kirimasim-tests/issues/14) | `no :expected_stdout:` checked before unsupported (18 cases) |
| [#84](https://github.com/kurochan001/kirimasim/issues/84) | (slang) Coverpoint name in covergroup bin-set expression fails to resolve to outer parameter (one case) |

### Per-test ledger

- [Triage ledger](kirimasim_failure_triage_2026-10-28.md): all 176 by responsibility, issue and confidence.
- [CSV](kirimasim_failure_triage_2026-10-28.csv): the same content for spreadsheets/scripts.

The ledger has 168 `confirmed`, eight `likely` and zero `needs-review`.
Do not declare the eight `likely` cases slang bugs before a human checks the standard text.

On 2026-10-28, the 25 of the 44 accepted-illegal-design cases not confirmed with Icarus were also checked against the standard.
All 44 were judged correct tests with missing slang checks.
