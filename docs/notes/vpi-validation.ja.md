# P1 VPIの最初の検証

[English](vpi-validation.md)

2026-11-01、macOS arm64で測定しました。[機械可読の記録](../measurements/2026-11-01/vpi-execution.json)に
ケースの結果、ソース・binaryのhash、構成ごとの結果、コンパイル変異を記載します。
[実装と境界](vpi-execution.ja.md)も参照してください。

## 実行とABI

- native CLI：**109/109件**。不変の参照23入力、**70件**の行列（14ソース文脈×5操作）、
  寿命・loader・エラー・コンパイルモード16件です。独自のC pluginを本体のヘッダでコンパイルして実行します。
- kernel/frontend：**32件**。登録・呼出し箇所、scalar・集約ref、一度だけの評価、宣言範囲、escaped名、
  Contextの復元、不正handle、payloadのコピー、取り消し、X/Z比較、再入、制限、force/release、
  読み取り専用の書込み、寿命切れbindingを検査します。
- 独立したC11のABI assertionを、本体のヘッダとインストール済みIcarus 13.0の公開SDKの両方で
  コンパイルしました。サイズ・offset・定数・一部の関数型を検査し、登録記述子のconstも確認しています。
  [ABIの観測記録](../measurements/2026-11-01/vpi-abi.json)に数値を残します。
  公開ヘッダのC++20ビルドも成功しています。他シミュレータの実装ソースは読んでいません。

## 構成ごとの確認

ソース改定`405334dc`で、最終の実装の回帰が成功しました。

- dev：**11,354/11,354件**、失敗・skipなし。
- release・ASan・UBSan：**関連163/163件ずつ**、失敗・skipなし。
  native CLIと、記録したcohortが選ぶ既存のscheduler・activation・class・ref・pattern・CLIの検査を含みます。

続くテストだけの変更でContextの解体を強化しました。終了callbackが解放前に一度だけ実行され、
ライブラリが解体まで生存し、逆順で閉じられることを確認します。その後、構成の版情報を更新し、
**VPIとCLIオプションの34検査**を4構成で再実行し、全て成功しました。機械記録には両方のbinary識別情報を残し、
古いcacheの版表示をソースの識別には使いません。ソースのhashと改定が実装を特定します。

既存suiteの本体、stdout期待値、PerfCount期待値、固定P1母集団は変更していません。
件数は成功したdev全件JUnitから別に更新します。全local_ci、Linux、他3構成の全件回帰、
外部比較、nightly fuzzの連続成功は再測定していません。

## コンパイル変異

**10/10件を実コンパイルして検出**しました。変更前・復元後のcohortが成功し、ソースはbyte単位で
完全復元しています。入れ子Contextの復元、compiletfの配置、bvalの比較、読み取り専用の保護、
持続するforce、遅延payloadの所有、Post-NBAの配置、引数の一度だけの評価、一時bindingの切離し、
nullable参照をVPI内へ閉じ込める検証を、それぞれ壊すものです。

最初のnullable参照fixtureには位置情報の不足もあり、変異が検出されませんでした。修正したテストは
位置を設定し、通常の参照へ変えた同等入力が検証を通ることと、専用のescape診断を要求します。
そのコンパイル変異を再実行して検出しました。ビルド失敗を検出に数えません。
構成検査とソース変異は別に実行します。並行した変異が混ざった途中のASan結果は破棄し、
上記の正常なソースでの結果に置き換えました。

clang-format、対象clang-tidy（LLVM 23.1.4）、global-state probe、releaseフラグ、禁止API、SPDX、
期待値ファイル対応、変異アンカー、コミット単位の期待値保護を検査しました。
この有限のVPIの根拠で、VPI全体・Icarusとの同等性、cocotb連携、P1完了を示すものではありません。
