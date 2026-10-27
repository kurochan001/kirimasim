# ソースからのイベントNBA変換

[English](event-nba-source-execution.md)。状態：ローカルのソース対応の区切りを検証済み、2026-10-27。
main `81799982`が基です。過去の拒否期待は[承認後に移行](event-nba-expectation-migration.ja.md)し、
期待値専用コミット`16c033a4`に分けました。
イベントNBA全体やP1の完全な同等性を示すものではありません。

## 規則と実装場所

承認済みの[イベントNBA契約](event-nba-proposal.ja.md)、
[仕様への対応](p1-execution-contracts-2026-10-25.ja.md)、mainの
[型付きSIR/VM](event-nba-sir-execution.ja.md)に従います。背景の規格節は
IEEE 1800-2023 9.4.2、9.4.5、10.4.2、11.4.14、13.4.4、15.5、18.14です。
規格本文・例や外部シミュレータのコード・テストは写していません。
依存、ABI版、SIRパーサ、詳細仕様の変更は追加しません。

`Lowerer::lowerEventNba`が右辺、全幅・型付きのrepeat回数、代入先の選択式の順で保持します。
`captureNbaWrite`は既存の型付き`Place`を配置表の対象へ変換します。集約値とストリーミングは
通常代入と共通の値保持・型変換・分解を使います。`finishEventNba`は文全体で1つの配置表を
作り、`sir::nbaWrites`で保守的な書き込み範囲を記録します。forkや葉ごとの回数制御にはしません。
0以下・X/Zの回数なら名前付きeventの選択式を評価しません。既存の4値比較と条件分岐で経路を
選び、元の全幅の回数をそのまま共有kernelの正規化へ渡します。

guardとwatchは`waiterOf`の独立モードで作ります。通常のautomatic・非待機SIR functionを
使い、元の字句スコープを保ち、発行元frameの変数表を借りません。新しいprivate領域、RNG、
ソース呼び出し深さ、通知、回数、保持値のルートと寿命は既存のVM/kernel境界を使います。
今回VMにはソース形状や実行場所による分岐を追加しません。module、task、function、fork、
interface、package、program、generate、class staticメソッドはすべて同じ変換関数へ進みます。

slangの厳格な診断と既存の条件の純粋性制限は維持します。制御からautomatic変数を参照する
制限も維持します。[automatic入力引数をrepeat回数にする独自例](../measurements/2026-10-27/nba-front-automatic-count-boundary.json)は、固定slangと手元の
Icarus 13.0が拒否し、14.0は受理しました。既定の診断を緩める根拠にはしません。

## 独自検証と測定範囲

`tools/gen_event_nba_matrix.py`は独自ソース**1,018設計**を追加します。
場所10×書き込み11×制御8の880件、回数と副作用80件、event選択の保持と寿命4件、
キュー・領域・終了8件、広い回数8件、RNG/深さ8件、集約値の評価順20件、disable 6件、
全guardの順序と無効な対象3件、独立した回数1件です。すべて新規のstdout/stderr期待値を持ち、
`spec_check: ai`です。既存のソース・期待値・スキップ・PerfCount予算は変更しません。

1,018件すべてが直接実行で成功しました。最初のdev全体実行は新規1,018件とツール2件を
含み、失敗は古い拒否期待とREADME件数だけです。件数は実際のJUnitから再生成します。
独立した契約ツールで、1制御/3書き込み、発行元frameを借りないcallback、イベント待機や
forkへの展開がないこと、`--max-procs=1`での実行を確認します。単体全件と既存ダンプ29件も
期待値を維持したまま通っています。

[コンパイルした6故障](../measurements/2026-10-27/nba-front-controls.json)をすべて検出しました。
回数の切り詰め、省く制御の名前付き選択式の評価、集約値の先行する葉の欠落、対象選択後の
回数評価、callbackのautomatic領域の欠落、guardの省略です。ソース全バイトを復元し、
再ビルド後も1,018件が成功しました。全mutationの実行ではありません。

[固定した比較](../measurements/2026-10-27/nba-front-comparison.json)で、母集団は
**1,456/1,466**、失敗0・未対応10、sv-testsは**244/344**を維持し、以前の成功を失いません。
2版それぞれの固定95プローブでは`event_controlled_nba`だけが実行失敗から成功へ変わりました。
その有限の一覧の残差は6件で、P1全体の残作業数ではありません。

[新規ソースの比較](../measurements/2026-10-27/nba-front-source-comparison.json)は同じ1,018入力を
手元のIcarus 2版で実行します。13.0は132件がコンパイルでき、44件が期待出力と一致しました。
14.0は138件がコンパイルでき、47件が一致しました。コンパイル失敗と出力差は保存しています。
ORのまとめ方、値/回数/対象の評価順と保留順、RNG、programの寿命は承認済みのプロジェクト規則で、
Icarusの出力から規則を推測したものではありません。

[独立した切り分け60件](../measurements/2026-10-27/nba-front-isolation.json)はnativeですべて成功し、
参照の観測を保存します。この入力では両版とも、XOR/引数付きfunctionのイベント式では最初の変更後に
代入し、引数なしfunctionの監視では0のままでした。automatic taskから戻るとslot制御の結果が0のままで、
taskを生かすと2通知後に代入します。両版はこの観測で一致します。内部の原因や規格解釈全体は**未確認**です。
挙げた規格節の確認と独自の通知/依存プローブの拡張で確かめます。外部実装のソースは読まず、
説明できていない差を同等性の達成には数えません。

## 残作業

過去のCLI拒否期待の移行は承認済みで、`16c033a4`として適用しました。
[最初の全体検証](../measurements/2026-10-27/nba-front-initial-validation.json)は古い期待値の失敗と、
optional参照4か所・vector事前確保1か所のlint指摘を保存します。明示的なoptional確認と
選択式の領域予約で直し、修正したソースで故障検出と全CIを再実行しました。
[最終検証](../measurements/2026-10-27/nba-front-validation.json)は全21項目が成功し、
dev/ASan/UBSanは8,129/8,131、releaseは8,130/8,132、失敗0・同じ既知スキップ2件です。
単体773件と既存ダンプ29件も成功しました。最終ソース比較のbinary hashは最終devと一致します。
実装・テスト・文書はローカルに保持し、未コミットです。過去のnativeの休眠制御ベンチマークからソースevent-NBAの速度は主張しません。
適格なstaticの単一値/コンテナ、固定集約値、packed選択、連結、ストリーミングを検査しますが、
全組み合わせではありません。ref経由の配列要素・入れ子のpacked窓、event値の一般の再関連付け/戻り値、
managed-event保持値の差し替え、字句activation/回収の全行列は未対応または未確認です。
一般のevent再関連付けは元から未対応で、新規fixtureでは対応済みの実行時event配列選択を使います。
[文字列文字のNBAの独自観測2件](../measurements/2026-10-27/nba-front-string-observations.json)も未解決です。
nativeは文字列全体を保持し、参照runtimeは両版とも異常終了します。文字列文字のNBAが合法か、
対象の粒度がどうなるかは未確認で、6.16/10.4.2/9.4.5を確認してから修正方針を選びます。
適合性の期待値や成功行列の件数には数えません。以前のmonitorノートの、通常eventでの
呼び出し関数private値の直接参照とソース純粋性の差も残ります。
Linux、既定C++ allocatorの故障注入、静かなホストでの速度、他のP1機能、Icarus全範囲の同等性には
引き続き証拠が必要です。P1は未完です。
