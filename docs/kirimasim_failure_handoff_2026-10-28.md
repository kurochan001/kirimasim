# kirimasim 全テスト実行と失敗分析 — 引き継ぎ

作成: 2026-10-28 / 対象: `main` `a2420294`（`kirimasim 0.0.0`）
作成者: AI（DeepSeek V4.1 Flash）。人による独立レビューはまだありません。

---

## 0. TL;DR（次の一手）

- kirimasim-tests の全 **7,740** 本を最新 kirimasim で実行し、**5,053 本が一致**、**2,225 本が未実装（unsupported）**、**462 本が fail** でした。
- fail のうち **286 本は `:expected_stdout:` を持たないテスト**で、シミュレータの失敗ではありません。実際に対処すべき fail は **176 本**です。
- 実行可能な分母（7,740 − 2,225 unsupported − 286 期待出力なし = **5,229 本**）に対する一致率は **96.63%** です。
- 176 本の内訳は、**コンパイルエラー 104**、**違法設計の受理 44**、**実行時出力の不一致 21**、**診断文・終了コード 7** です。
- 最優先は **ch33 の config（実行時5本）**、**ch11 の real/条件演算子（4本）**、**ch16/ch17 の checker・program・assertion（コンパイル26本）**、**ch23 の uwire/interconnect（コンパイル）** です。

---

## 1. 実行環境

| 項目 | 値 |
|---|---|
| kirimasim | `main` `a2420294`（2026-10-28、`kirimasim 0.0.0`、Clang 20.1.8） |
| ビルド | `build/release/src/driver/kirimasim` |
| テストスイート | kirimasim-tests `main`（7,740 本） |
| 実行 | `tools/run_kirimasim.py`、16 並列、約 38 秒 |
| 結果 | `/tmp/opencode/kirimasim-final.json`、`/tmp/opencode/kirimasim-final.log` |

再実行:

```sh
python3 tools/run_kirimasim.py \
  --kirimasim <kirimasim>/build/release/src/driver/kirimasim \
  -j 16 --json /tmp/kirimasim.json
```

ランナーの扱い:

- `error: unsupported:` を返したテストは **unsupported**。
- `:dpi_c:` / `:vpi_c:` を持つテストは、kirimasim が C をロードできないため **unsupported**。
- `:expected_stdout:` を持たないテストは **fail（no :expected_stdout:）** になるが、シミュレータの失敗ではない。
- 違法設計は `:expected_stderr_regex:` の位置にエラーが出れば pass。

---

## 2. 結果

| 状態 | 件数 | 意味 |
|---|---:|---|
| pass | **5,053** | 期待出力と一致 |
| unsupported | **2,225** | kirimasim が未実装、または C コード |
| fail | **462** | 不一致 |
| うち期待出力なし | 286 | テスト側の整備対象 |
| **実際の fail** | **176** | ここを直す |

実行可能分母 5,229 本に対する一致率: **96.63%**。

### 失敗の内訳（176本）

| 区分 | 件数 | 主な意味 |
|---|---:|---|
| コンパイルエラー（exit 2） | **104** | 合法なコードを拒否、または診断がテストの期待と違う |
| 違法設計の受理 | **44** | コンパイル時検査の欠落 |
| 実行時出力の不一致 | **21** | 値・順序・時刻の違い |
| 診断文・終了コード | **7** | `$fatal` の文、severity、exit code |

### 章別（実際の fail 176本）

| 章 | 件数 | 主な内容 |
|---|---:|---|
| ch23 | 20 | uwire/inout、interconnect、defparam、パラメータ型 |
| ch16 | 18 | checker、sequence/property、deferred assertion |
| ch33 | 16 | config の binding・パラメータ・library/cell |
| ch6 | 15 | interconnect、型・宣言 |
| ch20 | 13 | `$dimensions(real)`、`$fatal`、severity |
| ch11 | 12 | real/条件演算子、連結、stream、specparam |
| ch17 | 12 | checker 本体、program item、assertion 配置 |
| ch21 | 9 | readmemh、VCD scope、plusargs の string |
| ch25 | 9 | interface/modport の違法系 |
| ch7 | 8 | queue の境界、ref 引数 |
| ch19 | 7 | covergroup の違法系 |
| ch22 | 7 | プリプロセッサの違法系 |
| その他 | 30 | ch5/ch12/ch18/ch24/ch4/ch9/ch10/ch13 など |

### unsupported 2,225本（失敗ではない）

| 章 | 件数 | 主な内容 |
|---|---:|---|
| ch37 | 227 | VPI |
| ch8 | 196 | class/interface の未実装機能 |
| ch16 | 184 | SVA の未実装機能 |
| ch7 | 158 | 配列・キュー・ロケータの未実装 |
| gen | 136 | 生成テスト（強度 `%v` など） |
| ch18 | 119 | 制約・ランダムの未実装 |
| ch35 | 117 | DPI（C をロードできない） |
| ch38 | 109 | VPI |
| ch19 | 103 | covergroup |
| ch36 | 87 | VPI |

これらは「kirimasim がまだ実装していない」もので、fail とは分けて扱います。`%v` のように「未実装」と明示されるものは、テストを fail に数えない運用が妥当です。

---

## 3. 失敗クラスタと修正方針

### 3.1 コンパイルエラー（104本）

まず「合法なコードを拒否している」のか、「違法だが診断が違う」のかを、引用節とテスト本体で判定します。以下は多い順の家族です。

| エラー（要旨） | 件数 | 例 | 判定のポイント / 方針 |
|---|---:|---|---|
| `statement is not allowed in 'initial' procedure inside a checker` | 4 | `ch17/test_checker_body_procedural_and_generate_items` | checker 本体で許される文か（17.7）。合法なら frontend 修正 |
| `'uwire' port 'w' cannot have direction 'inout'` | 4 | `ch23/test_23_extra_uwire_dominates` | uwire に inout を許すか（23.3.3）。合法なら修正、違法ならテスト修正 |
| `deferred assertion action must be a subroutine call` | 3 | `ch17/test_checker_assertion_placement` | deferred assertion の action 制限（16.4）。合法形か確認 |
| `cannot reference program item from outside of a program` | 3 | `ch17/test_checker_declaration_scope_kinds` | program item の可視性（24.3）。合法なら修正 |
| `invalid argument type 'real' to system function` | 3 | `ch20/test_array_query_dimension_counts` | `$dimensions(real)` は 0 を返すべき（20.7）。**bug 候補** |
| `cannot mix strings and integers in a concatenation` | 2 | `ch11/test_concatenation_pattern_string_operand_and_replication_once` | 暗黙変換の可否（11.4.12/5.9）。合法なら修正 |
| `expected ')'` / `expected member` / `expected ';'` / `expected identifier` / `expected '?'` | 有 | `ch12/test_pattern_conditional_predicate` ほか | 合法な構文の parser 取りこぼし。**parser bug 候補** |
| `cannot access checker members hierarchically` | 2 | `ch16/test_ch16_sim_checker_inference` | checker メンバーの階層参照（17.7）。合法なら修正 |
| `sequence match items can only modify local variables` | 2 | `ch16/test_sequence_match_item_and_triggered_method` | match item の代入先（16.10）。合法形か確認 |
| `no implicit conversion from 'bit[63:0]' to 'string'` | 2 | `ch21/test_plusargs_key_from_variable` | plusargs の key に string を要求（21.6）。テストが合法か確認 |
| `cannot reference interconnect net 'ic' outside of a net port connection` | 2 | `ch6/test_a2_1_3_interconnect_declaration_identifiers` | interconnect の利用制限（6.6.8）。合法なら修正 |
| `argument ... connects to 'ref' port of inequivalent type` | 2 | `ch7/test_assoc_narrow_prev_next_overflow` | ref の型等価（13.5.2）。正しい拒否の可能性。テスト側を確認 |
| `stream expression type ... is not a bit-stream type` | 2 | `ch11/test_stream_item_order_struct_union_object_assoc` | tagged union/class の stream 可否（11.4.14）。要確認 |
| `specparams cannot be used in general constant expressions` | 1 | `ch11/test_A_extra_specparam_range` | specparam の定数式利用（6.20.2）。要確認 |
| `value of type ... is invalid for '%d'` | 1 | `ch11/test_tagged_union_expression_and_matches_precedence` | tagged union の `%d` 表示。要確認 |
| `sequence must not admit an empty match` | 1 | `ch16/test_A_extra_repetitions` | 空マッチの制限（16.12）。正しい拒否か確認 |
| その他の単発 | 約 | assertion の clock、再帰、port 方向など | 1件ずつ規格と照合 |

**修正方針:**

1. まず「テストが規格上合法か」を判定する。合法なら kirimasim の frontend を修正し、違法ならテスト／期待値を修正する。
2. parser 系（`expected ...`）は、同じ構文を共有するテストをまとめて1 Issue にする。
3. checker/program/assertion 系（ch16/ch17）は、frontend の semantic 検査を1機能ずつ分ける。
4. 「正しい拒否だが診断が違う」ものは、期待値ではなく診断文・終了コードを合わせる（またはテストの regex を緩める）。

### 3.2 違法設計の受理（44本）

コンパイル時検査の欠落です。章別では **ch25 6、ch33 6、ch22 5、ch19 4、ch6 4、ch20 3、ch23 3、ch5 3、ch7 2** など。

代表例:

- ch25: `test_illegal_generic_interface_port_dot_name`、`test_illegal_interface_port_actual_through_generate_block`、`test_illegal_modport_import_default_argument_without_prototype`
- ch33: config の階層名・index・instance array path など
- ch22: `begin_keywords` の閉じ忘れ、`include` の絶対パス、`line` の末尾コメント
- ch19: covergroup の関数副作用、外側定数、ref formal
- ch6: interconnect の宣言・利用制限
- ch20: `$bits` の動的戻り、`$dist_exponential` の平均0、`$isunbounded` の変数

**修正方針:**

- 構文・意味検査ごとに1 Issue。同じ検査で複数テストが通るものを優先。
- `:expected_stderr_regex:` の位置にエラーが出ることを確認。
- 「未実装だから受理」と「検査を忘れて受理」を分ける。未実装なら `error: unsupported:` を返して unsupported にするのが望ましい。

### 3.3 実行時出力の不一致（21本）

| テスト | 最初の差分（actual / expected） | 分野 |
|---|---|---|
| ch33 の config 5本 | `u1=1` / `u1=5`、binding が子に降りない、library/cell 名など | config の実装 |
| ch11 `test_conditional_real_shortreal_branch_promotion` | `16777217.0` の精度 | real/shortreal の型昇格 |
| ch11 `test_expr_conditional_ambiguous` | `arr=1,2` / `arr=1,0` | 条件演算子の型決定 |
| ch11 `test_precedence_*` | `... -> 0 = 0` / `= 1` | 条件・含意の優先順位 |
| ch16 `test_deferred_flush_points` | `latch fail` / `latch pass` | deferred assertion の flush |
| ch16 `test_syntax_deferred_immediate_assertions` | `wrong` / `0 assert zero sees 5` | deferred immediate assertion |
| ch21 `test_readmemh_address_range` | `m[0][2]=ee` / `=11` | readmemh のアドレス順 |
| ch21 `test_vcd_scope_commands_begin_fork_task` | fork scope が 0 | VCD scope |
| ch23 `test_defparam_last_in_source_text_wins` | `m.A=1` / `=2` | defparam の優先順位 |
| ch23 `test_parameter_override_type_and_range` | `R=3/4` の判定 | パラメータ型・範囲 |
| ch24 `test_program_port_reactive_variable_and_net` | `ov=1 iv=1 iw=0 mw=0` | program port |
| ch4 `test_reactive_region_set_order` | `program: v changed` / `module: ...` | Reactive 領域 |
| ch7 `test_queue_method_prototypes_and_invalid_indices` | `insert(...): size=5` | queue の境界 |
| ch9 `test_always_comb_longest_static_prefix_and_locals` | `y_vec=0` / 期待値 | always_comb の感度 |
| ch12 `test_syntax_loop_statement_forms` | `sum=21 count=6` / 期待値 | foreach |
| ch13 `test_constant_function_call_fresh_initial_values` | `P1=1 P2=2` / `P1=11` | 定数関数の初期値 |

**修正方針:**

- **ch33 config の5本は1根因**の可能性が高い。binding・パラメータ・library/cell のどこで落ちるかを最小化し、1 Issue にまとめる。
- ch11 の real/条件演算子は、式の型決定（11.6/11.8）を1か所で直せる可能性。優先度 high。
- 実行時出力は「値の差」なので、期待値を kirimasim に合わせず、規格の式から再計算する。
- ch21 の readmemh と VCD scope は、ファイル I/O・波形の独立した機能として分ける。

### 3.4 診断文・終了コード（7本）

- `$fatal` のメッセージ・引数（ch20 4本）: severity の出力形式と `finish_number`。
- `test_severity_tasks_report_contents`（ch20）: severity の内容。
- `test_ch16_sim_clocking_single`（ch16）: multiclock のエラー位置・文言。
- `test_queue_invalid_index_write_ignored_warning`（ch7）: 警告の有無。
- `test_illegal_timeformat_unit_out_of_range`（ch20）: exit code 3 / expected 2。

**修正方針:** 診断文はテストの regex と照合し、位置（file:line）を優先して合わせる。終了コードは規格とテストの意図を確認し、必要ならテスト側の `:expected_exit:` を修正する。

---

## 4. 優先順位

1. **実行時出力 21本**（機能的な誤り）  
   ch33 config 5 → ch11 real/条件 4 → ch16 deferred 2 → ch21 readmemh/VCD 2 → ch23 defparam/parameter 2 → その他。
2. **コンパイルエラー 104本**（合法コードの拒否）  
   parser 系 → checker/program/assertion（ch16/ch17）→ uwire/interconnect/ref（ch23/ch6/ch7）→ real array query（ch20）。
3. **違法設計の受理 44本**  
   構文・意味検査ごとに1 Issue。ch25/ch33/ch22 の順。
4. **診断文・終了コード 7本**  
   安価なので、機能修正の合間に片付ける。
5. **unsupported 2,225本**  
   失敗ではなく既知の未実装。`docs/` の対応表に残し、実装時に unsupported から pass へ移す。

### 章ごとの着手順（目安）

| 順 | 章 | 対象 | 理由 |
|---|---:|---:|---|
| 1 | ch33 | 16 | config の実行時5本が1根因の可能性 |
| 2 | ch11 | 12 | real/条件演算子の型決定 |
| 3 | ch16/ch17 | 30 | checker/program/assertion の機能ブロック |
| 4 | ch23 | 20 | uwire/inout、interconnect、defparam |
| 5 | ch20 | 13 | `$dimensions(real)`、`$fatal`、severity |
| 6 | ch6/ch7/ch21 | 32 | interconnect、queue、ファイル I/O |
| 7 | 残り | — | 単発・違法系 |

---

## 5. 検証手順

1. 家族単位で再実行:

```sh
python3 tools/run_kirimasim.py \
  --kirimasim <kirimasim>/build/release/src/driver/kirimasim \
  -j 8 --json /tmp/kirimasim-family.json \
  tests/ch33/test_config_*.sv
```

2. 前回 JSON と比較して、改善と回帰を確認:

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

3. 全件実行で回帰がないことを確認。
4. 修正した家族・前後の件数・関連する規格節をコミットメッセージに残す。

**期待値を kirimasim の出力に合わせて書き換えないでください。** テストを直す場合は、規格の節・ブロックIDを根拠に expected を修正し、kirimasim-tests 側の台帳も更新します。未実装は `known-limitation` / `unsupported` として分離します。

---

## 6. 成果物と注意

- 実行結果: `/tmp/opencode/kirimasim-final.json`（status/reason のみ）
- ログ: `/tmp/opencode/kirimasim-final.log`
- 今回の分析は AI によるもので、人による独立確認はまだありません。
- 失敗の分類は「コンパイルエラー」「違法受理」「実行時差」「診断」の4種類に限定しています。個々のテストが規格上合法かどうかは、修正前に必ず引用節で確認してください。
- 286本の `no :expected_stdout:` はシミュレータの失敗ではありません。テスト整備の別タスクとして扱ってください。
