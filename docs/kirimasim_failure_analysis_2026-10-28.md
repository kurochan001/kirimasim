# kirimasim 失敗 176 本の再解析 — 責任分界と優先順位

作成: 2026-10-28 / 対象: `main` `a2420294`（`kirimasim 0.0.0`）
元資料: `docs/kirimasim_failure_handoff_2026-10-28.md`
結果: `/tmp/opencode/kirimasim-final.json`（7,740 本、fail 462）

> この文書は AI による再解析です。人による独立確認はまだありません。
> 規格の判断は IEEE 1800-2023 の該当節をローカル PDF で確認しましたが、
> 引用はせず、節番号と要旨だけを書いています。

---

## 0. 結論（前の引き継ぎからの訂正）

引き継ぎは「fail 176 本を kirimasim で直す」と読めますが、**責任の所在は 3 つに分かれます**。

| 責任 | 件数 | 内容 |
|---|---:|---|
| **slang（フロントエンド）** | **111** | コンパイル時の診断。slang は pinned・無改造なので、このリポジトリの `src/` では直せない |
| **kirimasim（実行系・ドライバ）** | **25** | 実行時出力 20 + 診断・終了コード 2 + config 選択 3 |
| **kirimasim-tests（テスト側）** | **40** | 期待値・正規表現・違法設計・構文の取り違え。規格の明文で確認済み 39 + 推定 1 |

つまり、**優先して直すべき kirimasim の実バグは 25 本**です。残り 151 本のうち 111 本は slang 上流の問題、40 本はテストの修正です。

さらに、**25 本のうち 8 本は 1 つの根因**（`--top` 指定時に config を適用しない）です。したがって、
「kirimasim で直す独立した根因」は **18 個**ほどに減ります。

### 特に重要な訂正

1. **ch33 の config 5 本はテストの期待どおりで、kirimasim の `--top` 処理が原因**。
   テストランナーは `:top_module:` があると必ず `--top=<module>` を渡します。
   `chooseConfigTop` は `topModules` が空でないと即 return するため、config が一切適用されません。
   `--top` を外すと同じバイナリで 5 本とも期待どおりになります（§3.1）。

2. **ch23 の uwire/inout 7 本はテストが正しく、slang が厳しすぎる**。
   6.6.2 が禁じているのは「uwire を 2 つ以上のドライバに接続すること」と
   「双方向パススイッチの双方向端子に接続すること」だけで、inout ポートは禁じていません。
   Icarus 13.0 と、手元の Verilator 5.050 も inout uwire を受け付けます
   （テストのコメントは 5.052 が拒否すると書いていますが、5.050 では受理されます。
   5.052 の挙動は未確認です）。

3. **ch17/ch6 の checker 5 本はテストが規格違反**。
   17.5 は checker の initial 手続きに置けるものを let・アサーション・イベント制御だけに限定し、
   17.7.1 は checker 変数の initial での代入を禁じています。
   同じスイートの `test_illegal_checker_variable_initial_assignment` などと矛盾しています。

4. **ch21 の readmemh はテストの期待値が誤り**。
   21.4/21.4.3 では start/finish は「最上位次元のアドレス」を指します。
   `m[0:1][0:2]` に対する 2〜5 は範囲外なので、kirimasim の警告と「未変更」が規格どおりです。
   これはスイートの `docs/simulator-differences.md` にも「候補」として既に載っています。

5. **ch20 の `$fatal` / severity 4 本はテストの正規表現が書式依存**。
   kirimasim は `file:line: fatal: msg`、Icarus/Verilator は `FATAL: file:line: msg` の順です。
   20.10 は書式をツールに任せているので、どちらの順でも受けるべきです。
   同じ不備は 2026-10-25 に 2 本だけ修正済みで、この 4 本は漏れています。

### 286 本の「`:expected_stdout:` なし」の内訳

引き継ぎは 286 本を「テスト整備」としていますが、実際は次の 4 種類です。

| 種類 | 件数 | 扱い |
|---|---:|---|
| `:type: formal`（形式検証用） | 268 | シミュレーションランナーでは実行しない（`run_sby.py` の対象） |
| kirimasim が `error: unsupported:` を返す | 16 | fail ではなく unsupported |
| 期待値が `:expected_stdout_verilator:` のみ | 1 | kirimasim では対象外、または汎用期待値を追加 |
| 出力も期待値もない | 1 | 空の `.expected` を追加 |

したがって、正しい集計は次のとおりです。

| 状態 | 件数 |
|---|---:|
| pass | 5,053 |
| 実際の fail | 176 |
| 期待出力なし（テスト整備） | 2 |
| formal（対象外） | 268 |
| unsupported | 2,241（2,225 + 16） |
| **合計** | **7,740** |

実行可能分母 5,229 と一致率 96.63% は変わりませんが、fail 462 のうち 268 は formal、16 は unsupported です。

---

## 1. 証拠の種類

| 証拠 | 使い方 |
|---|---|
| `kirimasim-final.json` の 462 fail | 176 本の実 fail を抽出（`:expected_stdout:` なし 286 本を除く） |
| `coverage/results/refresh-20261027-icarus13.json`（7,613 本） | Icarus 13.0 の結果。`:oracle:` に iverilog があるテストの裏付け |
| `coverage/results/verilator.json`（3,912 本） | Verilator 5.052 の結果。`:oracle:` に verilator があるテストの裏付け |
| `coverage/results/ghost-local-20261028.json`（7,629 本） | 参考（独立実装ではない可能性があるため弱い証拠） |
| スイートの `docs/simulator-differences.md` | Icarus/Verilator と規格の既知の差。readmemh はここに記載済み |
| スイートの `docs/lyra-disagreement-review-20261025.md` | 194 本の spec-only テストの再確認。期待値の誤りは 0 と結論 |
| IEEE 1800-2023 PDF（ローカル） | 6.6.2、6.20.5、12.7.3、16.4.1、16.10、17.5、17.7.1、21.4、21.4.3、24.3 などを確認 |
| 実行実験 | `--top` の有無、Icarus/Verilator での最小再現 |

176 本の `:oracle:` の内訳:

| oracle | 件数 |
|---|---:|
| `spec` のみ（シミュレータ未検証） | 114 |
| Icarus または Verilator が付き、そのツールが pass | **38** |
| その他（oracle はあるが結果が missing/known） | 24 |

**38 本は「oracle にしたツールが期待どおりに動いている」**ので、テスト期待値が正しい強い証拠になります。
一方、114 本は AI が規格から作った期待値なので、個別の規格確認が必要です。

---

## 2. 責任別の内訳

### 2.1 章別

| 章 | fail | slang | kirimasim | テスト |
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
| その他 | 13 | 10 | 3 | 0 |
| **合計** | **176** | **111** | **25** | **40** |

### 2.2 失敗の種類別

| 種類 | 件数 | slang | kirimasim | テスト |
|---|---:|---:|---:|---:|
| コンパイルエラー（exit 2） | 104 | 67 | 3（config 選択） | 34 |
| 違法設計の受理 | 44 | 44 | 0 | 0 |
| 実行時出力の不一致 | 21 | 0 | 20 | 1 |
| 診断文・終了コード | 7 | 0 | 2 | 5 |

---

## 3. kirimasim で直す 25 本

### 3.1 ch33 config の選択（8 本、1 根因）

対象:

- 実行時 5: `test_config_cell_parameter`、`test_config_instance_over_cell`、
  `test_config_binding_descends_through_children`、`test_config_library_cell_and_topmodule_names`、
  `test_config_rules_bind_each_instance`
- コンパイル 3: `test_config_hierarchical`、`test_config_name_disambiguation_config_suffix`、
  `test_config_use_config_suffix`

根因は `src/front/front.cpp` の `chooseConfigTop`:

```cpp
bool chooseConfigTop(slang::driver::Driver& driver, std::string& diagnostics) {
    if (!driver.options.topModules.empty()) return true;   // ← ここ
    ...
}
```

テストランナー `tools/run_kirimasim.py` は `:top_module:` があると `--top=<module>` を渡します。
そのため config が選ばれず、モジュールが素のまま展開されます。

再現（同じバイナリ、`--top` の有無だけを変える）:

```sh
kirimasim tests/ch33/test_config_cell_parameter.sv
# u1=5 u2=5   ← 期待どおり

kirimasim --top=test_config_cell_parameter tests/ch33/test_config_cell_parameter.sv
# u1=1 u2=1   ← config が適用されない
```

`test_config_library_cell_and_topmodule_names.sv` のコメントにも
「config の design がテストモジュールを指す。`:top_module:` がそれを指す」と書かれており、
テストの意図は明確です。

**修正方針（案）:**

1. `--top=<module>` が与えられたとき、design 文が `<module>` を指す config を探し、
   その config を `topModules` に `"<config>:config"` で入れる。
2. `--top` がないとき、複数の config があってもエラーにしない。
   テストの期待に合わせるなら「最後の config」を選ぶ。
3. `--top` が config 名そのものを指す場合は、今の `:config` 処理を維持する。

この 1 修正で 8 本が同時に通る見込みです。3 の「second config」エラーも同じ関数なので、
別 Issue にせず 1 つにまとめるのが妥当です。

### 3.2 defparam の適用順と優先順位（2 本）

対象: `ch23/test_defparam_last_in_source_text_wins`、
`ch23/test_parameter_override_type_and_range`

23.10.1 は「同じパラメータへの複数の defparam は、ソーステキストで最後のものが勝つ」、
23.10.4 は defparam とインスタンスのパラメータ代入が競合した場合の解決順を定めています。

実際の kirimasim:

- A は 1（最初の defparam）のまま。B は 3（リストの最初）のまま。
- E は早いモジュールの 7 が勝つ。C だけは遅いモジュールの 6 が勝つ。
- u3 は defparam の 2.5 ではなくインスタンス代入の 8'hF0 が勝つ。

つまり「ソース順」ではなく「モジュールの処理順」で上書きしており、
最初の代入が残る場合と最後が残る場合が混在しています。
defparam の解決を「ソーステキスト順」に一本化する必要があります。

### 3.3 式の型決定と優先順位（4 本）

| テスト | 実際 / 期待 | 根因 |
|---|---|---|
| `ch11/test_conditional_real_shortreal_branch_promotion` | `16777217.0` / `16777216.0` | shortreal 分岐の単精度丸めが効いていない |
| `ch11/test_expr_conditional_ambiguous` | `arr=1,2` / `arr=1,0` | 述語が x/z のときの集約の要素ごと合成（11.4.11）が未実装 |
| `ch11/test_precedence_distinguishing_values` | `conditional_before_implies=0` / `1` | `?:` と `->` の優先順位が逆（表 11-2 では `?:` が上） |
| `ch11/test_precedence_logical_conditional_implication_levels` | `1 ? 0 : 1 -> 0 = 0` / `1` | 同上 |

後半 2 本は 1 つの優先順位バグです。前半 2 本も 11.4.11 の型決定として別々の修正になります。

### 3.4 SVA の deferred assertion（2 本）

対象: `ch16/test_deferred_flush_points`、`ch16/test_syntax_deferred_immediate_assertions`

- `test_deferred_flush_points`: always_latch の更新で保留レポートがフラッシュされず、
  `latch fail` を報告する。16.4.2 のフラッシュ点（always_comb/always_latch の再開、
  イベント制御からの再開、最外スコープの disable）が不足しています。
- `test_syntax_deferred_immediate_assertions`: 最初の行が `wrong` で、
  `assert #0` の評価順・サンプル値がずれています。

どちらも 16.4 系の同じ機構（保留キューとフラッシュ）なので、まとめて直すのが妥当です。

### 3.5 program と Reactive 領域（2 本）

対象: `ch4/test_reactive_region_set_order`、`ch24/test_program_port_reactive_variable_and_net`

- ch4: program が module より先に値を記録する。4.5 では Active 領域の集合を先に処理するので、
  module 側が先に見えるべき。
- ch24: program ポートの net `mw` が 0 のまま。program の Reactive 実行とポートの反映順の問題。

同じ「program を Reactive 領域で実行する」機構の可能性が高いので、まとめて確認する価値があります。

### 3.6 残りの単発（7 本、うち診断 2 本）

| テスト | 実際 / 期待 | 根因の見当 |
|---|---|---|
| `ch9/test_always_comb_longest_static_prefix_and_locals` | `runs=3` / `2` | `vec[2]` の読みが `vec` 全体の感度になっている。9.2.2.2.1 の longest static prefix |
| `ch12/test_syntax_loop_statement_forms` | `both=1` / `0` | `foreach (m2[, ])` が 1 回実行される。12.7.3 では全ループ変数省略で反復しない |
| `ch7/test_queue_method_prototypes_and_invalid_indices` | `size=5` / `6` | `insert(64'h1_0000_0002, 7)` の index が integer に切り詰められず、範囲外扱い |
| `ch7/test_queue_invalid_index_write_ignored_warning` | 警告なし | 7.10.1 は範囲外/未知 index への書き込みに「実行時警告を発行する」と規定（may ではない） |
| `ch13/test_constant_function_call_fresh_initial_values` | `P1=1 P2=2` / `11 12` | static ローカル変数の初期値 10 が定数関数呼び出しに反映されない。13.4.3 |
| `ch21/test_vcd_scope_commands_begin_fork_task` | fork scope 0 / 1 | named fork の `$scope` が VCD に出ない。21.7.2.3 |
| `ch20/test_illegal_timeformat_unit_out_of_range` | exit 3 / 2、`internal error` | 範囲外 unit を実行時 internal error にしている。20.4.3 の違反として通常のエラーにする |

`ch13` は Icarus と Verilator の両方が期待値を再現するので、kirimasim のバグで確定です。

---

## 4. テスト側の誤り 40 本

### 4.1 規格の明文で確認した 39 本

| # | テスト | 節 | テストがしていること / 正しい扱い |
|---|---|---|---|
| 1 | `ch11/test_A_extra_specparam_range` | 6.20.5 | `localparam lo = sp[1:0]`。specparam は parameter/localparam の初期化式に使えない。コンパイルエラーが正しい |
| 2 | `ch17/test_checker_body_procedural_and_generate_items` | 17.5, 17.7.1 | checker の initial で `side = 7`。initial に置けるのは let・アサーション・イベント制御だけ |
| 3 | `ch17/test_checker_declaration_and_port_forms` | 17.5, 17.7.1 | `initial none_n = 1`、`initial empty_n = 2`、`initial y2 = y2 + 1`。いずれも不可 |
| 4 | `ch17/test_checker_rand_and_nested_checker` | 17.7.1 | `initial rf_out = rf`。checker 変数は initial で代入できない |
| 5 | `ch6/test_design_element_static_lifetime` | 17.5, 17.7.1 | checker の `initial cv = 6` と plain `always ... $display`。どちらも不可 |
| 6 | `ch23/test_scope_creators_all_kinds` | 17.5 | checker の `initial #9 $display`。# 遅延と $display は initial に置けない |
| 7 | `ch17/test_checker_assertion_placement` | 16.4.1 | deferred assertion の action が `def_fail = def_fail + 1`。action はサブルーチン呼び出しでなければならない |
| 8 | `ch17/test_checker_body_items` | 16.4.1 | 同上 |
| 9 | `ch17/test_checker_procedural_deferred_assertion` | 16.4.1 | 同上 |
| 10 | `ch17/test_checker_declaration_scope_kinds` | 24.3 | module から `p.hit` を読む。program の外から program signal を参照するのはエラー |
| 11 | `ch21/test_readmemh_address_range` | 21.4, 21.4.3 | `m[0:1][0:2]` に対する start=2, finish=5 は最上位次元の範囲外。kirimasim の警告が正しい |
| 12 | `ch20/test_fatal_form_finish_number_and_arguments` | 20.10 | regex が `fatal ... file:line:` の順を要求。書式はツール依存 |
| 13 | `ch20/test_fatal_form_finish_number_only` | 20.10 | 同上 |
| 14 | `ch20/test_fatal_form_without_parentheses` | 20.10 | 同上 |
| 15 | `ch20/test_severity_tasks_report_contents` | 20.10 | info/warn/error/fatal の順を要求。内容は一致している |
| 16 | `ch16/test_ch16_sim_clocking_single` | 16.16 | `.*multiclock` を要求するが、同じ行で「clocking block 内の明示クロック」のエラーが出る。どちらも正しい違反 |
| 17 | `ch11/test_conditional_predicate_and_integral_branch` | 12.6.3 | `(x matches ...)` を式として括弧で囲んでいる。matches 節は cond_pattern で、括弧で式にはできない。括弧を外す |
| 18 | `ch12/test_pattern_conditional_predicate` | 12.6.3 | 同上 |
| 19 | `ch12/test_pattern_match_known_result` | 12.6 | `matches` を一般式として代入している。predicate か `case ... matches` でのみ使える |
| 20 | `ch19/test_cg_bin_syntax_all_forms` | 19.5 | `bins wc = wildcard {...}`。`wildcard` は `bins` の前 |
| 21 | `ch23/test_syntax_port_expression_and_explicit_ports` | 23.2.2.2, Annex B | ポート名 `cell` は予約語。別名にする |
| 22 | `ch11/test_tagged_union_expression_and_matches_precedence` | 12.6 | `matches` を `$display` の式として使っている。predicate か `case ... matches` でのみ使える |
| 23 | `ch16/test_nexttime_always_eventually_alignment` | 16.13.1 | 異なるクロックのオペランドを `and` で結合している。`##1`/`##0` 以外は不可 |
| 24 | `ch16/test_triggered_matched_multiclock` | 16.13.1 | multiclock オペランドを `##[1:2]` で結合している。`##1`/`##0` 以外は不可 |
| 25 | `ch22/test_22_extra_include_syntax` | 22.4 | angle bracket の include を `:incdirs:` で探すことを期待している |
| 26 | `ch25/test_interface_extern_and_generate_items` | 25.3 | interface 内で module をインスタンス化している |
| 27 | `ch16/test_ch16_sim_checker_history` | 16.9.4 | `$future_gclk` を assertion の action block で呼んでいる |
| 28 | `ch16/test_sequence_formal_arguments_substitution` | 16.8.1 | `input int z` に `local` がない |
| 29 | `ch16/test_sequence_local_formal_assignment` | 16.8.1 | inout local formal の実引数 `w` がローカル変数でない |
| 30 | `ch25/test_virtual_interface_data_type_forms` | 25.9 | virtual interface をポートに使っている |
| 31 | `ch6/test_void_return_and_tagged_union_void_member` | 6.13 | `out` を方向キーワードとして使っている。正しくは `output` |
| 32 | `ch7/test_union_grammar_forms` | 7.3.1 | soft packed union に非整数の `shortreal` を入れている |
| 33 | `ch20/test_array_query_dimension_counts` | 20.7 | real/shortreal/event は配列でも整数でもなく、関数のドメイン外 |
| 34 | `ch20/test_array_query_functions` | 20.7 | 同上 |
| 35 | `ch20/test_array_query_invalid_dimension_all_x` | 20.7 | 同上 |
| 36 | `ch18/test_uniqueness_constraint_forms` | 18.5.4 | 一意性制約の変数が `rand` でない。randomize で値を割り当てるには `rand` が必要 |
| 37 | `ch25/test_interface_abstract_methods_and_broadcast` | 25.5 | modport にない `value` を `b.value` で読んでいる |
| 38 | `ch5/test_attribute_prefix_positions` | 6.5 | `wire ub;` の宣言前に `ub` を使っている |
| 39 | `ch16/test_A_extra_repetitions` | 16.12.22 | `1'b1[*]` を property に使っている。空マッチを含むので不可 |

### 4.2 推定 1 本

| # | テスト | 節 | 理由 |
|---|---|---|---|
| 1 | `ch17/test_checker_always_statement_kinds` | 17.5 | always_comb 内の `int acc;` が 17.5 の許可リストにない。ブロック宣言は「文」ではないとも読めるため、宣言を checker body に移す書き換えが安全 |

### 4.3 テスト側の修正を行うときの注意

CLAUDE.md の規則どおり、期待値・テストの変更は `src/` の変更と同じ PR に混ぜません。
修正 PR には、節番号と「なぜ今の期待値が規格に反するか」を書きます。
特に #2〜#5 は「合法なテストを違法系に作り替える」のではなく、
checker の静的寿命を別の書き方（宣言時初期化、`always_ff` 内の `$display` など）で
検査する形に作り直すのが望ましいです。

---

## 5. slang 111 本（上流）

### 5.1 位置づけ

`CLAUDE.md` は「フロントエンドは slang を pinned で無改造で使う」「slang の診断を緩めない」と定めています。
したがって、この 111 本は **kirimasim の `src/` では直せません**。選択肢は次の 3 つです。

1. slang 上流に Issue を出し、修正版の pin を取り込む。
2. テスト側が規格に反しているなら、テストを直す（§4）。
3. 未実装なら `unsupported` として分離する（fail にしない）。

### 5.2 家族別（多い順）

| 家族 | 件数 | 判断 |
|---|---:|---|
| 違法設計の受理（slang の検査欠落） | 44 | 44 本すべて規格本文で確認済み。うち 19 本は Icarus も期待位置で拒否 |
| uwire の inout を拒否 | 7 | slang が厳しすぎる（6.6.2 に inout 禁止なし。Icarus/Verilator も受理） |
| interconnect の参照を拒否 | 5 | slang が厳しすぎる（6.6.8 は端子接続・net_alias を許す） |
| user nettype の接続・force を拒否 | 5 | slang が厳しすぎる（スイートの Lyra レビューでテストが正しいと確認済み） |
| 整数→string の暗黙変換を拒否 | 5 | $fopen/$dumpvars/plusargs のキー。21.3.1 は integral を許す。slang が厳しすぎる |
| 配列クエリの string・定数範囲外 | 2 | 20.7。real の 3 本はドメイン外でテスト側（#18） |
| checker メンバーの階層読みを拒否 | 2 | 17.7.1 が禁じるのは代入のみ。$display での読みは合法。slang が厳しすぎる |
| sequence match item の非ローカル代入を拒否 | 2 | 16.10 は明示的に禁じていない。要確認（slang が厳しすぎる可能性） |
| string と整数の連結を拒否 | 2 | 11.4.12。スイートのレビューでテストが正しいと確認済み |
| stream 式の非 bit-stream 型を拒否 | 2 | 11.4.14.1。テストが正しいと確認済み |
| program のパラメータへの defparam/override を拒否 | 2 | 23.10.1 は program instance のパラメータを許す。slang が厳しすぎる |
| parser の取りこぼし（`)` `member` `;` `identifier` `?` など） | 6 | 構文。slang 上流の parser バグ。別の 5 本はテスト側の構文誤り（§4） |
| assertion の各種制限（match item、port direction、multiclock、empty match、clock 推論など） | 12 | 多くは slang の検査。個別に規格と照合（上の match item 2 本と重複） |
| ref 引数の型等価（`bit[7:0]` → `int`） | 2 | 13.5.2。正しい拒否の可能性。テスト側の確認が必要 |
| その他の単発 | 15 前後 | 1 件ずつ規格と照合（表は重複を含むため合計は 111 にならない） |

### 5.3 44 本の「違法設計の受理」

スイート自身が「どのシミュレータも期待位置で拒否できておらず、根拠は AI の検証だけなので
優先して見直す」と記録しています。今回、Icarus の結果と突き合わせて **19 本**は
Icarus が期待位置で拒否することを確認しました。残り 25 本も 2026-10-28 に規格本文で確認し、
**44 本すべてでテストが正しく、slang に検査が欠けている**と判断しました。
確認した節は台帳の `note` に 1 本ずつ書いています。

この 44 本は「slang の検査追加」なので、上流 Issue のまとめ単位としては
「構文・意味検査ごとに 1 Issue」が適切です。同じ検査で複数テストが通るものを優先します。

---

## 6. 改訂した優先順位

| 順 | 対象 | 件数 | 理由 |
|---|---|---:|---|
| 1 | ch33 config の `--top` 選択 | 8 | 1 根因で 8 本。実行時 5 + コンパイル 3 |
| 2 | ch11 の式（優先順位 2、条件演算子 2） | 4 | 言語の意味に直結。優先順位は 1 修正で 2 本 |
| 3 | ch23 defparam の順序・優先度 | 2 | 1 機構で 2 本。他の defparam テストにも波及 |
| 4 | ch16 deferred assertion の flush | 2 | 16.4 の保留キュー機構 |
| 5 | ch4/ch24 program の Reactive | 2 | 4.5 と 24.3 の実行領域 |
| 6 | ch9/ch12/ch7（queue の index 切捨て）/ch13/ch21 の単発 | 5 | 機構が独立。1 本ずつ |
| 7 | 診断 2 本（timeformat、queue の書き込み警告） | 2 | 安価 |
| 8 | テスト側 40 本 | 40 | `src/` と別 PR。規格の節を根拠に修正 |
| 9 | slang 上流 111 本 | 111 | 上流 Issue + pin 更新。ここでは直さない |
| 10 | unsupported 2,225 本 | — | 失敗ではない。実装時に pass へ移す |

### 期待される効果

- 1〜7 で **25 本**が通る（176 → 151）。
- 8 で **40 本**が正しいテストになる（151 → 111、実質は slang 上流待ち）。
- 9 のうち、まず over-strict の 30 本程度を上流に報告できれば、次の pin 更新で大きく減る。

---

## 7. 検証手順

家族単位の再実行:

```sh
python3 tools/run_kirimasim.py \
  --kirimasim build/release/src/driver/kirimasim \
  -j 8 --json /tmp/kirimasim-family.json \
  tests/ch33/test_config_*.sv
```

`--top` の根因確認:

```sh
kirimasim tests/ch33/test_config_cell_parameter.sv
kirimasim --top=test_config_cell_parameter tests/ch33/test_config_cell_parameter.sv
# 前者は u1=5 u2=5、後者は u1=1 u2=1 なら、この文書の根因どおり
```

回帰確認:

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

全件実行で回帰がないことを確認し、修正した家族・前後の件数・関連する規格節を
コミットメッセージに残します。

---

## 8. 注意

- **期待値を kirimasim の出力に合わせて書き換えないでください。**
  テストを直す場合は、規格の節・ブロック ID を根拠に expected を修正し、
  kirimasim-tests 側の台帳も更新します。
- 286 本の内訳は §0 のとおりです。268 本は formal、16 本は unsupported、2 本がテスト整備です（kirimasim-tests #13、#14）。
- 2,225 本の unsupported は失敗ではありません。`known-limitation` / `unsupported` として分離します。
- この文書の分類は AI によるものです。特に §4.2 と §5.3 の残り 14 本は、
  人が規格本文で確認してから修正してください。
- slang の pin を更新するときは、`cmake/Dependencies.cmake` の SHA と
  `docs/notes/slang-*.md` の記録を合わせて更新します。

---

## 9. 登録した Issue

この解析をもとに、次の Issue を登録しました（2026-10-28）。

### kirimasim（実行系・ドライバ）

| Issue | 内容 | 対象 |
|---:|---|---:|
| [#58](https://github.com/kurochan001/kirimasim/issues/58) | `--top` が config 選択を飛ばす | 8 |
| [#59](https://github.com/kurochan001/kirimasim/issues/59) | defparam がソース順でなく elaboration 順 | 2 |
| [#60](https://github.com/kurochan001/kirimasim/issues/60) | x/z 述語の `?:` が集約を要素ごとに合成しない | 1 |
| [#61](https://github.com/kurochan001/kirimasim/issues/61) | shortreal 分岐が単精度に丸まらない | 1 |
| [#62](https://github.com/kurochan001/kirimasim/issues/62) | deferred assertion の flush point がない | 2 |
| [#63](https://github.com/kurochan001/kirimasim/issues/63) | program が module の Active 更新より先に走る | 2 |
| [#64](https://github.com/kurochan001/kirimasim/issues/64) | always_comb が定数ビット選択でベクトル全体に感度を持つ | 1 |
| [#65](https://github.com/kurochan001/kirimasim/issues/65) | 全ループ変数を省略した foreach が 1 回回る | 1 |
| [#66](https://github.com/kurochan001/kirimasim/issues/66) | queue メソッドの index を integer に変換しない | 1 |
| [#67](https://github.com/kurochan001/kirimasim/issues/67) | 範囲外 queue index への書き込みで警告がない | 1 |
| [#68](https://github.com/kurochan001/kirimasim/issues/68) | 定数関数呼び出しが static ローカルの初期値を無視する | 1 |
| [#69](https://github.com/kurochan001/kirimasim/issues/69) | VCD `$scope` に named fork が出ない | 1 |
| [#70](https://github.com/kurochan001/kirimasim/issues/70) | 範囲外 `$timeformat` unit が internal error / exit 3 | 1 |
| [#54](https://github.com/kurochan001/kirimasim/issues/54) | `?:` が `->` より下にグループ化される（既存） | 2 |

### kirimasim-tests（テスト側）

| Issue | 内容 | 対象 |
|---:|---|---:|
| [#6](https://github.com/kurochan001/kirimasim-tests/issues/6) | checker の initial 代入・always_comb 内宣言 | 5 |
| [#7](https://github.com/kurochan001/kirimasim-tests/issues/7) | deferred assertion の action が代入 | 3 |
| [#8](https://github.com/kurochan001/kirimasim-tests/issues/8) | severity / `$fatal` の regex が語順依存 | 5 |
| [#9](https://github.com/kurochan001/kirimasim-tests/issues/9) | readmemh の start/finish を線形アドレスとみなす | 1 |
| [#10](https://github.com/kurochan001/kirimasim-tests/issues/10) | specparam / program signal / checker delay | 3 |
| [#15](https://github.com/kurochan001/kirimasim-tests/issues/15) | parser 群など 6 本が文法に反する構文を使っている | 6 |
| [#16](https://github.com/kurochan001/kirimasim-tests/issues/16) | §4.3 の 7 本が規格の禁止する構文・意味を使っている | 7 |
| [#17](https://github.com/kurochan001/kirimasim-tests/issues/17) | §4.3 の 6 本が規格の禁止する構文・意味を使っている | 6 |
| [#18](https://github.com/kurochan001/kirimasim-tests/issues/18) | 配列クエリに real/shortreal/event を渡している | 3 |
| [#19](https://github.com/kurochan001/kirimasim-tests/issues/19) | `1'b1[*]` を property に使っている | 1 |

### slang（上流待ち）

| Issue | 内容 | 対象 |
|---:|---|---:|
| [#71](https://github.com/kurochan001/kirimasim/issues/71) | uwire の inout を拒否 | 7 |
| [#72](https://github.com/kurochan001/kirimasim/issues/72) | interconnect の端子・net_alias・入れ子ポートを拒否 | 5 |
| [#73](https://github.com/kurochan001/kirimasim/issues/73) | user nettype の接続・force・部分ドライブを拒否 | 5 |
| [#74](https://github.com/kurochan001/kirimasim/issues/74) | 整数→string の暗黙変換を拒否 | 5 |
| [#75](https://github.com/kurochan001/kirimasim/issues/75) | real / dynamic 型の配列クエリを拒否 | 5 |
| [#76](https://github.com/kurochan001/kirimasim/issues/76) | program / block / task / function のパラメータを拒否 | 3 |
| [#77](https://github.com/kurochan001/kirimasim/issues/77) | checker の階層読み・covergroup・空ポートを拒否 | 5 |
| [#78](https://github.com/kurochan001/kirimasim/issues/78) | parser の取りこぼし | 11 |
| [#79](https://github.com/kurochan001/kirimasim/issues/79) | プリプロセッサ・top・package・config の検査欠落 | 16 |
| [#80](https://github.com/kurochan001/kirimasim/issues/80) | interface / modport の検査欠落 | 7 |
| [#81](https://github.com/kurochan001/kirimasim/issues/81) | covergroup の検査欠落 | 4 |
| [#82](https://github.com/kurochan001/kirimasim/issues/82) | net・型・定数・force の検査欠落 | 17 |
| [#83](https://github.com/kurochan001/kirimasim/issues/83) | その他の過剰検査（要分割） | 44 |
| [#98](https://github.com/kurochan001/kirimasim/issues/98) | unpacked union への `$bits` | 1 |
| [#99](https://github.com/kurochan001/kirimasim/issues/99) | interface class のメソッド実装に `virtual` を要求 | 1 |

新規 51 Issue（既存 #54 を含めると 52）で 176 本をカバーします。

### 残り 14 本（needs-review）

| Issue | 内容 | 対象 |
|---:|---|---:|
| [#85](https://github.com/kurochan001/kirimasim/issues/85) | `[*]` の空マッチ | 1 |
| [#86](https://github.com/kurochan001/kirimasim/issues/86) | `##1` 後の再帰 property | 1 |
| [#87](https://github.com/kurochan001/kirimasim/issues/87) | 自動 task 内の concurrent assertion | 1 |
| [#88](https://github.com/kurochan001/kirimasim/issues/88) | match item の非ローカル代入 | 2 |
| [#89](https://github.com/kurochan001/kirimasim/issues/89) | 定数式の `$timeunit` | 1 |
| [#90](https://github.com/kurochan001/kirimasim/issues/90) | task 呼び出しの `ref` 実引数 | 1 |
| [#91](https://github.com/kurochan001/kirimasim/issues/91) | string → unpacked byte array の cast | 1 |
| [#92](https://github.com/kurochan001/kirimasim/issues/92) | 定数の範囲外 array index | 1 |
| [#93](https://github.com/kurochan001/kirimasim/issues/93) | `local::` の名前解決 | 1 |
| [#94](https://github.com/kurochan001/kirimasim/issues/94) | constraint 内の string 比較 | 1 |
| [#95](https://github.com/kurochan001/kirimasim/issues/95) | derived covergroup の cross 再定義 | 1 |
| [#96](https://github.com/kurochan001/kirimasim/issues/96) | cross select 式 | 1 |
| [#97](https://github.com/kurochan001/kirimasim/issues/97) | state-dependent path の module path 式 | 1 |

### 286 本の分類（176 本の外）

| Issue | 内容 |
|---:|---|
| [#13](https://github.com/kurochan001/kirimasim-tests/issues/13) | simulation runner が `:type: formal` を実行してしまう（268 本） |
| [#14](https://github.com/kurochan001/kirimasim-tests/issues/14) | `no :expected_stdout:` を unsupported より先に判定してしまう（18 本） |
| [#84](https://github.com/kurochan001/kirimasim/issues/84) | (slang) covergroup の bin set 式で coverpoint 名が外側のパラメータに解決されない（1 本） |

### 1 本ずつの台帳

- [トリアージ台帳](kirimasim_failure_triage_2026-10-28.md): 176 本を責任・Issue・根拠の強度で分類
- [CSV](kirimasim_failure_triage_2026-10-28.csv): 同じ内容を表計算・スクリプトで扱う用

台帳では `confirmed` 168 本、`likely` 8 本、`needs-review` 0 本です。
`likely` の 8 本は、規格本文を人が確認するまで slang のバグと決めないでください。

2026-10-28 に、違法設計の受理 44 本のうち Icarus が確認できなかった 25 本も
規格本文で確認しました。44 本すべてテストが正しく、slang に検査が欠けています。
