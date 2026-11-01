# Triage of 65 non-slang failures (2026-10-28)

[日本語](kirimasim_nonslang_triage_2026-10-28.ja.md)

> Historical triage as of 2026-10-28. Diagnoses and proposed fixes below are retained as recorded,
> not current approved decisions. Follow-up corrections are in the [criteria audit](P1_CRITERIA_GAPS_2026-10-28.md).
> Counts in the original prose have not been recomputed.

Scope: of 176 failures, **25 kirimasim + 40 test-side cases**, excluding slang causes.
The remaining 112 slang cases are tracked separately in
[the failure triage](kirimasim_failure_triage_2026-10-28.md) and `docs/notes/slang-*-issue-draft.md`.

- kirimasim: **all 25 confirmed** (#54, #58–#70).
- Test side: **39 confirmed + 1 likely** (#6–#10, #15–#19).
- These 65 do not include slang judgments.

---

## 1. The 25 kirimasim cases

### 1.1 ch33 config selection (8 cases, #58)

**Root cause:** `chooseConfigTop` in `src/front/front.cpp` returns immediately when
`driver.options.topModules` is nonempty. The runner supplies `--top=<module>` for a `:top_module:` header,
so config is not applied. Without `--top`, the first config is selected and a second is an error.

**Proposed fix:** with `--top=<module>`, select a config whose design statement names that module,
and use `"<config>:config"` as top. Without `--top`, select the last config to match test expectations
instead of rejecting the second.

| Test | Symptom |
|---|---|
| `ch33/test_config_cell_parameter` | `u1=1 u2=1` (expected `u1=5 u2=5`) |
| `ch33/test_config_instance_over_cell` | `u1=a1 u2=a1` (expected `u1=b1 u2=c1`) |
| `ch33/test_config_binding_descends_through_children` | m2's leaf does not become alt |
| `ch33/test_config_library_cell_and_topmodule_names` | Remains `work.cfg_a` |
| `ch33/test_config_rules_bind_each_instance` | a2 remains `crb_adder` |
| `ch33/test_config_hierarchical` | `second config 'cfg'` error |
| `ch33/test_config_name_disambiguation_config_suffix` | `second config 'ccn'` error |
| `ch33/test_config_use_config_suffix` | `second config 'cfg'` error |

**Evidence:** removing `--top` makes five cases match expectations on the same binary.
The test comments also say the config's design names the `:top_module:` module.

### 1.2 defparam (2 cases, #59)

**Root cause:** defparams are applied in elaboration order rather than source-text order;
the priority between defparams and instance assignments is reversed too.

**Proposed fix:** apply defparams to the same parameter in source order, with priority over
instance parameter assignments (23.10.1, 23.10.4).

| Test | Symptom |
|---|---|
| `ch23/test_defparam_last_in_source_text_wins` | `A=1 B=3 E=7` (expected `A=2 B=4 E=8`) |
| `ch23/test_parameter_override_type_and_range` | `u3.U==2.5` is 0 (defparam loses) |

### 1.3 Precedence of `?:` and `->` (2 cases, existing #54)

**Root cause:** `?:` groups below `->`; Table 11-2 places `?:` above it.

| Test | Symptom |
|---|---|
| `ch11/test_precedence_distinguishing_values` | `conditional_before_implies=0` (expected 1) |
| `ch11/test_precedence_logical_conditional_implication_levels` | `1 ? 0 : 1 -> 0 = 0` (expected 1) |

### 1.4 Conditional-operator type determination (2 cases, #60 / #61)

| Test | Root cause | Proposed fix |
|---|---|---|
| `ch11/test_expr_conditional_ambiguous` | Unequal aggregates are not merged element-wise for an x/z predicate | Implement the element-wise merge of 11.4.11 |
| `ch11/test_conditional_real_shortreal_branch_promotion` | shortreal branch remains double precision | Round to single precision in the shortreal result type |

### 1.5 Deferred assertions (2 cases, #62)

**Root cause:** missing flush points from 16.4.2 (always_comb/always_latch resume,
event-control resume and outermost-scope disable).

| Test | Symptom |
|---|---|
| `ch16/test_deferred_flush_points` | `t=15 latch fail` (expected `latch pass`) |
| `ch16/test_syntax_deferred_immediate_assertions` | First line is `wrong` |

### 1.6 Programs and the Reactive region (2 cases, #63)

**Root cause:** the program runs before the module's Active update;
the program-port net is not updated before program resume.

| Test | Symptom |
|---|---|
| `ch4/test_reactive_region_set_order` | `program: v changed` (expected `module: ...`) |
| `ch24/test_program_port_reactive_variable_and_net` | `mw=0` (expected `mw=1`) |

### 1.7 Individual cases (9 cases)

| Test | Issue | Root cause / proposed fix |
|---|---|---|
| `ch9/test_always_comb_longest_static_prefix_and_locals` | #64 | Reading `vec[2]` makes all of `vec` sensitive; implement the longest static prefix of 9.2.2.2.1 |
| `ch12/test_syntax_loop_statement_forms` | #65 | `foreach (m2[, ])` iterates once; 12.7.3 requires no iteration when all loop variables are omitted |
| `ch7/test_queue_method_prototypes_and_invalid_indices` | #66 | Index of `insert(64'h1_0000_0002,7)` is not truncated to integer |
| `ch7/test_queue_invalid_index_write_ignored_warning` | #67 | Missing out-of-range write warning required by 7.10.1 |
| `ch13/test_constant_function_call_fresh_initial_values` | #68 | Static local's initial value 10 is not used in a constant-function call |
| `ch21/test_vcd_scope_commands_begin_fork_task` | #69 | Named fork's `$scope` missing from VCD |
| `ch20/test_illegal_timeformat_unit_out_of_range` | #70 | Out-of-range unit gives internal error / exit 3; change to ordinary error / exit 2 |

(The original count explanation adds seven #64–#70 cases, four #60/#61/#62/#63 cases and two #59 cases
for 13, then eight #58 and two #54 for 23, then the two #62 cases for 25.)

---

## 2. The 40 test-side cases

### 2.1 Checker legality (5 cases, #6)

| Test | Proposed correction |
|---|---|
| `ch17/test_checker_body_procedural_and_generate_items` | Replace `initial side = 7` with declaration initialization |
| `ch17/test_checker_declaration_and_port_forms` | Initialize `none_n`, `empty_n`, `y2` at declaration |
| `ch17/test_checker_rand_and_nested_checker` | Initialize `rf_out` at declaration |
| `ch6/test_design_element_static_lifetime` | Use checker `int cv = 6;` and `always_ff @(posedge clk) $display(...)` |
| `ch17/test_checker_always_statement_kinds` | **Likely:** move `int acc;` into checker body; declaration in for initializer needs checking |

### 2.2 Deferred-assertion actions (3 cases, #7)

Replace `assert #0 (...) else x = x + 1;` with a call to a small task that increments the counter.
16.4.1 requires the action to be a subroutine call.

- `ch17/test_checker_assertion_placement`
- `ch17/test_checker_body_items`
- `ch17/test_checker_procedural_deferred_assertion`

### 2.3 Severity / `$fatal` / clocking regexes (5 cases, #8)

The tests require the order `fatal ... file:line:`. Accept either `error[^\n]*file:line:`
or `file:line:[^\n]*error`. The clocking case should accept explicit-clock wording as well as `multiclock`.

- `ch20/test_fatal_form_finish_number_and_arguments`
- `ch20/test_fatal_form_finish_number_only`
- `ch20/test_fatal_form_without_parentheses`
- `ch20/test_severity_tasks_report_contents`
- `ch16/test_ch16_sim_clocking_single`

### 2.4 readmemh (1 case, #9)

start=2/finish=5 for `m[0:1][0:2]` is outside the outermost dimension.
Expect a warning and unchanged values instead; update the external suite's `docs/simulator-differences.md` too.

### 2.5 specparam / program signal / checker delay (3 cases, #10)

- `ch11/test_A_extra_specparam_range`: stop using `localparam = sp[...]` (6.20.5).
- `ch17/test_checker_declaration_scope_kinds`: do not read `p.hit` from a module;
  use a program port or task (24.3).
- `ch23/test_scope_creators_all_kinds`: move checker `initial #9 $display` to `always_ff` or `final` (17.5).

### 2.6 Syntax outside the grammar (6 cases, #15)

- `ch11/test_conditional_predicate_and_integral_branch`: remove outer parentheses around `(x matches ...)`.
- `ch12/test_pattern_conditional_predicate`: same correction.
- `ch12/test_pattern_match_known_result`: use `matches` in a predicate or `case ... matches`.
- `ch19/test_cg_bin_syntax_all_forms`: use `wildcard bins wc = {...}`.
- `ch23/test_syntax_port_expression_and_explicit_ports`: rename port `cell`.
- `ch11/test_tagged_union_expression_and_matches_precedence`: move `$display`'s `matches` into a predicate.

### 2.7 Seven cases from §4.3 (#16)

| Test | Proposed correction |
|---|---|
| `ch16/test_nexttime_always_eventually_alignment` | Do not combine different clocks with `and`; use `##1`/`##0` |
| `ch16/test_triggered_matched_multiclock` | Replace `##[1:2]` with `##1`/`##0` |
| `ch18/test_uniqueness_constraint_forms` | Make `a,b,c` `rand` |
| `ch22/test_22_extra_include_syntax` | Do not search angle includes through `:incdirs:`; use quoted include |
| `ch25/test_interface_extern_and_generate_items` | Remove the module instance inside the interface |
| `ch25/test_interface_abstract_methods_and_broadcast` | Add `value` to modport `caller`, or stop using `b.value` |
| `ch5/test_attribute_prefix_positions` | Place `wire ub;` before the instance |

### 2.8 Six cases from §4.3 (#17)

| Test | Proposed correction |
|---|---|
| `ch16/test_ch16_sim_checker_history` | Use `$future_gclk` inside a property/sequence |
| `ch16/test_sequence_formal_arguments_substitution` | Add `local` to `input int z` |
| `ch16/test_sequence_local_formal_assignment` | Pass a local variable to `s_add` |
| `ch25/test_virtual_interface_data_type_forms` | Do not use virtual interfaces as ports (25.9) |
| `ch6/test_void_return_and_tagged_union_void_member` | Replace `out` with `output` |
| `ch7/test_union_grammar_forms` | Move `shortreal` to an unpacked union |

### 2.9 Real arguments to array queries (3 cases, #18)

`real`, `shortreal` and `event` are outside 20.7's domain (arrays, integral types and string).
Remove those calls from the three cases. Also remove the real row of `test_array_query_types_and_invalid`;
its string and out-of-range-dimension rows remain in slang #75.

### 2.10 Empty-matching property (1 case, #19)

`1'b1[*]` in `ch16/test_A_extra_repetitions` includes an empty match and cannot be a property (16.12.22).
Remove the `a_any` line or use `[*1]`.

---

## 3. The one remaining likely case

Only `ch17/test_checker_always_statement_kinds` (#6) remains `likely`.
It places `int acc;` inside always_comb. Declarations are absent from 17.5's permitted list,
so a test defect is likely, but a block declaration can also be read as not being a “statement”.
**Moving the declaration to the checker body** is legal under either interpretation and is the safe first rewrite.

---

## 4. Verification

```sh
python3 tools/run_kirimasim.py \
  --kirimasim <kirimasim>/build/release/src/driver/kirimasim \
  -j 8 --json /tmp/kirimasim-nonslang.json \
  tests/ch33/test_config_*.sv tests/ch23/test_defparam_*.sv \
  tests/ch11/test_*precedence*.sv tests/ch16/test_deferred_*.sv
```

Compare `pass` / `fail` before and after fixes and check for regressions.
