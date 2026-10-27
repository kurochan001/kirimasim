# イベントNBAの過去の拒否期待を移行する提案

[English](event-nba-expectation-migration.md)。状態：**承認・適用済み（2026-10-27）**。
人が承認した3ファイルだけのパッチを期待値専用コミット`16c033a4`で適用しました。
既存ソースの全バイトは不変で、そのコミットには`src/`の変更やpushを含みません。
以下の元の提案は意思決定の記録として維持し、承認依頼や未来形は承認前の状態を示します。
実装と統合の検証は継続中です。

## 理由と根拠

IEEE 1800-2023 9.4.5、10.4.2と承認済みの
[イベントNBA契約](event-nba-proposal.ja.md)は、発行元を待たせない代入内イベント制御を
扱います。ローカルのソース変換はmainの型付きSIR/VMの区切り`6f43e387`と共有kernelを
使います。独自の新規ソース行列1,018設計が直接実行で通りました。実行場所10種類、
書き込み形状11種類、制御8種類、値と回数の評価順、全幅の回数、新しいcallback領域、
発行元のRNGと呼び出し深さ、return/disable、無効な代入先、保留順、program終了を
含みます。イベントNBA全体やP1/Icarusの完全な同等性の証拠ではありません。
OR・発行順・RNGの意図したプロジェクト規則も含め、参照実行との差は
[ソース比較記録](../measurements/2026-10-27/nba-front-source-comparison.json)に保存しています。

既存の`tests/cli/unsupported_matrix/nonblocking_event.sv`は、initial、always、
static task、automatic task、fork、generateの6代入を含みます。入力は変更せず、現在は
終了コード0、標準出力なし、情報以外の診断なしで終了します。triggerは発生せず、保留制御は
自然終了を妨げません。最初のdev全体実行の失敗は、古い拒否期待6行による`cli/options`と、
件数再生成前の`tools/readme_counts`の2件だけです。新規ソース行列、単体、ダンプ、
PerfCountは通りました。故障検出とsanitizerの最終検証は別途継続し、その結果で実装を評価します。

## 承認してほしい変更の全範囲

人の承認後、[確認用パッチ](event-nba-expectation-migration.patch)だけを、`src/`とは
別の期待値専用コミットで適用します。

1. 未対応行列generatorの`ACCEPTED`に`nonblocking_event`だけを追加します。
   生成version 19と既存ソースの全バイトを維持します。
2. `tests/cli/unsupported_matrix.accepted`を再生成し、その名前1つを追加します。
3. `tests/cli/unsupported_matrix.stderr`を再生成し、7:5、10:5、13:5、17:5、22:7、
   27:7の古い診断6行だけを削除します。
4. CLI runnerは変更しません。受理後も終了コード0、標準出力なし、情報以外の診断なしを
   検査します。他の期待行・入力・スキップ・PerfCount予算は維持します。
5. 移行後にCLI全体、generator、回帰検査を実行します。

[AI方針](../spec/AI_POLICY.md#rules-that-protect-expected-values)には
“A change of an expected value states its reason and the clause of the standard, and a human approves it.”
とあります。この承認範囲は過去の期待値の移行だけです。新しい意味、slangの厳格な診断の緩和、
依存の追加、push、P1完了の承認ではありません。
