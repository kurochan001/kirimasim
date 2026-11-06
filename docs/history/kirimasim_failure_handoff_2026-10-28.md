# kirimasim full-suite execution and failure analysis — handoff

[日本語](kirimasim_failure_handoff_2026-10-28.ja.md)

> Historical analysis. The subsequent [reanalysis](kirimasim_failure_analysis_2026-10-28.md) and
> [criteria audit](P1_CRITERIA_GAPS_2026-10-28.md) contain corrections. Original counts and proposals below are retained.

Created: 2026-10-28 / baseline: `main` `a2420294` (`kirimasim 0.0.0`).
Author: AI (DeepSeek V4.1 Flash). No independent human review yet.

---

## 0. Overview and next steps

- All **7,740** kirimasim-tests cases were run with the latest kirimasim: **5,053 match**, **2,225 unsupported**, **462 fail**.
- **286 failures lack `:expected_stdout:`** and are not simulator failures. **176 failures** need actual attention.
- Against the executable denominator (7,740 − 2,225 unsupported − 286 without expected output = **5,229**), the match rate is **96.63%**.
- The 176 comprise **104 compilation errors**, **44 accepted illegal designs**, **21 runtime output mismatches**, **7 diagnostic/exit-code mismatches**.
- Highest priority: **ch33 config (five runtime cases)**, **ch11 real/conditional operators (four)**,
  **ch16/ch17 checker/program/assertion (26 compilation cases)** and **ch23 uwire/interconnect (compilation)**.

---

## 1. Execution environment

| Item | Value |
|---|---|
| kirimasim | `main` `a2420294` (2026-10-28, `kirimasim 0.0.0`, Clang 20.1.8) |
| Build | `build/release/src/driver/kirimasim` |
| Suite | kirimasim-tests `main` (7,740 cases) |
| Execution | `tools/run_kirimasim.py`, 16 workers, about 38 seconds |
| Results | `/tmp/opencode/kirimasim-final.json`, `/tmp/opencode/kirimasim-final.log` |

Rerun:

```sh
python3 tools/run_kirimasim.py \
  --kirimasim <kirimasim>/build/release/src/driver/kirimasim \
  -j 16 --json /tmp/kirimasim.json
```

Runner treatment:

- Tests returning `error: unsupported:` are **unsupported**.
- Tests with `:dpi_c:` / `:vpi_c:` are **unsupported** because kirimasim cannot load C.
- Tests without `:expected_stdout:` become **fail (no :expected_stdout:)**, but are not simulator failures.
- Illegal-design tests pass when an error appears at the location required by `:expected_stderr_regex:`.

---

## 2. Results

| Status | Count | Meaning |
|---|---:|---|
| pass | **5,053** | Matches expected output |
| unsupported | **2,225** | Unimplemented in kirimasim, or C code |
| fail | **462** | Mismatch |
| Of these, no expected output | 286 | Test maintenance |
| **Actual failures** | **176** | Cases to address |

Match rate against the 5,229 executable denominator: **96.63%**.

### Failure breakdown (176 cases)

| Category | Count | Main meaning |
|---|---:|---|
| Compilation errors (exit 2) | **104** | Legal code rejected, or diagnostic differs from test expectation |
| Accepted illegal designs | **44** | Missing compile-time checks |
| Runtime output mismatches | **21** | Different values, order or times |
| Diagnostic text / exit code | **7** | `$fatal` text, severity, exit code |

### By chapter (176 actual failures)

| Chapter | Count | Main content |
|---|---:|---|
| ch23 | 20 | uwire/inout, interconnect, defparam, parameter types |
| ch16 | 18 | Checkers, sequences/properties, deferred assertions |
| ch33 | 16 | Config binding, parameters, library/cell |
| ch6 | 15 | Interconnect, types/declarations |
| ch20 | 13 | `$dimensions(real)`, `$fatal`, severity |
| ch11 | 12 | Real/conditional operators, concatenation, streams, specparam |
| ch17 | 12 | Checker body, program items, assertion placement |
| ch21 | 9 | readmemh, VCD scopes, plusargs strings |
| ch25 | 9 | Illegal interface/modport forms |
| ch7 | 8 | Queue boundaries, ref arguments |
| ch19 | 7 | Illegal covergroup forms |
| ch22 | 7 | Illegal preprocessor forms |
| Other | 30 | ch5/ch12/ch18/ch24/ch4/ch9/ch10/ch13, etc. |

### 2,225 unsupported cases (not failures)

| Chapter | Count | Main content |
|---|---:|---|
| ch37 | 227 | VPI |
| ch8 | 196 | Unimplemented class/interface features |
| ch16 | 184 | Unimplemented SVA features |
| ch7 | 158 | Unimplemented arrays, queues, locators |
| gen | 136 | Generated tests (strength `%v`, etc.) |
| ch18 | 119 | Unimplemented constraints/randomization |
| ch35 | 117 | DPI (cannot load C) |
| ch38 | 109 | VPI |
| ch19 | 103 | Covergroups |
| ch36 | 87 | VPI |

These are not yet implemented and are tracked separately from fail.
Explicitly unsupported features such as `%v` should not be counted as test failures.

---

## 3. Failure clusters and proposed fixes

### 3.1 Compilation errors (104 cases)

First use the cited clause and test body to decide whether legal code is being rejected or an illegal design gets
a different diagnostic. The most common families follow.

| Error (summary) | Count | Example | Decision point / proposal |
|---|---:|---|---|
| `statement is not allowed in 'initial' procedure inside a checker` | 4 | `ch17/test_checker_body_procedural_and_generate_items` | Is the statement permitted in a checker (17.7)? Fix frontend if legal |
| `'uwire' port 'w' cannot have direction 'inout'` | 4 | `ch23/test_23_extra_uwire_dominates` | Is uwire inout allowed (23.3.3)? Fix implementation if legal, test if illegal |
| `deferred assertion action must be a subroutine call` | 3 | `ch17/test_checker_assertion_placement` | Check legal action form under deferred-assertion restrictions (16.4) |
| `cannot reference program item from outside of a program` | 3 | `ch17/test_checker_declaration_scope_kinds` | Program-item visibility (24.3); fix if legal |
| `invalid argument type 'real' to system function` | 3 | `ch20/test_array_query_dimension_counts` | `$dimensions(real)` should return 0 (20.7); **bug candidate** |
| `cannot mix strings and integers in a concatenation` | 2 | `ch11/test_concatenation_pattern_string_operand_and_replication_once` | Implicit conversion (11.4.12/5.9); fix if legal |
| `expected ')'` / `expected member` / `expected ';'` / `expected identifier` / `expected '?'` | Present | `ch12/test_pattern_conditional_predicate`, etc. | Legal syntax missed by parser; **parser bug candidates** |
| `cannot access checker members hierarchically` | 2 | `ch16/test_ch16_sim_checker_inference` | Hierarchical checker-member reference (17.7); fix if legal |
| `sequence match items can only modify local variables` | 2 | `ch16/test_sequence_match_item_and_triggered_method` | Check legal match-item targets (16.10) |
| `no implicit conversion from 'bit[63:0]' to 'string'` | 2 | `ch21/test_plusargs_key_from_variable` | String requirement for plusargs key (21.6); check test legality |
| `cannot reference interconnect net 'ic' outside of a net port connection` | 2 | `ch6/test_a2_1_3_interconnect_declaration_identifiers` | Interconnect use restrictions (6.6.8); fix if legal |
| `argument ... connects to 'ref' port of inequivalent type` | 2 | `ch7/test_assoc_narrow_prev_next_overflow` | Ref type equivalence (13.5.2); rejection may be correct, check test |
| `stream expression type ... is not a bit-stream type` | 2 | `ch11/test_stream_item_order_struct_union_object_assoc` | Check tagged-union/class streaming (11.4.14) |
| `specparams cannot be used in general constant expressions` | 1 | `ch11/test_A_extra_specparam_range` | Check specparam in constant expressions (6.20.2) |
| `value of type ... is invalid for '%d'` | 1 | `ch11/test_tagged_union_expression_and_matches_precedence` | Check tagged-union `%d` formatting |
| `sequence must not admit an empty match` | 1 | `ch16/test_A_extra_repetitions` | Check empty-match restriction (16.12) and rejection |
| Other individual cases | Approximate | Assertion clocks, recursion, port directions, etc. | Compare each with the standard |

**Proposed approach:**

1. Determine whether the test is legal under the standard first. Fix kirimasim's frontend if legal; correct the test/expectation if illegal.
2. Group parser `expected ...` cases sharing syntax into one issue.
3. Split checker/program/assertion semantic checks (ch16/ch17) by feature.
4. For correct rejection with a differing diagnostic, align diagnostic text/exit codes rather than value expectations
   (or relax the test regex).

### 3.2 Accepted illegal designs (44 cases)

Missing compile-time checks. By chapter: **ch25 6, ch33 6, ch22 5, ch19 4, ch6 4, ch20 3, ch23 3, ch5 3, ch7 2**, etc.

Representative cases:

- ch25: `test_illegal_generic_interface_port_dot_name`, `test_illegal_interface_port_actual_through_generate_block`,
  `test_illegal_modport_import_default_argument_without_prototype`.
- ch33: config hierarchical names, indices, instance-array paths, etc.
- ch22: unclosed `begin_keywords`, absolute `include` paths, trailing comments on `line`.
- ch19: covergroup function side effects, outer constants, ref formals.
- ch6: interconnect declaration/use restrictions.
- ch20: dynamic `$bits` returns, zero mean in `$dist_exponential`, variable `$isunbounded`.

**Proposed approach:**

- One issue per syntax/semantic check; prioritize checks affecting multiple cases.
- Check that errors occur at the location specified by `:expected_stderr_regex:`.
- Distinguish accepting an unimplemented construct from forgetting validation. For unimplemented constructs,
  returning `error: unsupported:` to classify them as unsupported is preferable.

### 3.3 Runtime output mismatches (21 cases)

| Test | First difference (actual / expected) | Area |
|---|---|---|
| Five ch33 config cases | `u1=1` / `u1=5`, binding not descending to children, library/cell names, etc. | Config implementation |
| ch11 `test_conditional_real_shortreal_branch_promotion` | Precision of `16777217.0` | Real/shortreal promotion |
| ch11 `test_expr_conditional_ambiguous` | `arr=1,2` / `arr=1,0` | Conditional type determination |
| ch11 `test_precedence_*` | `... -> 0 = 0` / `= 1` | Conditional/implication precedence |
| ch16 `test_deferred_flush_points` | `latch fail` / `latch pass` | Deferred-assertion flush |
| ch16 `test_syntax_deferred_immediate_assertions` | `wrong` / `0 assert zero sees 5` | Deferred immediate assertions |
| ch21 `test_readmemh_address_range` | `m[0][2]=ee` / `=11` | readmemh address order |
| ch21 `test_vcd_scope_commands_begin_fork_task` | Zero fork scopes | VCD scopes |
| ch23 `test_defparam_last_in_source_text_wins` | `m.A=1` / `=2` | defparam precedence |
| ch23 `test_parameter_override_type_and_range` | Test of `R=3/4` | Parameter type/range |
| ch24 `test_program_port_reactive_variable_and_net` | `ov=1 iv=1 iw=0 mw=0` | Program ports |
| ch4 `test_reactive_region_set_order` | `program: v changed` / `module: ...` | Reactive region |
| ch7 `test_queue_method_prototypes_and_invalid_indices` | `insert(...): size=5` | Queue boundaries |
| ch9 `test_always_comb_longest_static_prefix_and_locals` | `y_vec=0` / expected value | always_comb sensitivity |
| ch12 `test_syntax_loop_statement_forms` | `sum=21 count=6` / expected value | foreach |
| ch13 `test_constant_function_call_fresh_initial_values` | `P1=1 P2=2` / `P1=11` | Constant-function initial values |

**Proposed approach:**

- **The five ch33 config cases likely share one cause.** Minimize binding, parameter and library/cell failures into one issue.
- ch11 real/conditional operators may share a fix in expression type determination (11.6/11.8); high priority.
- Runtime differences concern values: recalculate from standard expressions rather than adapting expectations to kirimasim.
- Separate ch21 readmemh and VCD scopes as independent file-I/O/waveform features.

### 3.4 Diagnostic text / exit codes (7 cases)

- `$fatal` messages/arguments (four ch20 cases): severity format and `finish_number`.
- `test_severity_tasks_report_contents` (ch20): severity content.
- `test_ch16_sim_clocking_single` (ch16): multiclock diagnostic position/text.
- `test_queue_invalid_index_write_ignored_warning` (ch7): warning presence.
- `test_illegal_timeformat_unit_out_of_range` (ch20): exit 3 / expected 2.

**Proposed approach:** compare text with test regexes, prioritizing location (file:line).
Check standard/test intent for exit codes; amend test `:expected_exit:` if necessary.

---

## 4. Priority

1. **21 runtime output cases** (functional errors): ch33 config 5 → ch11 real/conditional 4 →
   ch16 deferred 2 → ch21 readmemh/VCD 2 → ch23 defparam/parameter 2 → others.
2. **104 compilation errors** (rejected legal code): parser → checker/program/assertion (ch16/ch17) →
   uwire/interconnect/ref (ch23/ch6/ch7) → real array queries (ch20).
3. **44 accepted illegal designs:** one issue per syntax/semantic check, ch25/ch33/ch22 first.
4. **7 diagnostic/exit-code cases:** inexpensive work between functional fixes.
5. **2,225 unsupported cases:** known unimplemented functionality rather than failures;
   keep in `docs/` support tables and move to pass as implemented.

### Suggested chapter order

| Order | Chapter | Cases | Reason |
|---|---:|---:|---|
| 1 | ch33 | 16 | Five runtime config cases may share one cause |
| 2 | ch11 | 12 | Real/conditional expression type determination |
| 3 | ch16/ch17 | 30 | Checker/program/assertion feature group |
| 4 | ch23 | 20 | uwire/inout, interconnect, defparam |
| 5 | ch20 | 13 | `$dimensions(real)`, `$fatal`, severity |
| 6 | ch6/ch7/ch21 | 32 | Interconnect, queues, file I/O |
| 7 | Remaining | — | Individual/illegal cases |

---

## 5. Verification procedure

1. Rerun each family:

```sh
python3 tools/run_kirimasim.py \
  --kirimasim <kirimasim>/build/release/src/driver/kirimasim \
  -j 8 --json /tmp/kirimasim-family.json \
  tests/ch33/test_config_*.sv
```

2. Compare with previous JSON for improvements/regressions:

```sh
python3 - <<'PY'
import json
old=json.load(open('/tmp/opencode/kirimasim-final.json'))['tests']
new=json.load(open('/tmp/kirimasim-family.json'))['tests']
reg=[k for k in old if k in new and old[k]['status']=='pass' and new[k]['status']!='pass']
imp=[k for k in old if k in new and old[k]['status']!='pass' and new[k]['status']=='pass']
print('regressions',len(reg),'improvements',len(imp))
PY
```

3. Rerun the full suite to check regressions.
4. Record the fixed family, before/after counts and standard clauses in the commit message.

**Do not rewrite expectations to match kirimasim output.** For test corrections, use standard clauses/block IDs
and update the kirimasim-tests ledger. Keep unimplemented features separate as `known-limitation` / `unsupported`.

---

## 6. Artifacts and cautions

- Results: `/tmp/opencode/kirimasim-final.json` (status/reason only).
- Log: `/tmp/opencode/kirimasim-final.log`.
- This analysis is AI-authored; no independent human confirmation yet.
- The four failure categories are compilation errors, accepted illegal designs, runtime differences and diagnostics.
  Before fixes, verify each test's legality against the cited clause.
- The 286 `no :expected_stdout:` cases are not simulator failures; handle as separate test maintenance.
