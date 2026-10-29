# kirimasim 失敗 176 本トリアージ台帳

作成: 2026-10-28 / 対象: `main` `a2420294` / スイート: kirimasim-tests `b83d0ffd`
元資料: [失敗の再解析](kirimasim_failure_analysis_2026-10-28.md)

この台帳は、fail 176 本の 1 本ずつについて「誰の責任か」「どの Issue か」
「根拠の強度」を記録したものです。CSV 版は
[`kirimasim_failure_triage_2026-10-28.csv`](kirimasim_failure_triage_2026-10-28.csv)
にあります。

> AI による分類です。`needs-review` は、規格本文を人が確認するまで
> 「slang のバグ」と決めないでください。

## 1. 集計

| 責任 | confirmed | likely | needs-review | 合計 |
|---|---:|---:|---:|---:|
| テスト側 | 39 | 1 | 0 | 40 |
| kirimasim | 25 | 0 | 0 | 25 |
| slang | 104 | 7 | 0 | 111 |
| **合計** | **168** | **8** | **0** | **176** |

- **confirmed**: 規格の明文、Icarus の拒否、またはスイートのレビューで根拠がある。
- **likely**: 根因はほぼ確定だが、修正後の再実行または条文の最終確認が残る。
- **needs-review**: まだ「slang のバグ」と「テストの誤り」を決めていない。

## 2. 確定した失敗（168 本）

### 2.1 テスト側（39 本）

| Test | Kind | Issue | Confidence | Clause | Note |
|---|---|---|---|---|---|
| `ch11/test_A_extra_specparam_range` | compile | #10 | confirmed | 1800-2023 11.2.1 6.20.5 A.8.4 | 6.20.5: specparam cannot initialize a localparam |
| `ch11/test_conditional_predicate_and_integral_branch` | compile | #15 | confirmed | 1800-2023 11.4.11 | 12.6.3: a matches clause is a cond_pattern, not a parenthesized expression |
| `ch11/test_tagged_union_expression_and_matches_precedence` | compile | #15 | confirmed | 1800-2023 11.9 7.3.2 A.8.3 A.10 | 12.6: matches is only allowed in a predicate or case ... matches |
| `ch12/test_pattern_conditional_predicate` | compile | #15 | confirmed | 1800-2023 12.6.3 12.6.2 11.4.11 | 12.6.3: a matches clause is a cond_pattern, not a parenthesized expression |
| `ch12/test_pattern_match_known_result` | compile | #15 | confirmed | 1800-2023 12.6 12.6.1 | 12.6: matches is only allowed in a predicate or case ... matches |
| `ch16/test_A_extra_repetitions` | compile | #19 | confirmed | 1800-2023 16.9.2 A.2.10 | 16.12.22: a sequence used as a property shall not admit an empty match |
| `ch16/test_ch16_sim_checker_history` | compile | #17 | confirmed | 1800-2023 16.5.1 17.7 16.9.4 | 16.9.4: $future_gclk is only allowed in a sequence or property expression |
| `ch16/test_ch16_sim_clocking_single` | diagnostic | #8 | confirmed | 1800-2023 16.16 16.16.1 | 16.16: the explicit-clock and multiclock rules both apply; regex is too narrow |
| `ch16/test_nexttime_always_eventually_alignment` | compile | #16 | confirmed | 1800-2023 16.12.10 16.12.11 16.12.13 | 16.13.1: only ##1 and ##0 may combine differently clocked operands |
| `ch16/test_sequence_formal_arguments_substitution` | compile | #17 | confirmed | 1800-2023 16.8 16.8.1 16.12.18 16.12.19 | 16.8.1: a formal direction requires the local keyword |
| `ch16/test_sequence_local_formal_assignment` | compile | #17 | confirmed | 1800-2023 16.8.1 | 16.8.1: the actual of an inout/output local formal must be a local variable |
| `ch16/test_triggered_matched_multiclock` | compile | #16 | confirmed | 1800-2023 16.13.5 16.13.6 16.13.1 16.7 16.14.3 | 16.13.1: only ##1 and ##0 may combine a multiclocked operand |
| `ch17/test_checker_assertion_placement` | compile | #7 | confirmed | 1800-2023 17.3 17.5 16.14.6 16.4 | 16.4.1: a deferred assertion action must be a subroutine call |
| `ch17/test_checker_body_items` | compile | #7 | confirmed | 1800-2023 17.2 17.5 17.3 17.7 17.1 | 16.4.1: a deferred assertion action must be a subroutine call |
| `ch17/test_checker_body_procedural_and_generate_items` | compile | #6 | confirmed | 1800-2023 17.5 17.7 A.1.8 | 17.5/17.7.1: initial may contain only let, assertions and an event control |
| `ch17/test_checker_declaration_and_port_forms` | compile | #6 | confirmed | 1800-2023 17.2 A.1.2 A.1.8 | 17.5/17.7.1: initial may contain only let, assertions and an event control |
| `ch17/test_checker_declaration_scope_kinds` | compile | #10 | confirmed | 1800-2023 17.2 | 24.3: a program signal cannot be read from outside a program |
| `ch17/test_checker_procedural_deferred_assertion` | compile | #7 | confirmed | 1800-2023 17.3 16.4.1 16.14.6.1 | 16.4.1: a deferred assertion action must be a subroutine call |
| `ch17/test_checker_rand_and_nested_checker` | compile | #6 | confirmed | 1800-2023 17.2 17.7 A.1.8 | 17.7.1: a checker variable cannot be assigned in an initial procedure |
| `ch18/test_uniqueness_constraint_forms` | compile | #16 | confirmed | 1800-2023 18.5.4 18.5 A.10 | 18.5.4: the range_list names variables, but this test needs rand variables to be satisfiable |
| `ch19/test_cg_bin_syntax_all_forms` | compile | #15 | confirmed | 1800-2023 19.5 19.5.1 19.5.2 19.5.3 19.5.4 19.11.1 | 19.5: wildcard precedes bins_keyword, not = |
| `ch20/test_array_query_dimension_counts` | compile | #18 | confirmed | 1800-2023 20.7 | 20.7: real, shortreal and event are outside the array/integral/string domain |
| `ch20/test_array_query_functions` | compile | #18 | confirmed | 1800-2023 20.7 | 20.7: real, shortreal and event are outside the array/integral/string domain |
| `ch20/test_array_query_invalid_dimension_all_x` | compile | #18 | confirmed | 1800-2023 20.7 21.2.1.3 | 20.7: real, shortreal and event are outside the array/integral/string domain |
| `ch20/test_fatal_form_finish_number_and_arguments` | diagnostic | #8 | confirmed | 1800-2023 20.10 20.2 A.1.4 | 20.10: message format is tool-defined; regex must accept either order |
| `ch20/test_fatal_form_finish_number_only` | diagnostic | #8 | confirmed | 1800-2023 20.10 20.2 A.1.4 | 20.10: message format is tool-defined; regex must accept either order |
| `ch20/test_fatal_form_without_parentheses` | diagnostic | #8 | confirmed | 1800-2023 20.10 20.2 A.1.4 | 20.10: message format is tool-defined; regex must accept either order |
| `ch20/test_severity_tasks_report_contents` | diagnostic | #8 | confirmed | 1800-2023 20.10 22.13 | 20.10: message format is tool-defined; regex must accept either order |
| `ch21/test_readmemh_address_range` | runtime | #9 | confirmed | 1800-2023 21.4.3 21.4.1 7.4.4 | 21.4/21.4.3: start/finish address the highest dimension, so 2..5 is out of range |
| `ch22/test_22_extra_include_syntax` | compile | #16 | confirmed | 1800-2023 22.4 22.5.1 | 22.4: angle-bracket include searches only the standard location |
| `ch23/test_scope_creators_all_kinds` | compile | #10 | confirmed | 1800-2023 23.9 | 17.5: a checker initial may not contain a delay or $display |
| `ch23/test_syntax_port_expression_and_explicit_ports` | compile | #15 | confirmed | 1800-2023 23.2.2.1 23.2.2.2 23.2.2.3 A.1.3 | Annex B: cell is a reserved keyword and cannot be a port identifier |
| `ch25/test_interface_abstract_methods_and_broadcast` | compile | #16 | confirmed | 1800-2023 25.2 25.7.4 | 25.5: a modport restricts access to the names it lists; value is not in caller |
| `ch25/test_interface_extern_and_generate_items` | compile | #16 | confirmed | 1800-2023 25.7 25.3 A.1.6 A.2.9 | 25.3: a module cannot be instantiated in an interface |
| `ch25/test_virtual_interface_data_type_forms` | compile | #17 | confirmed | 1800-2023 25.9 25.9.2 A.2.2.1 | 25.9: a virtual interface shall not be used as a port |
| `ch5/test_attribute_prefix_positions` | compile | #16 | confirmed | 1800-2023 5.12 | 6.10: the explicit net declaration after the use is a redeclaration; declare it before the instance |
| `ch6/test_design_element_static_lifetime` | compile | #6 | confirmed | 1800-2023 6.21 | 17.5/17.7.1: checker initial assignment and plain always |
| `ch6/test_void_return_and_tagged_union_void_member` | compile | #17 | confirmed | 1800-2023 6.13 7.3.2 11.9 | 6.13: the direction keyword is output, not out |
| `ch7/test_union_grammar_forms` | compile | #17 | confirmed | 1800-2023 7.3 | 7.3.1: a soft packed union still needs integral members |

### 2.2 kirimasim（25 本）

| Test | Kind | Issue | Confidence | Clause | Note |
|---|---|---|---|---|---|
| `ch11/test_conditional_real_shortreal_branch_promotion` | runtime | #61 | confirmed | 1800-2023 11.4.11 6.12 | shortreal result is not rounded to single precision |
| `ch11/test_expr_conditional_ambiguous` | runtime | #60 | confirmed | 1800-2023 11.4.11 | x/z predicate does not combine unequal aggregates element by element |
| `ch11/test_precedence_distinguishing_values` | runtime | #54 | confirmed | 1800-2023 11.3.2 11.4.3 | existing issue: ?: must bind above -> |
| `ch11/test_precedence_logical_conditional_implication_levels` | runtime | #54 | confirmed | 1800-2023 11.3.2 11.4.7 11.4.11 | existing issue: ?: must bind above -> |
| `ch12/test_syntax_loop_statement_forms` | runtime | #65 | confirmed | 1800-2023 12.7 12.7.1 12.7.2 12.7.3 12.7.4 12.7.5 12.7.6 A.6.8 A.6.4 | foreach with every loop variable omitted iterates once |
| `ch13/test_constant_function_call_fresh_initial_values` | runtime | #68 | confirmed | 1800-2023 13.4.3 | static local initializer is ignored in a constant call |
| `ch16/test_deferred_flush_points` | runtime | #62 | confirmed | 1800-2023 16.4.2 16.4.4 16.4.1 | deferred assertion flush points are missing |
| `ch16/test_syntax_deferred_immediate_assertions` | runtime | #62 | confirmed | 1800-2023 16.3 16.4 A.6.10 A.6.4 | deferred immediate assertion scheduling |
| `ch20/test_illegal_timeformat_unit_out_of_range` | diagnostic | #70 | confirmed | 1800-2023 20.4.3 | out-of-range unit becomes an internal error with exit 3 |
| `ch21/test_vcd_scope_commands_begin_fork_task` | runtime | #69 | confirmed | 1800-2023 21.7.2.3 21.7.2.1 | VCD $scope omits a named fork block |
| `ch23/test_defparam_last_in_source_text_wins` | runtime | #59 | confirmed | 1800-2023 23.10.1 | defparam order and defparam-vs-instance precedence |
| `ch23/test_parameter_override_type_and_range` | runtime | #59 | confirmed | 1800-2023 23.10 6.20.2 6.12.1 | defparam order and defparam-vs-instance precedence |
| `ch24/test_program_port_reactive_variable_and_net` | runtime | #63 | confirmed | 1800-2023 24.3.2 24.3.1 4.5 23.3.3 | program port net is not updated before the program resumes |
| `ch33/test_config_binding_descends_through_children` | runtime | #58 | confirmed | 1800-2023 33.2 | chooseConfigTop skips config selection when --top is given |
| `ch33/test_config_cell_parameter` | runtime | #58 | confirmed | 1800-2023 33.4.3 33.4.1.4 33.4.1.6 | chooseConfigTop skips config selection when --top is given |
| `ch33/test_config_hierarchical` | compile | #58 | confirmed | 1800-2023 33.4.2 33.4.1.3 33.4.1.6 33.4.1.1 33.4.1.5 33.3.1.1 | the same chooseConfigTop rejects a second config; the test comments name the config the top module selects |
| `ch33/test_config_instance_over_cell` | runtime | #58 | confirmed | 1800-2023 33.4.1.3 33.4.1.4 33.4.1.6 | chooseConfigTop skips config selection when --top is given |
| `ch33/test_config_library_cell_and_topmodule_names` | runtime | #58 | confirmed | 1800-2023 33.4.1 33.4.1.1 33.4.1.4 33.7 A.9.3 | chooseConfigTop skips config selection when --top is given |
| `ch33/test_config_name_disambiguation_config_suffix` | compile | #58 | confirmed | 1800-2023 33.2.1 33.4.1.6 33.4.2 | the same chooseConfigTop rejects a second config; the test comments name the config the top module selects |
| `ch33/test_config_rules_bind_each_instance` | runtime | #58 | confirmed | 1800-2023 33.2 | chooseConfigTop skips config selection when --top is given |
| `ch33/test_config_use_config_suffix` | compile | #58 | confirmed | 1800-2023 33.4.1.6 33.4.1.1 33.4.2 33.4.1.3 | the same chooseConfigTop rejects a second config; the test comments name the config the top module selects |
| `ch4/test_reactive_region_set_order` | runtime | #63 | confirmed | 1800-2023 4.4.2.7 4.4.2.8 4.5 24.3 | program runs before the module Active-region update |
| `ch7/test_queue_invalid_index_write_ignored_warning` | diagnostic | #67 | confirmed | 1800-2023 7.10.1 | 7.10.1 requires a warning for an invalid queue write |
| `ch7/test_queue_method_prototypes_and_invalid_indices` | runtime | #66 | confirmed | 1800-2023 7.10.2.2 7.10.2.1 7.10.2.3 7.10.2.4 7.10.2.5 7.10.2.6 7.10.2.7 | queue index is not converted to integer |
| `ch9/test_always_comb_longest_static_prefix_and_locals` | runtime | #64 | confirmed | 1800-2023 9.2.2.2.1 9.2.2.2 11.5.3 | constant bit-select sensitivity is not narrowed |

### 2.3 slang（104 本）

| Test | Kind | Issue | Confidence | Clause | Note |
|---|---|---|---|---|---|
| `ch10/test_illegal_force_mixed_driven_variable` | illegal | #82 | confirmed | 1800-2023 10.6.2 | 10.6.2: force is illegal on a variable with mixed continuous and procedural drivers |
| `ch10/test_syntax_assignment_pattern_forms` | compile | #83 | confirmed | 1800-2023 10.9 10.9.1 10.9.2 A.6.7.1 | 10.9.2/A.6.7.1: an assignment pattern may be an lvalue |
| `ch11/test_concatenation_pattern_string_operand_and_replication_once` | compile | #83 | confirmed | 1800-2023 11.4.12 11.4.12.1 11.4.12.2 6.16 | the suite review checked the clause and the test is right |
| `ch11/test_illegal_unsized_constant_in_concatenation` | illegal | #82 | confirmed | 1800-2023 11.4.12 | Icarus rejects at the expected place |
| `ch11/test_stream_item_order_struct_union_object_assoc` | compile | #83 | confirmed | 1800-2023 11.4.14.1 | the suite review checked the clause and the test is right |
| `ch11/test_stream_unpack_consumes_left_converts_state_keeps_objects` | compile | #83 | confirmed | 1800-2023 11.4.14.3 | the suite review checked the clause and the test is right |
| `ch12/test_illegal_for_type_reference_without_var` | illegal | #82 | confirmed | 1800-2023 12.7 6.23 | Icarus rejects at the expected place |
| `ch12/test_statement_item_forms` | compile | #90 | confirmed | 1800-2023 12.3 12.2 | 13.5.2: a ref actual may be a variable; the int variable is legal |
| `ch13/test_A_extra_tf_dims` | compile | #83 | confirmed | 1800-2023 13.3 A.2.3 | A.2.6: a tf port item may have an initializer |
| `ch13/test_illegal_constant_function_call_in_constant_context` | illegal | #82 | confirmed | 1800-2023 13.4.3 | 13.4.3: a constant function may not call a constant function in a constant-expression context |
| `ch16/test_assume_property_actions` | compile | #78 | confirmed | 1800-2023 16.14.2 16.14.4 20.11 20.10 16.12 | 16.3/A.6.3: a null pass statement before else is a legal action_block |
| `ch16/test_ch16_sim_checker_inference` | compile | #77 | confirmed | 1800-2023 16.13.6 16.16 | 17.3, 17.6 and 17.7.1 allow these checker forms |
| `ch16/test_concurrent_assert_actions` | compile | #78 | confirmed | 1800-2023 16.14.1 16.3 20.10 20.11 4.4.2 16.12 | 16.3/A.6.3: a null pass statement before else is a legal action_block |
| `ch16/test_illegal_deferred_assertion_const_ref_dynamic_actual` | illegal | #82 | confirmed | 1800-2023 16.4 | 16.4: a dynamic variable cannot be a ref or const ref actual of a deferred assertion action |
| `ch16/test_named_sequence_scopes_and_instances` | compile | #78 | confirmed | 1800-2023 16.8 | 16.8: the final semicolon of a sequence declaration is optional |
| `ch16/test_recursive_property_time_advance` | compile | #86 | confirmed | 1800-2023 16.12 16.12.17 | 16.12.17 restriction 3: the recursive instance is after ##1 |
| `ch17/test_checker_body_declaration_kinds` | compile | #77 | confirmed | 1800-2023 17.2 17.7 A.1.8 | 17.3, 17.6 and 17.7.1 allow these checker forms |
| `ch17/test_checker_covergroup_sample` | compile | #77 | confirmed | 1800-2023 17.6 19.8.1 19.11.1 | 17.3, 17.6 and 17.7.1 allow these checker forms |
| `ch17/test_checker_instantiation_connection_forms` | compile | #77 | confirmed | 1800-2023 17.3 17.2 A.4.1.4 | 17.3, 17.6 and 17.7.1 allow these checker forms |
| `ch17/test_checker_procedures_reactive_renba` | compile | #77 | confirmed | 1800-2023 17.7.3 24.3.1 | 17.3, 17.6 and 17.7.1 allow these checker forms |
| `ch18/test_inline_local_scope_binding` | compile | #93 | confirmed | 1800-2023 18.7.1 | 18.7.1: local::obj binds to the receiver class scope |
| `ch18/test_rand_mode_inactive_handle` | compile | #83 | confirmed | 1800-2023 18.8 18.3 18.5.8 | 18.8: rand_mode() without an argument returns the current state |
| `ch19/test_cg_inheritance_args_and_cross` | compile | #95 | confirmed | 1800-2023 19.4.1 19.4 19.7 19.11 19.11.1 19.11.2 19.8.1 | 19.4.1: a cross with the same name in the derived covergroup replaces the base cross |
| `ch19/test_covergroup_cross_and_select_forms` | compile | #96 | confirmed | 1800-2023 19.6 19.6.1 19.6.1.2 19.6.1.3 19.6.1.4 19.11.2 A.2.11 19.2 | 19.6.1: binsof takes a cover point bin; ca.v[0] is such a bin |
| `ch19/test_illegal_cg_function_side_effect` | illegal | #81 | confirmed | 1800-2023 19.5 | Icarus rejects at the expected place |
| `ch19/test_illegal_cg_outer_constant` | illegal | #81 | confirmed | 1800-2023 19.5 | Icarus rejects at the expected place |
| `ch19/test_illegal_cg_ref_formal_expression` | illegal | #81 | confirmed | 1800-2023 19.5 | Icarus rejects at the expected place |
| `ch19/test_illegal_cg_ref_formal_variable` | illegal | #81 | confirmed | 1800-2023 19.5 | Icarus rejects at the expected place |
| `ch20/test_array_query_default_dimension_integer_result` | compile | #75 | confirmed | 1800-2023 20.7 | 20.7 defines 0 or x for these arguments; Icarus and Verilator accept real |
| `ch20/test_array_query_types_and_invalid` | compile | #75 | confirmed | 1800-2023 20.7 | 20.7 defines 0 or x for these arguments; Icarus and Verilator accept real |
| `ch20/test_illegal_bits_function_dynamic_return` | illegal | #82 | confirmed | 1800-2023 20.6.2 | 20.6.2: $bits of a function returning a dynamically sized type is an error |
| `ch20/test_illegal_dist_exponential_zero_mean` | illegal | #82 | confirmed | 1800-2023 20.14.2 | 20.14.2: the mean of $dist_exponential shall be greater than 0 |
| `ch20/test_illegal_isunbounded_variable` | illegal | #82 | confirmed | 1800-2023 20.6.3 | 20.6.3: the argument of $isunbounded shall be a parameter name |
| `ch21/test_ch21_extra_pattern_unpacked_struct` | compile | #83 | confirmed | 1800-2023 21.2.1.6 | the suite review checked the clause and the test is right |
| `ch21/test_evcd_dumpports_files_defaults_and_control_scope` | compile | #74 | confirmed | 1800-2023 21.7.3.1 21.7.3.2 21.7.3.3 21.7.3.5 21.7.3.7 21.7.4.3 21.7.4.3.1 | 21.3.1 and 21.6 accept an integral value containing a string |
| `ch21/test_evcd_dumpportslimit_named_by_variables` | compile | #74 | confirmed | 1800-2023 21.7.3.4 21.7.3.1 | 21.3.1 and 21.6 accept an integral value containing a string |
| `ch21/test_fopen_name_and_type_forms` | compile | #74 | confirmed | 1800-2023 21.3.1 21.3.2 21.3.4.2 | 21.3.1 and 21.6 accept an integral value containing a string |
| `ch21/test_fread_unpacked_struct_union` | compile | #83 | confirmed | 1800-2023 21.3.4.4 | 21.3.4.4: $fread into an unpacked struct or union is defined |
| `ch21/test_plusargs_key_from_variable` | compile | #74 | confirmed | 1800-2023 21.6 | 21.3.1 and 21.6 accept an integral value containing a string |
| `ch21/test_vcd_dumpvars_all_filename_start` | compile | #74 | confirmed | 1800-2023 21.7.1.1 21.7.1.2 21.7.1.3 | 21.3.1 and 21.6 accept an integral value containing a string |
| `ch22/test_ifdef_directive_names_not_defined` | compile | #78 | confirmed | 1800-2023 22.6 | 22.6: directive names count as not defined in ifdef and elsif |
| `ch22/test_illegal_begin_keywords_without_end` | illegal | #79 | confirmed | 1800-2023 22.14 | 22.14: begin_keywords and end_keywords shall be used as a pair |
| `ch22/test_illegal_directive_split_across_lines` | illegal | #79 | confirmed | 1800-2023 22.2 22.7 | 22.2: a directive is all on one line unless the section says it can span lines |
| `ch22/test_illegal_include_absolute_path_in_angle_brackets` | illegal | #79 | confirmed | 1800-2023 22.4 | Icarus rejects at the expected place |
| `ch22/test_illegal_include_line_trailing_code` | illegal | #79 | confirmed | 1800-2023 22.4 | Icarus rejects at the expected place |
| `ch22/test_illegal_line_directive_trailing_comment` | illegal | #79 | confirmed | 1800-2023 22.12 | 22.12: a comment is not allowed on a line directive line |
| `ch23/test_23_extra_upward_data_stop` | compile | #83 | confirmed | 1800-2023 23.8 | 23.8: an upward reference stops at a structure object |
| `ch23/test_23_extra_uwire_dominates` | compile | #71 | confirmed | 1800-2023 23.3.3.7 28.16 6.6.2 6.6.1 | 6.6.2 does not forbid an inout uwire; Icarus and Verilator accept it |
| `ch23/test_23_extra_uwire_warn` | compile | #71 | confirmed | 1800-2023 23.3.3.7 28.16 6.6.2 6.6.3 | 6.6.2 does not forbid an inout uwire; Icarus and Verilator accept it |
| `ch23/test_defparam_block_task_function_parameters` | compile | #76 | confirmed | 1800-2023 23.10.2 23.10.1 23.10.3 | 23.10.1 and 23.10.2 allow program, block, task and function parameters |
| `ch23/test_defparam_targets_module_interface_program` | compile | #76 | confirmed | 1800-2023 23.10.1 23.6 24.3 25.3 | 23.10.1 and 23.10.2 allow program, block, task and function parameters |
| `ch23/test_illegal_defparam_array_sibling_instance` | illegal | #79 | confirmed | 1800-2023 23.10.1 23.3.2 | 23.10.1: a defparam in one array element cannot target another element |
| `ch23/test_illegal_no_top_level_module` | illegal | #79 | confirmed | 1800-2023 23.3.1 | 23.3.1: a design shall contain at least one top-level module |
| `ch23/test_illegal_scope_duplicate_in_unselected_generate` | illegal | #79 | confirmed | 1800-2023 23.9 27.5 | Icarus rejects at the expected place |
| `ch23/test_interconnect_primitive_broadcast_input` | compile | #72 | confirmed | 1800-2023 23.3.3.7.2 6.6.8 | 6.6.8 and 23.3.3.7.2 allow terminal and net_alias uses |
| `ch23/test_interconnect_primitive_terminal` | compile | #72 | confirmed | 1800-2023 23.3.3.7.2 6.6.8 23.3.3.7.1 | 6.6.8 and 23.3.3.7.2 allow terminal and net_alias uses |
| `ch23/test_interconnect_terminal_wire_resolution` | compile | #72 | confirmed | 1800-2023 23.3.3.7.2 6.6.8 6.6.1 29.3 | 6.6.8 and 23.3.3.7.2 allow terminal and net_alias uses |
| `ch23/test_port_collapse_trireg_and_uwire_cells` | compile | #71 | confirmed | 1800-2023 23.3.3.7 | 6.6.2 does not forbid an inout uwire; Icarus and Verilator accept it |
| `ch23/test_port_collapse_uwire_external_delay` | compile | #71 | confirmed | 1800-2023 23.3.3.7 28.16 6.6.1 6.6.2 | 6.6.2 does not forbid an inout uwire; Icarus and Verilator accept it |
| `ch23/test_port_collapse_uwire_supply` | compile | #71 | confirmed | 1800-2023 23.3.3.7 6.6.2 6.6.6 | 6.6.2 does not forbid an inout uwire; Icarus and Verilator accept it |
| `ch23/test_port_collapse_wand_uwire_warning` | compile | #71 | confirmed | 1800-2023 23.3.3.7 28.16 6.6.3 6.6.2 | 6.6.2 does not forbid an inout uwire; Icarus and Verilator accept it |
| `ch23/test_port_collapse_wor_uwire_warning` | compile | #71 | confirmed | 1800-2023 23.3.3.7 28.16 6.6.3 6.6.2 | 6.6.2 does not forbid an inout uwire; Icarus and Verilator accept it |
| `ch24/test_illegal_anonymous_program_hierarchical_reference` | illegal | #79 | confirmed | 1800-2023 24.3 24.6 | 24.3: an anonymous program cannot reference another program scope hierarchically |
| `ch25/test_illegal_generic_interface_port_dot_name` | illegal | #80 | confirmed | 1800-2023 25.3.3 | 25.3.3: a generic interface needs a named port, not an implicit .name connection |
| `ch25/test_illegal_interface_port_actual_through_generate_block` | illegal | #80 | confirmed | 1800-2023 25.3 | 25.3: an interface port actual shall not resolve through a generate block |
| `ch25/test_illegal_interface_port_actual_through_instance_array` | illegal | #80 | confirmed | 1800-2023 25.3 | 25.3: an interface port actual shall not resolve through an instance array |
| `ch25/test_illegal_modport_import_default_argument_without_prototype` | illegal | #80 | confirmed | 1800-2023 25.7 | 25.7: a default argument requires a full prototype in the modport import |
| `ch25/test_illegal_modport_import_named_binding_without_prototype` | illegal | #80 | confirmed | 1800-2023 25.7 | 25.7: binding by name requires a full prototype in the modport import |
| `ch25/test_illegal_modport_prototype_unnamed_named_binding` | illegal | #80 | confirmed | 1800-2023 25.7 | 25.7: binding by name requires named formals in the prototype |
| `ch26/test_illegal_package_imported_before_compiled` | illegal | #79 | confirmed | 1800-2023 26.3 | Icarus rejects at the expected place |
| `ch28/test_gate_delay_forms` | compile | #83 | confirmed | 1800-2023 28.3.3 28.16 28.4 A.2.2.3 | 28.16.1: gate primitives allow three delay values |
| `ch3/test_declaration_across_file_boundary` | compile | #78 | confirmed | 1800-2023 3.12.1 | 3.12.1: an incomplete declaration continues into the next file |
| `ch3/test_element_and_block_name_spaces` | compile | #83 | confirmed | 1800-2023 3.13 | 3.13: a specify block has its own name space |
| `ch33/test_config_declaration_grammar_forms` | compile | #78 | confirmed | 1800-2023 33.4.1 33.4.3 A.1.5 | 33.4.1: use cell named_parameter_assignment is legal |
| `ch33/test_duplicate_cell_last_definition_wins` | compile | #83 | confirmed | 1800-2023 33.3.1 33.3.1.1 33.3.3 | 33.3.1.1: the last cell with the same name is written to the library |
| `ch33/test_illegal_config_constant_function` | illegal | #79 | confirmed | 1800-2023 33.4.3 13.4.3 | Icarus rejects at the expected place |
| `ch33/test_illegal_config_generate_scope_path` | illegal | #79 | confirmed | 1800-2023 33.4.3 27.5 | Icarus rejects at the expected place |
| `ch33/test_illegal_config_hierarchical_name_in_expression` | illegal | #79 | confirmed | 1800-2023 33.4.3 | Icarus rejects at the expected place |
| `ch33/test_illegal_config_index_not_literal` | illegal | #79 | confirmed | 1800-2023 33.4.3 | Icarus rejects at the expected place |
| `ch33/test_illegal_config_instance_array_path` | illegal | #79 | confirmed | 1800-2023 33.4.3 23.3.3.5 | Icarus rejects at the expected place |
| `ch33/test_illegal_config_package_binding` | illegal | #79 | confirmed | 1800-2023 33.4 33.4.1.4 33.4.1.6 | Icarus rejects at the expected place |
| `ch5/test_illegal_method_implicit_variable_without_parentheses` | illegal | #82 | confirmed | 1800-2023 5.13 13.4.1 6.16 | 5.13: an implicit variable always needs the parentheses |
| `ch5/test_illegal_string_hex_escape_z_digit` | illegal | #82 | confirmed | 1800-2023 5.9.1 | 5.9.1: z cannot be a hex digit in a string escape |
| `ch5/test_illegal_string_octal_escape_z_digit` | illegal | #82 | confirmed | 1800-2023 5.9.1 | 5.9.1: z cannot be an octal digit in a string escape |
| `ch6/test_a2_1_3_interconnect_declaration_identifiers` | compile | #72 | confirmed | 1800-2023 6.6.8 23.3.3.7 A.2.1.3 | 6.6.8 and 23.3.3.7.2 allow terminal and net_alias uses |
| `ch6/test_illegal_initial_block_variable_initializer_no_keyword` | illegal | #82 | confirmed | 1800-2023 6.21 9.2.1 | 6.21: a static variable with an initializer needs the static keyword |
| `ch6/test_illegal_net_type_reg_in_port_declaration` | illegal | #82 | confirmed | 1800-2023 6.7.1 23.2.2.3 | 6.7.1 and 23.2.2.3: reg is not a net data type |
| `ch6/test_illegal_static_task_variable_initializer_no_keyword` | illegal | #82 | confirmed | 1800-2023 6.21 13.3 | 6.21: a static task variable with an initializer needs the static keyword |
| `ch6/test_illegal_type_operator_dynamic_array_element` | illegal | #82 | confirmed | 1800-2023 6.23 | Icarus rejects at the expected place |
| `ch6/test_interconnect_and_net_port_types` | compile | #72 | confirmed | 1800-2023 6.6.8 23.3.3.7 A.2.1.3 A.2.2.1 | 6.6.8 and 23.3.3.7.2 allow terminal and net_alias uses |
| `ch6/test_nettype_alias_resolution_and_empty_drivers` | compile | #73 | confirmed | 1800-2023 6.7.2 6.7.3 6.22.6 | 6.6.7 and 23.3.3; the suite review checked the clauses and the tests are right |
| `ch6/test_nettype_declarations_and_user_nets` | compile | #73 | confirmed | 1800-2023 6.6.7 6.7.2 6.7.3 10.3.3 A.2.1.3 A.2.3 A.2.4 | 6.6.7 and 23.3.3; the suite review checked the clauses and the tests are right |
| `ch6/test_nettype_rename_matches` | compile | #73 | confirmed | 1800-2023 6.22.6 6.6.7 23.3.3 | 6.6.7 and 23.3.3; the suite review checked the clauses and the tests are right |
| `ch6/test_nettype_resolution` | compile | #73 | confirmed | 1800-2023 6.6.7 10.3 10.6.2 23.3.3 | 6.6.7 and 23.3.3; the suite review checked the clauses and the tests are right |
| `ch6/test_packed_union_size_and_implicit_conversion` | compile | #98 | confirmed | 1800-2023 6.24.1 6.19.4 | 6.24.3: an unpacked union of fixed-size members is a bit-stream type |
| `ch6/test_param_override_three_ways_module_interface_program` | compile | #76 | confirmed | 1800-2023 6.20 23.10.1 23.10.2.1 23.10.2.2 24.3 25.3 | 23.10.1 and 23.10.2 allow program, block, task and function parameters |
| `ch6/test_user_nettype_resolution_forms` | compile | #73 | confirmed | 1800-2023 6.6.7 6.6 6.7 6.7.2 6.7.3 13.8 | 6.6.7 and 23.3.3; the suite review checked the clauses and the tests are right |
| `ch7/test_array_index_and_size_signatures` | compile | #92 | confirmed | 1800-2023 7.4.5 7.5.2 | 7.4.5: an invalid index read returns Table 7-1 and a write does nothing |
| `ch7/test_assoc_narrow_prev_next_overflow` | compile | #83 | confirmed | 1800-2023 7.9.8 7.9.6 7.9.7 | the suite review checked the clause and the test is right |
| `ch7/test_assoc_traversal_narrow_index` | compile | #83 | confirmed | 1800-2023 7.9.8 7.9.4 7.9.5 | the suite review checked the clause and the test is right |
| `ch7/test_illegal_dynamic_new_nonblocking_assignment` | illegal | #82 | confirmed | 1800-2023 7.5.1 | Icarus rejects at the expected place |
| `ch7/test_illegal_union_holding_struct_member_default` | illegal | #82 | confirmed | 1800-2023 7.2.2 | 7.2.2: a member of an unpacked struct containing a union cannot have a default |
| `ch8/test_class_declaration_grammar_forms` | compile | #99 | confirmed | 1800-2023 8.3 8.17 8.20 8.24 8.26 | 8.26.2: a method declared in the implementing class may provide the implementation |
| `ch8/test_illegal_interface_typedef_scope_prefix_in_variable_declaration` | illegal | #80 | confirmed | 1800-2023 8.23 6.18 | Icarus rejects at the expected place |

## 3. 推定（8 本）

| Test | Kind | Issue | Confidence | Clause | Note |
|---|---|---|---|---|---|
| `ch17/test_checker_always_statement_kinds` | compile | #6 | likely | 1800-2023 17.5 | 17.5 does not list block item declarations; the for-loop declaration needs confirmation |
| `ch11/test_expr_timescale_constants` | compile | #89 | likely | 1800-2023 11.2.1 | 11.2.1 lists $timescale and $timeprecision; the omission of $timeunit may be editorial |
| `ch16/test_sampled_automatic_and_const_current` | compile | #87 | likely | 1800-2023 16.5.1 16.5.2 16.14.6.1 | 16.14.6 says procedural code; a task is procedural code, but the wording is indirect |
| `ch16/test_sequence_match_item_and_triggered_method` | compile | #88 | likely | 1800-2023 16.13.6 16.10 16.11 A.8.4 A.10 A.9.3 | 16.10 does not explicitly forbid a static variable target |
| `ch16/test_sequence_operator_and_delay_forms` | compile | #88 | likely | 1800-2023 16.9 16.10 16.12.2 16.12.15 16.14.1 A.2.10 | 16.10 does not explicitly forbid a static variable target |
| `ch18/test_rand_enum_solver_finds_solution` | compile | #94 | likely | 1800-2023 18.3 | the constraint-expression rules do not explicitly mention strings |
| `ch30/test_A_extra_module_path` | compile | #97 | likely | 1800-2023 30.4.4 11.2.1 A.7.2 A.8.3 A.8.4 | A.8.4 module path primaries include function calls and concatenations; the grammar was not confirmed |
| `ch5/test_string_literal_cast_justification` | compile | #91 | likely | 1800-2023 5.9 6.24.1 | 6.24.3 treats a string as a byte array for a bit-stream cast; the unpacked target needs confirmation |

## 4. 未トリアージ

なし。

## 5. 286 本の「`:expected_stdout:` なし」

176 本とは別枠です。詳細は [再解析 §0](kirimasim_failure_analysis_2026-10-28.md#286-本のexpected_stdout-なしの内訳) を参照。

| 種類 | 件数 | Issue |
|---|---:|---|
| `:type: formal`（シミュレーション対象外） | 268 | [kirimasim-tests #13](https://github.com/kurochan001/kirimasim-tests/issues/13) |
| `error: unsupported:` | 16 | [kirimasim-tests #14](https://github.com/kurochan001/kirimasim-tests/issues/14) |
| `:expected_stdout_verilator:` のみ | 1 | [kirimasim-tests #14](https://github.com/kurochan001/kirimasim-tests/issues/14)、[kirimasim #84](https://github.com/kurochan001/kirimasim/issues/84) |
| 出力も期待値もない | 1 | [kirimasim-tests #14](https://github.com/kurochan001/kirimasim-tests/issues/14) |

## 6. 更新のしかた

1. 行の `confidence` を `confirmed` に変える。
2. `note` に、確認した節番号と「テストが正しい / slang が厳しすぎる」の別を書く。
3. テスト側なら kirimasim-tests の Issue、slang 側なら kirimasim の `(slang)` Issue に、
   確認結果をコメントする。
4. CSV を更新したら、この Markdown を再生成する。

## 7. 注意

- `confirmed` は「修正が済んだ」意味ではありません。Issue が close するまで残ります。
- 2,225 本の unsupported はこの台帳の対象外です。別途、機能単位の台帳が必要です。
- 5,053 本の pass は再監査していません。期待値が誤っていても、kirimasim が
  それに一致すれば pass になります。
