# kirimasim 失敗 176 本トリアージ台帳

[English](kirimasim_failure_triage_2026-10-28.md)

作成: 2026-10-28 / 対象: `main` `a2420294` / スイート: kirimasim-tests `b83d0ffd`
元資料: [失敗の再解析](kirimasim_failure_analysis_2026-10-28.ja.md)

この台帳は、fail 176 本の 1 本ずつについて「誰の責任か」「どの Issue か」
「根拠の強度」を記録したものです。CSV 版は
[`kirimasim_failure_triage_2026-10-28.csv`](kirimasim_failure_triage_2026-10-28.csv)
にあります。[日本語 CSV](kirimasim_failure_triage_2026-10-28.ja.csv) は説明欄を翻訳したものです。

> AI による分類です。`needs-review` は、規格本文を人が確認するまで
> 「slang のバグ」と決めないでください。
> 過去の台帳です。現在の残件として使う前に、後続の[完了条件監査の訂正](P1_CRITERIA_GAPS_2026-10-28.ja.md)も確認してください。

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

| テスト | 種類 | Issue | 確度 | 節 | 注記 |
|---|---|---|---|---|---|
| `ch11/test_A_extra_specparam_range` | compile | #10 | confirmed | 1800-2023 11.2.1 6.20.5 A.8.4 | 6.20.5：specparam を localparam の初期値にできない |
| `ch11/test_conditional_predicate_and_integral_branch` | compile | #15 | confirmed | 1800-2023 11.4.11 | 12.6.3：matches 節は cond_pattern であり、括弧付きの式ではない |
| `ch11/test_tagged_union_expression_and_matches_precedence` | compile | #15 | confirmed | 1800-2023 11.9 7.3.2 A.8.3 A.10 | 12.6：matches は predicate または case ... matches の中でのみ使える |
| `ch12/test_pattern_conditional_predicate` | compile | #15 | confirmed | 1800-2023 12.6.3 12.6.2 11.4.11 | 12.6.3：matches 節は cond_pattern であり、括弧付きの式ではない |
| `ch12/test_pattern_match_known_result` | compile | #15 | confirmed | 1800-2023 12.6 12.6.1 | 12.6：matches は predicate または case ... matches の中でのみ使える |
| `ch16/test_A_extra_repetitions` | compile | #19 | confirmed | 1800-2023 16.9.2 A.2.10 | 16.12.22：property として使う sequence は空マッチを含んではいけない |
| `ch16/test_ch16_sim_checker_history` | compile | #17 | confirmed | 1800-2023 16.5.1 17.7 16.9.4 | 16.9.4：$future_gclk は sequence または property の式内でのみ使える |
| `ch16/test_ch16_sim_clocking_single` | diagnostic | #8 | confirmed | 1800-2023 16.16 16.16.1 | 16.16：明示クロックと multiclock の両規則が適用され、正規表現が狭すぎる |
| `ch16/test_nexttime_always_eventually_alignment` | compile | #16 | confirmed | 1800-2023 16.12.10 16.12.11 16.12.13 | 16.13.1：異なるクロックのオペランドを結合できるのは ##1 と ##0 だけ |
| `ch16/test_sequence_formal_arguments_substitution` | compile | #17 | confirmed | 1800-2023 16.8 16.8.1 16.12.18 16.12.19 | 16.8.1：仮引数の方向には local キーワードが必要 |
| `ch16/test_sequence_local_formal_assignment` | compile | #17 | confirmed | 1800-2023 16.8.1 | 16.8.1：inout/output local 仮引数の実引数はローカル変数が必要 |
| `ch16/test_triggered_matched_multiclock` | compile | #16 | confirmed | 1800-2023 16.13.5 16.13.6 16.13.1 16.7 16.14.3 | 16.13.1：multiclock のオペランドを結合できるのは ##1 と ##0 だけ |
| `ch17/test_checker_assertion_placement` | compile | #7 | confirmed | 1800-2023 17.3 17.5 16.14.6 16.4 | 16.4.1：遅延アサーションのアクションはサブルーチン呼び出しが必要 |
| `ch17/test_checker_body_items` | compile | #7 | confirmed | 1800-2023 17.2 17.5 17.3 17.7 17.1 | 16.4.1：遅延アサーションのアクションはサブルーチン呼び出しが必要 |
| `ch17/test_checker_body_procedural_and_generate_items` | compile | #6 | confirmed | 1800-2023 17.5 17.7 A.1.8 | 17.5/17.7.1：initial に置けるのは let・アサーション・イベント制御だけ |
| `ch17/test_checker_declaration_and_port_forms` | compile | #6 | confirmed | 1800-2023 17.2 A.1.2 A.1.8 | 17.5/17.7.1：initial に置けるのは let・アサーション・イベント制御だけ |
| `ch17/test_checker_declaration_scope_kinds` | compile | #10 | confirmed | 1800-2023 17.2 | 24.3：program の外から program 信号を読めない |
| `ch17/test_checker_procedural_deferred_assertion` | compile | #7 | confirmed | 1800-2023 17.3 16.4.1 16.14.6.1 | 16.4.1：遅延アサーションのアクションはサブルーチン呼び出しが必要 |
| `ch17/test_checker_rand_and_nested_checker` | compile | #6 | confirmed | 1800-2023 17.2 17.7 A.1.8 | 17.7.1：initial 手続きで checker 変数へ代入できない |
| `ch18/test_uniqueness_constraint_forms` | compile | #16 | confirmed | 1800-2023 18.5.4 18.5 A.10 | 18.5.4：range_list は変数を列挙するが、このテストを満たすには rand 変数が必要 |
| `ch19/test_cg_bin_syntax_all_forms` | compile | #15 | confirmed | 1800-2023 19.5 19.5.1 19.5.2 19.5.3 19.5.4 19.11.1 | 19.5：wildcard は = の前ではなく bins_keyword の前に置く |
| `ch20/test_array_query_dimension_counts` | compile | #18 | confirmed | 1800-2023 20.7 | 20.7：real・shortreal・event は配列・整数・string の対象範囲外 |
| `ch20/test_array_query_functions` | compile | #18 | confirmed | 1800-2023 20.7 | 20.7：real・shortreal・event は配列・整数・string の対象範囲外 |
| `ch20/test_array_query_invalid_dimension_all_x` | compile | #18 | confirmed | 1800-2023 20.7 21.2.1.3 | 20.7：real・shortreal・event は配列・整数・string の対象範囲外 |
| `ch20/test_fatal_form_finish_number_and_arguments` | diagnostic | #8 | confirmed | 1800-2023 20.10 20.2 A.1.4 | 20.10：メッセージ書式はツールが定めるため、正規表現はどちらの順序も受理すべき |
| `ch20/test_fatal_form_finish_number_only` | diagnostic | #8 | confirmed | 1800-2023 20.10 20.2 A.1.4 | 20.10：メッセージ書式はツールが定めるため、正規表現はどちらの順序も受理すべき |
| `ch20/test_fatal_form_without_parentheses` | diagnostic | #8 | confirmed | 1800-2023 20.10 20.2 A.1.4 | 20.10：メッセージ書式はツールが定めるため、正規表現はどちらの順序も受理すべき |
| `ch20/test_severity_tasks_report_contents` | diagnostic | #8 | confirmed | 1800-2023 20.10 22.13 | 20.10：メッセージ書式はツールが定めるため、正規表現はどちらの順序も受理すべき |
| `ch21/test_readmemh_address_range` | runtime | #9 | confirmed | 1800-2023 21.4.3 21.4.1 7.4.4 | 21.4/21.4.3：start/finish は最上位次元のアドレスで、2..5 は範囲外 |
| `ch22/test_22_extra_include_syntax` | compile | #16 | confirmed | 1800-2023 22.4 22.5.1 | 22.4：山括弧の include は標準の場所だけを探す |
| `ch23/test_scope_creators_all_kinds` | compile | #10 | confirmed | 1800-2023 23.9 | 17.5：checker の initial に遅延や $display は置けない |
| `ch23/test_syntax_port_expression_and_explicit_ports` | compile | #15 | confirmed | 1800-2023 23.2.2.1 23.2.2.2 23.2.2.3 A.1.3 | Annex B：cell は予約語であり、ポートの識別子にはできない |
| `ch25/test_interface_abstract_methods_and_broadcast` | compile | #16 | confirmed | 1800-2023 25.2 25.7.4 | 25.5：modport は列挙した名前へのアクセスだけを認めるが、caller に value がない |
| `ch25/test_interface_extern_and_generate_items` | compile | #16 | confirmed | 1800-2023 25.7 25.3 A.1.6 A.2.9 | 25.3：interface の中で module をインスタンス化できない |
| `ch25/test_virtual_interface_data_type_forms` | compile | #17 | confirmed | 1800-2023 25.9 25.9.2 A.2.2.1 | 25.9：virtual interface をポートに使ってはいけない |
| `ch5/test_attribute_prefix_positions` | compile | #16 | confirmed | 1800-2023 5.12 | 6.10：使用後の明示的ネット宣言は再宣言になるため、instance より前に宣言する |
| `ch6/test_design_element_static_lifetime` | compile | #6 | confirmed | 1800-2023 6.21 | 17.5/17.7.1：checker の initial 代入と通常の always |
| `ch6/test_void_return_and_tagged_union_void_member` | compile | #17 | confirmed | 1800-2023 6.13 7.3.2 11.9 | 6.13：方向キーワードは out ではなく output |
| `ch7/test_union_grammar_forms` | compile | #17 | confirmed | 1800-2023 7.3 | 7.3.1：soft packed union のメンバも整数型が必要 |

### 2.2 kirimasim（25 本）

| テスト | 種類 | Issue | 確度 | 節 | 注記 |
|---|---|---|---|---|---|
| `ch11/test_conditional_real_shortreal_branch_promotion` | runtime | #61 | confirmed | 1800-2023 11.4.11 6.12 | shortreal の結果が単精度に丸められていない |
| `ch11/test_expr_conditional_ambiguous` | runtime | #60 | confirmed | 1800-2023 11.4.11 | x/z の述語で、異なる集約を要素ごとに結合していない |
| `ch11/test_precedence_distinguishing_values` | runtime | #54 | confirmed | 1800-2023 11.3.2 11.4.3 | 既存 Issue：?: は -> より強く結び付く必要がある |
| `ch11/test_precedence_logical_conditional_implication_levels` | runtime | #54 | confirmed | 1800-2023 11.3.2 11.4.7 11.4.11 | 既存 Issue：?: は -> より強く結び付く必要がある |
| `ch12/test_syntax_loop_statement_forms` | runtime | #65 | confirmed | 1800-2023 12.7 12.7.1 12.7.2 12.7.3 12.7.4 12.7.5 12.7.6 A.6.8 A.6.4 | 全ループ変数を省略した foreach が 1 回反復する |
| `ch13/test_constant_function_call_fresh_initial_values` | runtime | #68 | confirmed | 1800-2023 13.4.3 | 定数呼び出しで static ローカルの初期化式が無視される |
| `ch16/test_deferred_flush_points` | runtime | #62 | confirmed | 1800-2023 16.4.2 16.4.4 16.4.1 | 遅延アサーションの破棄時点が不足している |
| `ch16/test_syntax_deferred_immediate_assertions` | runtime | #62 | confirmed | 1800-2023 16.3 16.4 A.6.10 A.6.4 | 遅延 immediate assertion のスケジューリング |
| `ch20/test_illegal_timeformat_unit_out_of_range` | diagnostic | #70 | confirmed | 1800-2023 20.4.3 | 範囲外 unit が終了 3 の internal error になる |
| `ch21/test_vcd_scope_commands_begin_fork_task` | runtime | #69 | confirmed | 1800-2023 21.7.2.3 21.7.2.1 | VCD の $scope が名前付き fork ブロックを省略する |
| `ch23/test_defparam_last_in_source_text_wins` | runtime | #59 | confirmed | 1800-2023 23.10.1 | defparam の順序と、インスタンス代入に対する優先順位 |
| `ch23/test_parameter_override_type_and_range` | runtime | #59 | confirmed | 1800-2023 23.10 6.20.2 6.12.1 | defparam の順序と、インスタンス代入に対する優先順位 |
| `ch24/test_program_port_reactive_variable_and_net` | runtime | #63 | confirmed | 1800-2023 24.3.2 24.3.1 4.5 23.3.3 | program が再開する前に program ポートのネットが更新されない |
| `ch33/test_config_binding_descends_through_children` | runtime | #58 | confirmed | 1800-2023 33.2 | --top 指定時に chooseConfigTop が config 選択を飛ばす |
| `ch33/test_config_cell_parameter` | runtime | #58 | confirmed | 1800-2023 33.4.3 33.4.1.4 33.4.1.6 | --top 指定時に chooseConfigTop が config 選択を飛ばす |
| `ch33/test_config_hierarchical` | compile | #58 | confirmed | 1800-2023 33.4.2 33.4.1.3 33.4.1.6 33.4.1.1 33.4.1.5 33.3.1.1 | 同じ chooseConfigTop が 2 つ目の config を拒否する。テストのコメントは top module が選ぶ config を明記している |
| `ch33/test_config_instance_over_cell` | runtime | #58 | confirmed | 1800-2023 33.4.1.3 33.4.1.4 33.4.1.6 | --top 指定時に chooseConfigTop が config 選択を飛ばす |
| `ch33/test_config_library_cell_and_topmodule_names` | runtime | #58 | confirmed | 1800-2023 33.4.1 33.4.1.1 33.4.1.4 33.7 A.9.3 | --top 指定時に chooseConfigTop が config 選択を飛ばす |
| `ch33/test_config_name_disambiguation_config_suffix` | compile | #58 | confirmed | 1800-2023 33.2.1 33.4.1.6 33.4.2 | 同じ chooseConfigTop が 2 つ目の config を拒否する。テストのコメントは top module が選ぶ config を明記している |
| `ch33/test_config_rules_bind_each_instance` | runtime | #58 | confirmed | 1800-2023 33.2 | --top 指定時に chooseConfigTop が config 選択を飛ばす |
| `ch33/test_config_use_config_suffix` | compile | #58 | confirmed | 1800-2023 33.4.1.6 33.4.1.1 33.4.2 33.4.1.3 | 同じ chooseConfigTop が 2 つ目の config を拒否する。テストのコメントは top module が選ぶ config を明記している |
| `ch4/test_reactive_region_set_order` | runtime | #63 | confirmed | 1800-2023 4.4.2.7 4.4.2.8 4.5 24.3 | program が module の Active 領域の更新より先に走る |
| `ch7/test_queue_invalid_index_write_ignored_warning` | diagnostic | #67 | confirmed | 1800-2023 7.10.1 | 7.10.1 は無効なキュー書き込みに警告を要求する |
| `ch7/test_queue_method_prototypes_and_invalid_indices` | runtime | #66 | confirmed | 1800-2023 7.10.2.2 7.10.2.1 7.10.2.3 7.10.2.4 7.10.2.5 7.10.2.6 7.10.2.7 | キューの添字を integer に変換していない |
| `ch9/test_always_comb_longest_static_prefix_and_locals` | runtime | #64 | confirmed | 1800-2023 9.2.2.2.1 9.2.2.2 11.5.3 | 定数ビット選択の感度範囲を狭めていない |

### 2.3 slang（104 本）

| テスト | 種類 | Issue | 確度 | 節 | 注記 |
|---|---|---|---|---|---|
| `ch10/test_illegal_force_mixed_driven_variable` | illegal | #82 | confirmed | 1800-2023 10.6.2 | 10.6.2：連続・手続きドライバが混在する変数への force は不正 |
| `ch10/test_syntax_assignment_pattern_forms` | compile | #83 | confirmed | 1800-2023 10.9 10.9.1 10.9.2 A.6.7.1 | 10.9.2/A.6.7.1：代入パターンは左辺値になれる |
| `ch11/test_concatenation_pattern_string_operand_and_replication_once` | compile | #83 | confirmed | 1800-2023 11.4.12 11.4.12.1 11.4.12.2 6.16 | スイートのレビューで条項を確認し、テストが正しいと判断している |
| `ch11/test_illegal_unsized_constant_in_concatenation` | illegal | #82 | confirmed | 1800-2023 11.4.12 | Icarus が期待位置で拒否する |
| `ch11/test_stream_item_order_struct_union_object_assoc` | compile | #83 | confirmed | 1800-2023 11.4.14.1 | スイートのレビューで条項を確認し、テストが正しいと判断している |
| `ch11/test_stream_unpack_consumes_left_converts_state_keeps_objects` | compile | #83 | confirmed | 1800-2023 11.4.14.3 | スイートのレビューで条項を確認し、テストが正しいと判断している |
| `ch12/test_illegal_for_type_reference_without_var` | illegal | #82 | confirmed | 1800-2023 12.7 6.23 | Icarus が期待位置で拒否する |
| `ch12/test_statement_item_forms` | compile | #90 | confirmed | 1800-2023 12.3 12.2 | 13.5.2：ref の実引数は変数にでき、この int 変数は合法 |
| `ch13/test_A_extra_tf_dims` | compile | #83 | confirmed | 1800-2023 13.3 A.2.3 | A.2.6：tf port item は初期化式を持てる |
| `ch13/test_illegal_constant_function_call_in_constant_context` | illegal | #82 | confirmed | 1800-2023 13.4.3 | 13.4.3：定数関数は定数式の文脈で定数関数を呼べない |
| `ch16/test_assume_property_actions` | compile | #78 | confirmed | 1800-2023 16.14.2 16.14.4 20.11 20.10 16.12 | 16.3/A.6.3：else の前の空の pass 文は合法な action_block |
| `ch16/test_ch16_sim_checker_inference` | compile | #77 | confirmed | 1800-2023 16.13.6 16.16 | 17.3・17.6・17.7.1 はこれらの checker の形を認める |
| `ch16/test_concurrent_assert_actions` | compile | #78 | confirmed | 1800-2023 16.14.1 16.3 20.10 20.11 4.4.2 16.12 | 16.3/A.6.3：else の前の空の pass 文は合法な action_block |
| `ch16/test_illegal_deferred_assertion_const_ref_dynamic_actual` | illegal | #82 | confirmed | 1800-2023 16.4 | 16.4：動的変数は遅延アサーションのアクションの ref/const ref 実引数にできない |
| `ch16/test_named_sequence_scopes_and_instances` | compile | #78 | confirmed | 1800-2023 16.8 | 16.8：sequence 宣言の最後のセミコロンは任意 |
| `ch16/test_recursive_property_time_advance` | compile | #86 | confirmed | 1800-2023 16.12 16.12.17 | 16.12.17 制限 3：再帰インスタンスは ##1 の後にある |
| `ch17/test_checker_body_declaration_kinds` | compile | #77 | confirmed | 1800-2023 17.2 17.7 A.1.8 | 17.3・17.6・17.7.1 はこれらの checker の形を認める |
| `ch17/test_checker_covergroup_sample` | compile | #77 | confirmed | 1800-2023 17.6 19.8.1 19.11.1 | 17.3・17.6・17.7.1 はこれらの checker の形を認める |
| `ch17/test_checker_instantiation_connection_forms` | compile | #77 | confirmed | 1800-2023 17.3 17.2 A.4.1.4 | 17.3・17.6・17.7.1 はこれらの checker の形を認める |
| `ch17/test_checker_procedures_reactive_renba` | compile | #77 | confirmed | 1800-2023 17.7.3 24.3.1 | 17.3・17.6・17.7.1 はこれらの checker の形を認める |
| `ch18/test_inline_local_scope_binding` | compile | #93 | confirmed | 1800-2023 18.7.1 | 18.7.1：local::obj は受け手の class スコープへ束縛する |
| `ch18/test_rand_mode_inactive_handle` | compile | #83 | confirmed | 1800-2023 18.8 18.3 18.5.8 | 18.8：引数なしの rand_mode() は現在の状態を返す |
| `ch19/test_cg_inheritance_args_and_cross` | compile | #95 | confirmed | 1800-2023 19.4.1 19.4 19.7 19.11 19.11.1 19.11.2 19.8.1 | 19.4.1：派生 covergroup の同名の cross は基底の cross を置き換える |
| `ch19/test_covergroup_cross_and_select_forms` | compile | #96 | confirmed | 1800-2023 19.6 19.6.1 19.6.1.2 19.6.1.3 19.6.1.4 19.11.2 A.2.11 19.2 | 19.6.1：binsof は cover point の bin を受け、ca.v[0] はそれに当たる |
| `ch19/test_illegal_cg_function_side_effect` | illegal | #81 | confirmed | 1800-2023 19.5 | Icarus が期待位置で拒否する |
| `ch19/test_illegal_cg_outer_constant` | illegal | #81 | confirmed | 1800-2023 19.5 | Icarus が期待位置で拒否する |
| `ch19/test_illegal_cg_ref_formal_expression` | illegal | #81 | confirmed | 1800-2023 19.5 | Icarus が期待位置で拒否する |
| `ch19/test_illegal_cg_ref_formal_variable` | illegal | #81 | confirmed | 1800-2023 19.5 | Icarus が期待位置で拒否する |
| `ch20/test_array_query_default_dimension_integer_result` | compile | #75 | confirmed | 1800-2023 20.7 | 20.7 はこれらの引数に 0 または x を定める。Icarus と Verilator は real を受理する |
| `ch20/test_array_query_types_and_invalid` | compile | #75 | confirmed | 1800-2023 20.7 | 20.7 はこれらの引数に 0 または x を定める。Icarus と Verilator は real を受理する |
| `ch20/test_illegal_bits_function_dynamic_return` | illegal | #82 | confirmed | 1800-2023 20.6.2 | 20.6.2：動的サイズ型を返す関数への $bits はエラー |
| `ch20/test_illegal_dist_exponential_zero_mean` | illegal | #82 | confirmed | 1800-2023 20.14.2 | 20.14.2：$dist_exponential の平均は 0 より大きい必要がある |
| `ch20/test_illegal_isunbounded_variable` | illegal | #82 | confirmed | 1800-2023 20.6.3 | 20.6.3：$isunbounded の引数はパラメータ名が必要 |
| `ch21/test_ch21_extra_pattern_unpacked_struct` | compile | #83 | confirmed | 1800-2023 21.2.1.6 | スイートのレビューで条項を確認し、テストが正しいと判断している |
| `ch21/test_evcd_dumpports_files_defaults_and_control_scope` | compile | #74 | confirmed | 1800-2023 21.7.3.1 21.7.3.2 21.7.3.3 21.7.3.5 21.7.3.7 21.7.4.3 21.7.4.3.1 | 21.3.1 と 21.6 は文字列を含む整数値を認める |
| `ch21/test_evcd_dumpportslimit_named_by_variables` | compile | #74 | confirmed | 1800-2023 21.7.3.4 21.7.3.1 | 21.3.1 と 21.6 は文字列を含む整数値を認める |
| `ch21/test_fopen_name_and_type_forms` | compile | #74 | confirmed | 1800-2023 21.3.1 21.3.2 21.3.4.2 | 21.3.1 と 21.6 は文字列を含む整数値を認める |
| `ch21/test_fread_unpacked_struct_union` | compile | #83 | confirmed | 1800-2023 21.3.4.4 | 21.3.4.4：unpacked struct/union への $fread が定められている |
| `ch21/test_plusargs_key_from_variable` | compile | #74 | confirmed | 1800-2023 21.6 | 21.3.1 と 21.6 は文字列を含む整数値を認める |
| `ch21/test_vcd_dumpvars_all_filename_start` | compile | #74 | confirmed | 1800-2023 21.7.1.1 21.7.1.2 21.7.1.3 | 21.3.1 と 21.6 は文字列を含む整数値を認める |
| `ch22/test_ifdef_directive_names_not_defined` | compile | #78 | confirmed | 1800-2023 22.6 | 22.6：ifdef と elsif では directive 名を未定義として扱う |
| `ch22/test_illegal_begin_keywords_without_end` | illegal | #79 | confirmed | 1800-2023 22.14 | 22.14：begin_keywords と end_keywords は対で使う必要がある |
| `ch22/test_illegal_directive_split_across_lines` | illegal | #79 | confirmed | 1800-2023 22.2 22.7 | 22.2：複数行を許すと明記する節以外、directive は 1 行に収める |
| `ch22/test_illegal_include_absolute_path_in_angle_brackets` | illegal | #79 | confirmed | 1800-2023 22.4 | Icarus が期待位置で拒否する |
| `ch22/test_illegal_include_line_trailing_code` | illegal | #79 | confirmed | 1800-2023 22.4 | Icarus が期待位置で拒否する |
| `ch22/test_illegal_line_directive_trailing_comment` | illegal | #79 | confirmed | 1800-2023 22.12 | 22.12：line directive の行にコメントは置けない |
| `ch23/test_23_extra_upward_data_stop` | compile | #83 | confirmed | 1800-2023 23.8 | 23.8：上方参照は構造体オブジェクトで止まる |
| `ch23/test_23_extra_uwire_dominates` | compile | #71 | confirmed | 1800-2023 23.3.3.7 28.16 6.6.2 6.6.1 | 6.6.2 は inout uwire を禁止しない。Icarus と Verilator は受理する |
| `ch23/test_23_extra_uwire_warn` | compile | #71 | confirmed | 1800-2023 23.3.3.7 28.16 6.6.2 6.6.3 | 6.6.2 は inout uwire を禁止しない。Icarus と Verilator は受理する |
| `ch23/test_defparam_block_task_function_parameters` | compile | #76 | confirmed | 1800-2023 23.10.2 23.10.1 23.10.3 | 23.10.1 と 23.10.2 は program・block・task・function のパラメータを認める |
| `ch23/test_defparam_targets_module_interface_program` | compile | #76 | confirmed | 1800-2023 23.10.1 23.6 24.3 25.3 | 23.10.1 と 23.10.2 は program・block・task・function のパラメータを認める |
| `ch23/test_illegal_defparam_array_sibling_instance` | illegal | #79 | confirmed | 1800-2023 23.10.1 23.3.2 | 23.10.1：配列要素内の defparam は別の要素を対象にできない |
| `ch23/test_illegal_no_top_level_module` | illegal | #79 | confirmed | 1800-2023 23.3.1 | 23.3.1：設計は少なくとも 1 つの top-level module を含む必要がある |
| `ch23/test_illegal_scope_duplicate_in_unselected_generate` | illegal | #79 | confirmed | 1800-2023 23.9 27.5 | Icarus が期待位置で拒否する |
| `ch23/test_interconnect_primitive_broadcast_input` | compile | #72 | confirmed | 1800-2023 23.3.3.7.2 6.6.8 | 6.6.8 と 23.3.3.7.2 は端子接続と net_alias の使用を認める |
| `ch23/test_interconnect_primitive_terminal` | compile | #72 | confirmed | 1800-2023 23.3.3.7.2 6.6.8 23.3.3.7.1 | 6.6.8 と 23.3.3.7.2 は端子接続と net_alias の使用を認める |
| `ch23/test_interconnect_terminal_wire_resolution` | compile | #72 | confirmed | 1800-2023 23.3.3.7.2 6.6.8 6.6.1 29.3 | 6.6.8 と 23.3.3.7.2 は端子接続と net_alias の使用を認める |
| `ch23/test_port_collapse_trireg_and_uwire_cells` | compile | #71 | confirmed | 1800-2023 23.3.3.7 | 6.6.2 は inout uwire を禁止しない。Icarus と Verilator は受理する |
| `ch23/test_port_collapse_uwire_external_delay` | compile | #71 | confirmed | 1800-2023 23.3.3.7 28.16 6.6.1 6.6.2 | 6.6.2 は inout uwire を禁止しない。Icarus と Verilator は受理する |
| `ch23/test_port_collapse_uwire_supply` | compile | #71 | confirmed | 1800-2023 23.3.3.7 6.6.2 6.6.6 | 6.6.2 は inout uwire を禁止しない。Icarus と Verilator は受理する |
| `ch23/test_port_collapse_wand_uwire_warning` | compile | #71 | confirmed | 1800-2023 23.3.3.7 28.16 6.6.3 6.6.2 | 6.6.2 は inout uwire を禁止しない。Icarus と Verilator は受理する |
| `ch23/test_port_collapse_wor_uwire_warning` | compile | #71 | confirmed | 1800-2023 23.3.3.7 28.16 6.6.3 6.6.2 | 6.6.2 は inout uwire を禁止しない。Icarus と Verilator は受理する |
| `ch24/test_illegal_anonymous_program_hierarchical_reference` | illegal | #79 | confirmed | 1800-2023 24.3 24.6 | 24.3：無名 program は他の program スコープを階層参照できない |
| `ch25/test_illegal_generic_interface_port_dot_name` | illegal | #80 | confirmed | 1800-2023 25.3.3 | 25.3.3：generic interface には名前付きポートが必要で、暗黙の .name 接続は使えない |
| `ch25/test_illegal_interface_port_actual_through_generate_block` | illegal | #80 | confirmed | 1800-2023 25.3 | 25.3：interface ポートの実引数は generate ブロックを経由して解決してはいけない |
| `ch25/test_illegal_interface_port_actual_through_instance_array` | illegal | #80 | confirmed | 1800-2023 25.3 | 25.3：interface ポートの実引数はインスタンス配列を経由して解決してはいけない |
| `ch25/test_illegal_modport_import_default_argument_without_prototype` | illegal | #80 | confirmed | 1800-2023 25.7 | 25.7：既定引数には modport import の完全なプロトタイプが必要 |
| `ch25/test_illegal_modport_import_named_binding_without_prototype` | illegal | #80 | confirmed | 1800-2023 25.7 | 25.7：名前による束縛には modport import の完全なプロトタイプが必要 |
| `ch25/test_illegal_modport_prototype_unnamed_named_binding` | illegal | #80 | confirmed | 1800-2023 25.7 | 25.7：名前による束縛にはプロトタイプ内の名前付き仮引数が必要 |
| `ch26/test_illegal_package_imported_before_compiled` | illegal | #79 | confirmed | 1800-2023 26.3 | Icarus が期待位置で拒否する |
| `ch28/test_gate_delay_forms` | compile | #83 | confirmed | 1800-2023 28.3.3 28.16 28.4 A.2.2.3 | 28.16.1：ゲートプリミティブは遅延値 3 個を認める |
| `ch3/test_declaration_across_file_boundary` | compile | #78 | confirmed | 1800-2023 3.12.1 | 3.12.1：未完了の宣言は次のファイルへ続く |
| `ch3/test_element_and_block_name_spaces` | compile | #83 | confirmed | 1800-2023 3.13 | 3.13：specify ブロックは固有の名前空間を持つ |
| `ch33/test_config_declaration_grammar_forms` | compile | #78 | confirmed | 1800-2023 33.4.1 33.4.3 A.1.5 | 33.4.1：use cell の named_parameter_assignment は合法 |
| `ch33/test_duplicate_cell_last_definition_wins` | compile | #83 | confirmed | 1800-2023 33.3.1 33.3.1.1 33.3.3 | 33.3.1.1：同名で最後の cell をライブラリへ記録する |
| `ch33/test_illegal_config_constant_function` | illegal | #79 | confirmed | 1800-2023 33.4.3 13.4.3 | Icarus が期待位置で拒否する |
| `ch33/test_illegal_config_generate_scope_path` | illegal | #79 | confirmed | 1800-2023 33.4.3 27.5 | Icarus が期待位置で拒否する |
| `ch33/test_illegal_config_hierarchical_name_in_expression` | illegal | #79 | confirmed | 1800-2023 33.4.3 | Icarus が期待位置で拒否する |
| `ch33/test_illegal_config_index_not_literal` | illegal | #79 | confirmed | 1800-2023 33.4.3 | Icarus が期待位置で拒否する |
| `ch33/test_illegal_config_instance_array_path` | illegal | #79 | confirmed | 1800-2023 33.4.3 23.3.3.5 | Icarus が期待位置で拒否する |
| `ch33/test_illegal_config_package_binding` | illegal | #79 | confirmed | 1800-2023 33.4 33.4.1.4 33.4.1.6 | Icarus が期待位置で拒否する |
| `ch5/test_illegal_method_implicit_variable_without_parentheses` | illegal | #82 | confirmed | 1800-2023 5.13 13.4.1 6.16 | 5.13：暗黙の変数には常に括弧が必要 |
| `ch5/test_illegal_string_hex_escape_z_digit` | illegal | #82 | confirmed | 1800-2023 5.9.1 | 5.9.1：文字列のエスケープで z は 16 進桁にできない |
| `ch5/test_illegal_string_octal_escape_z_digit` | illegal | #82 | confirmed | 1800-2023 5.9.1 | 5.9.1：文字列のエスケープで z は 8 進桁にできない |
| `ch6/test_a2_1_3_interconnect_declaration_identifiers` | compile | #72 | confirmed | 1800-2023 6.6.8 23.3.3.7 A.2.1.3 | 6.6.8 と 23.3.3.7.2 は端子接続と net_alias の使用を認める |
| `ch6/test_illegal_initial_block_variable_initializer_no_keyword` | illegal | #82 | confirmed | 1800-2023 6.21 9.2.1 | 6.21：初期化式を持つ static 変数には static キーワードが必要 |
| `ch6/test_illegal_net_type_reg_in_port_declaration` | illegal | #82 | confirmed | 1800-2023 6.7.1 23.2.2.3 | 6.7.1 と 23.2.2.3：reg はネットのデータ型ではない |
| `ch6/test_illegal_static_task_variable_initializer_no_keyword` | illegal | #82 | confirmed | 1800-2023 6.21 13.3 | 6.21：初期化式を持つ static task の変数には static キーワードが必要 |
| `ch6/test_illegal_type_operator_dynamic_array_element` | illegal | #82 | confirmed | 1800-2023 6.23 | Icarus が期待位置で拒否する |
| `ch6/test_interconnect_and_net_port_types` | compile | #72 | confirmed | 1800-2023 6.6.8 23.3.3.7 A.2.1.3 A.2.2.1 | 6.6.8 と 23.3.3.7.2 は端子接続と net_alias の使用を認める |
| `ch6/test_nettype_alias_resolution_and_empty_drivers` | compile | #73 | confirmed | 1800-2023 6.7.2 6.7.3 6.22.6 | 6.6.7 と 23.3.3：スイートのレビューで条項を確認し、テストが正しいと判断している |
| `ch6/test_nettype_declarations_and_user_nets` | compile | #73 | confirmed | 1800-2023 6.6.7 6.7.2 6.7.3 10.3.3 A.2.1.3 A.2.3 A.2.4 | 6.6.7 と 23.3.3：スイートのレビューで条項を確認し、テストが正しいと判断している |
| `ch6/test_nettype_rename_matches` | compile | #73 | confirmed | 1800-2023 6.22.6 6.6.7 23.3.3 | 6.6.7 と 23.3.3：スイートのレビューで条項を確認し、テストが正しいと判断している |
| `ch6/test_nettype_resolution` | compile | #73 | confirmed | 1800-2023 6.6.7 10.3 10.6.2 23.3.3 | 6.6.7 と 23.3.3：スイートのレビューで条項を確認し、テストが正しいと判断している |
| `ch6/test_packed_union_size_and_implicit_conversion` | compile | #98 | confirmed | 1800-2023 6.24.1 6.19.4 | 6.24.3：固定サイズのメンバを持つ unpacked union は bit-stream 型 |
| `ch6/test_param_override_three_ways_module_interface_program` | compile | #76 | confirmed | 1800-2023 6.20 23.10.1 23.10.2.1 23.10.2.2 24.3 25.3 | 23.10.1 と 23.10.2 は program・block・task・function のパラメータを認める |
| `ch6/test_user_nettype_resolution_forms` | compile | #73 | confirmed | 1800-2023 6.6.7 6.6 6.7 6.7.2 6.7.3 13.8 | 6.6.7 と 23.3.3：スイートのレビューで条項を確認し、テストが正しいと判断している |
| `ch7/test_array_index_and_size_signatures` | compile | #92 | confirmed | 1800-2023 7.4.5 7.5.2 | 7.4.5：無効な添字の読み取りは Table 7-1 の値を返し、書き込みは何もしない |
| `ch7/test_assoc_narrow_prev_next_overflow` | compile | #83 | confirmed | 1800-2023 7.9.8 7.9.6 7.9.7 | スイートのレビューで条項を確認し、テストが正しいと判断している |
| `ch7/test_assoc_traversal_narrow_index` | compile | #83 | confirmed | 1800-2023 7.9.8 7.9.4 7.9.5 | スイートのレビューで条項を確認し、テストが正しいと判断している |
| `ch7/test_illegal_dynamic_new_nonblocking_assignment` | illegal | #82 | confirmed | 1800-2023 7.5.1 | Icarus が期待位置で拒否する |
| `ch7/test_illegal_union_holding_struct_member_default` | illegal | #82 | confirmed | 1800-2023 7.2.2 | 7.2.2：union を含む unpacked struct のメンバは既定値を持てない |
| `ch8/test_class_declaration_grammar_forms` | compile | #99 | confirmed | 1800-2023 8.3 8.17 8.20 8.24 8.26 | 8.26.2：実装側の class で宣言したメソッドが実装を提供できる |
| `ch8/test_illegal_interface_typedef_scope_prefix_in_variable_declaration` | illegal | #80 | confirmed | 1800-2023 8.23 6.18 | Icarus が期待位置で拒否する |

## 3. 推定（8 本）

| テスト | 種類 | Issue | 確度 | 節 | 注記 |
|---|---|---|---|---|---|
| `ch17/test_checker_always_statement_kinds` | compile | #6 | likely | 1800-2023 17.5 | 17.5 は block item 宣言を列挙していない。for ループの宣言は確認が必要 |
| `ch11/test_expr_timescale_constants` | compile | #89 | likely | 1800-2023 11.2.1 | 11.2.1 は $timescale と $timeprecision を列挙する。$timeunit の省略は編集上の可能性がある |
| `ch16/test_sampled_automatic_and_const_current` | compile | #87 | likely | 1800-2023 16.5.1 16.5.2 16.14.6.1 | 16.14.6 は手続きコードと述べる。task は手続きコードだが文面は間接的 |
| `ch16/test_sequence_match_item_and_triggered_method` | compile | #88 | likely | 1800-2023 16.13.6 16.10 16.11 A.8.4 A.10 A.9.3 | 16.10 は static 変数の代入先を明示的に禁止していない |
| `ch16/test_sequence_operator_and_delay_forms` | compile | #88 | likely | 1800-2023 16.9 16.10 16.12.2 16.12.15 16.14.1 A.2.10 | 16.10 は static 変数の代入先を明示的に禁止していない |
| `ch18/test_rand_enum_solver_finds_solution` | compile | #94 | likely | 1800-2023 18.3 | 制約式の規則は文字列に明示的に言及していない |
| `ch30/test_A_extra_module_path` | compile | #97 | likely | 1800-2023 30.4.4 11.2.1 A.7.2 A.8.3 A.8.4 | A.8.4 の module path primary は関数呼び出しと連結を含む。文法は未確認 |
| `ch5/test_string_literal_cast_justification` | compile | #91 | likely | 1800-2023 5.9 6.24.1 | 6.24.3 は bit-stream cast で string を byte 配列と扱う。unpacked の変換先は確認が必要 |

## 4. 未トリアージ

なし。

## 5. 286 本の「`:expected_stdout:` なし」

176 本とは別枠です。詳細は [再解析 §0](kirimasim_failure_analysis_2026-10-28.ja.md#286-本のexpected_stdout-なしの内訳) を参照。

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
4. CSV を更新したら、この Markdown を再生成し、両言語を同期する。

## 7. 注意

- `confirmed` は「修正が済んだ」意味ではありません。Issue が close するまで残ります。
- 2,225 本の unsupported はこの台帳の対象外です。別途、機能単位の台帳が必要です。
- 5,053 本の pass は再監査していません。期待値が誤っていても、kirimasim が
  それに一致すれば pass になります。
