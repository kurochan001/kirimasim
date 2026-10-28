# 非 slang 65 本のトリアージ（2026-10-28）

対象: fail 176 本のうち、slang 起因を除く **kirimasim 25 本 + テスト側 40 本**。
残り 112 本の slang は別台帳（`docs/kirimasim_failure_triage_2026-10-28.md`）と
`docs/notes/slang-*-issue-draft.md` で扱います。

- kirimasim: **25 本すべて confirmed**（#54、#58〜#70）
- テスト側: **39 本 confirmed + 1 本 likely**（#6〜#10、#15〜#19）
- この 65 本に slang の判断は含まれません。

---

## 1. kirimasim の 25 本

### 1.1 ch33 config の選択（8 本、#58）

**根因**: `src/front/front.cpp` の `chooseConfigTop` が
`driver.options.topModules` 非空で即 return する。テストランナーは
`:top_module:` があると `--top=<module>` を渡すため、config が適用されない。
`--top` がない場合は最初の config を取り、2 つ目をエラーにする。

**修正**: `--top=<module>` のとき、design 文がその module を指す config を
選んで `"<config>:config"` を top にする。`--top` がないときは 2 つ目を
エラーにせず、テストの期待に合わせて最後の config を選ぶ。

| テスト | 症状 |
|---|---|
| `ch33/test_config_cell_parameter` | `u1=1 u2=1`（期待 `u1=5 u2=5`） |
| `ch33/test_config_instance_over_cell` | `u1=a1 u2=a1`（期待 `u1=b1 u2=c1`） |
| `ch33/test_config_binding_descends_through_children` | m2 の leaf が alt にならない |
| `ch33/test_config_library_cell_and_topmodule_names` | `work.cfg_a` のまま |
| `ch33/test_config_rules_bind_each_instance` | a2 が `crb_adder` のまま |
| `ch33/test_config_hierarchical` | `second config 'cfg'` エラー |
| `ch33/test_config_name_disambiguation_config_suffix` | `second config 'ccn'` エラー |
| `ch33/test_config_use_config_suffix` | `second config 'cfg'` エラー |

**証拠**: `--top` を外すと同じバイナリで 5 本が期待どおり。テストのコメントも
config の design が `:top_module:` の module を指すと書いている。

### 1.2 defparam（2 本、#59）

**根因**: defparam をソーステキスト順ではなく elaboration の処理順で適用し、
defparam とインスタンス代入の優先順位も逆。

**修正**: 同一パラメータへの defparam をソース順に適用し、defparam を
インスタンスのパラメータ代入より優先する（23.10.1、23.10.4）。

| テスト | 症状 |
|---|---|
| `ch23/test_defparam_last_in_source_text_wins` | `A=1 B=3 E=7`（期待 `A=2 B=4 E=8`） |
| `ch23/test_parameter_override_type_and_range` | `u3.U==2.5` が 0（defparam が負け） |

### 1.3 `?:` と `->` の優先順位（2 本、#54 既存）

**根因**: `?:` が `->` より下にグループ化される。表 11-2 では `?:` が上。

| テスト | 症状 |
|---|---|
| `ch11/test_precedence_distinguishing_values` | `conditional_before_implies=0`（期待 1） |
| `ch11/test_precedence_logical_conditional_implication_levels` | `1 ? 0 : 1 -> 0 = 0`（期待 1） |

### 1.4 条件演算子の型決定（2 本、#60 / #61）

| テスト | 根因 | 修正 |
|---|---|---|
| `ch11/test_expr_conditional_ambiguous` | 述語が x/z のとき、等しくない集約を要素ごとに合成していない | 11.4.11 の要素ごとの合成を実装 |
| `ch11/test_conditional_real_shortreal_branch_promotion` | shortreal 分岐が倍精度のまま | 結果型 shortreal で単精度に丸める |

### 1.5 deferred assertion（2 本、#62）

**根因**: 16.4.2 の flush point（always_comb/always_latch の再開、イベント制御
からの再開、最外スコープの disable）が不足。

| テスト | 症状 |
|---|---|
| `ch16/test_deferred_flush_points` | `t=15 latch fail`（期待 `latch pass`） |
| `ch16/test_syntax_deferred_immediate_assertions` | 1 行目が `wrong` |

### 1.6 program と Reactive 領域（2 本、#63）

**根因**: program が module の Active 更新より先に走る。program ポートの net が
program の再開前に更新されない。

| テスト | 症状 |
|---|---|
| `ch4/test_reactive_region_set_order` | `program: v changed`（期待 `module: ...`） |
| `ch24/test_program_port_reactive_variable_and_net` | `mw=0`（期待 `mw=1`） |

### 1.7 単発（9 本）

| テスト | Issue | 根因 / 修正 |
|---|---|---|
| `ch9/test_always_comb_longest_static_prefix_and_locals` | #64 | `vec[2]` の読みが `vec` 全体の感度。9.2.2.2.1 の longest static prefix を実装 |
| `ch12/test_syntax_loop_statement_forms` | #65 | `foreach (m2[, ])` が 1 回回る。12.7.3 では全ループ変数省略で反復しない |
| `ch7/test_queue_method_prototypes_and_invalid_indices` | #66 | `insert(64'h1_0000_0002,7)` の index を integer に切り詰めていない |
| `ch7/test_queue_invalid_index_write_ignored_warning` | #67 | 7.10.1 が要求する範囲外書き込みの警告がない |
| `ch13/test_constant_function_call_fresh_initial_values` | #68 | 定数関数呼び出しで static ローカルの初期値 10 が使われない |
| `ch21/test_vcd_scope_commands_begin_fork_task` | #69 | VCD に named fork の `$scope` が出ない |
| `ch20/test_illegal_timeformat_unit_out_of_range` | #70 | 範囲外 unit が internal error / exit 3。通常のエラー / exit 2 にする |

（#64〜#70 の 7 本に、上の #60/#61/#62/#63 の 4 本と #59 の 2 本を足すと 13。
#58 の 8 本と #54 の 2 本で 23。残り 2 本は #62 の 2 本で、合計 25。）

---

## 2. テスト側の 40 本

### 2.1 checker の合法性（5 本、#6）

| テスト | 修正 |
|---|---|
| `ch17/test_checker_body_procedural_and_generate_items` | `initial side = 7` を宣言時初期化に変える |
| `ch17/test_checker_declaration_and_port_forms` | `none_n`、`empty_n`、`y2` を宣言時初期化に変える |
| `ch17/test_checker_rand_and_nested_checker` | `rf_out` を宣言時初期化に変える |
| `ch6/test_design_element_static_lifetime` | checker の `int cv = 6;` と `always_ff @(posedge clk) $display(...)` に変える |
| `ch17/test_checker_always_statement_kinds` | **likely**: `int acc;` を checker body に移す。for 初期化子の宣言は要確認 |

### 2.2 deferred assertion の action（3 本、#7）

`assert #0 (...) else x = x + 1;` を、カウンタを増やす小さな task の呼び出しに
変える。16.4.1 では action はサブルーチン呼び出しでなければならない。

- `ch17/test_checker_assertion_placement`
- `ch17/test_checker_body_items`
- `ch17/test_checker_procedural_deferred_assertion`

### 2.3 severity / `$fatal` / clocking の regex（5 本、#8）

`fatal ... file:line:` の語順を要求している。`error[^\n]*file:line:` または
`file:line:[^\n]*error` のどちらでも受ける形にする。cloking の 1 本は
`multiclock` だけでなく明示クロックの文言も受ける。

- `ch20/test_fatal_form_finish_number_and_arguments`
- `ch20/test_fatal_form_finish_number_only`
- `ch20/test_fatal_form_without_parentheses`
- `ch20/test_severity_tasks_report_contents`
- `ch16/test_ch16_sim_clocking_single`

### 2.4 readmemh（1 本、#9）

`m[0:1][0:2]` への start=2/finish=5 は最上位次元の範囲外。期待値を
「警告して未変更」に直す。`docs/simulator-differences.md` も更新する。

### 2.5 specparam / program signal / checker delay（3 本、#10）

- `ch11/test_A_extra_specparam_range`: `localparam = sp[...]` をやめる（6.20.5）。
- `ch17/test_checker_declaration_scope_kinds`: module から `p.hit` を読むのをやめ、
  program ポートか task 経由にする（24.3）。
- `ch23/test_scope_creators_all_kinds`: checker の `initial #9 $display` を
  `always_ff` か `final` に移す（17.5）。

### 2.6 文法に反する構文（6 本、#15）

- `ch11/test_conditional_predicate_and_integral_branch`: `(x matches ...)` の
  外側の括弧を外す。
- `ch12/test_pattern_conditional_predicate`: 同上。
- `ch12/test_pattern_match_known_result`: `matches` を predicate か
  `case ... matches` の中で使う。
- `ch19/test_cg_bin_syntax_all_forms`: `wildcard bins wc = {...}` にする。
- `ch23/test_syntax_port_expression_and_explicit_ports`: port 名 `cell` を
  別名にする。
- `ch11/test_tagged_union_expression_and_matches_precedence`: `$display` の
  `matches` を predicate に移す。

### 2.7 §4.3 の 7 本（#16）

| テスト | 修正 |
|---|---|
| `ch16/test_nexttime_always_eventually_alignment` | 異なるクロックを `and` で結合しない。`##1`/`##0` にする |
| `ch16/test_triggered_matched_multiclock` | `##[1:2]` を `##1`/`##0` にする |
| `ch18/test_uniqueness_constraint_forms` | `a,b,c` を `rand` にする |
| `ch22/test_22_extra_include_syntax` | angle include を `:incdirs:` で探さない。quoted include にする |
| `ch25/test_interface_extern_and_generate_items` | interface 内の module instance を削除する |
| `ch25/test_interface_abstract_methods_and_broadcast` | `caller` modport に `value` を追加する（または `b.value` をやめる） |
| `ch5/test_attribute_prefix_positions` | `wire ub;` を instance の前に置く |

### 2.8 §4.3 の 6 本（#17）

| テスト | 修正 |
|---|---|
| `ch16/test_ch16_sim_checker_history` | `$future_gclk` を property/sequence の中で使う |
| `ch16/test_sequence_formal_arguments_substitution` | `input int z` に `local` を付ける |
| `ch16/test_sequence_local_formal_assignment` | `s_add` にローカル変数を渡す |
| `ch25/test_virtual_interface_data_type_forms` | virtual interface を port にしない（25.9） |
| `ch6/test_void_return_and_tagged_union_void_member` | `out` を `output` にする |
| `ch7/test_union_grammar_forms` | `shortreal` を unpacked union に移す |

### 2.9 配列クエリの real 引数（3 本、#18）

`real`、`shortreal`、`event` は 20.7 のドメイン（配列・整数・string）外。
3 本からこれらの呼び出しを削除する。`test_array_query_types_and_invalid` の
real 行も削除する（string と範囲外 dimension の行は slang #75 に残す）。

### 2.10 空マッチの property（1 本、#19）

`ch16/test_A_extra_repetitions` の `1'b1[*]` は空マッチを含むため、
property には使えません（16.12.22）。`a_any` の行を削除するか `[*1]` に
変えます。

---

## 3. 残る likely 1 本

`ch17/test_checker_always_statement_kinds`（#6）だけが `likely` です。
`int acc;` を always_comb の中に置いています。17.5 の許可リストには宣言が
ないため、テスト側の誤りの可能性が高いですが、ブロック宣言は「文」ではない
とも読めます。**宣言を checker body に移す**書き換えなら、どちらの解釈でも
合法になるので、まずその形に直すのが安全です。

---

## 4. 検証

```sh
python3 tools/run_kirimasim.py \
  --kirimasim <kirimasim>/build/release/src/driver/kirimasim \
  -j 8 --json /tmp/kirimasim-nonslang.json \
  tests/ch33/test_config_*.sv tests/ch23/test_defparam_*.sv \
  tests/ch11/test_*precedence*.sv tests/ch16/test_deferred_*.sv
```

修正前後で `pass` / `fail` を比較し、回帰がないことを確認します。
