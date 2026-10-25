# SIRの型付き管理event値

[English](event-sir-contract.md)が正。Status: SIR段階`efebd540`、2026-10-25。
VMでの実行とautomatic eventのソース生成は未実装です。

**後続の接続（2026-10-26）：** [境界でのVM部品](event-vm-boundaries.ja.md)で型付き形式を
実行します。以下はSIRだけの段階の過去の記録で、ソース生成と命令間safepointは未有効です。

## 根拠と境界

根拠は[承認済みautomatic捕捉契約](automatic-capture-proposal.ja.md)5/6/7、
[規範的な対応](p1-execution-contracts-2026-10-25.ja.md)、SIR_SPEC 3.2.1/7/8、
RUNTIME_ABI 4/6とIEEE 1800-2023 6.8・6.21・15.5です。実装とテストは独自に作成し、
他シミュレータの実装・テストコードは読んでいません。依存・新IR・C ABIの選定はありません。

`event.new`はオペランドなしで型付き`event`値を1個返し、終端・待機命令ではありません。
既存の名前付き待機・混合event待機・即時trigger・非ブロッキングtriggerは、従来のslotに加え、
型付きSSA/定数のeventを受け取ります。frame参照を直接event値とせず、先に型付きloadを
使います。混合待機の項ごとの`iff`・信号の辺・順序と、遅延が整数である規則を保ちます。

event定数はゼロの1語だけとし、未知ビットの語・バイト列は持てません。非nullの同一性は
生成または型付き記憶域からのloadで得て、整数定数で偽造しません。型付きeventオペランドには
辺・選択・ブロック引数数を付けられません。SSAの支配関係と待機をまたぐSSA値の禁止を保ち、
後で使う値は型付きhomeに保存します。draftの出力は`event.new event`・`%tN`・
`const event null`を使い、形式版は**0**のままです。パーサや往復保証の追加ではありません。

## 検証

新規`EventSir`8テストは正しい設計17件と不正な設計40件を検査します。
EventSir/Sir/VerifyMatrixの対象75テストが通りました。引数数・結果型・定数・対象型・
隠れた選択・混合待機の認識・命令名・null出力に関する独立した8故障を実際にコンパイルし、
すべて検出しました。コンパイル失敗や判定器の自己検査は検出に数えません。
復元した実装でも75テストすべてが通ります。

[全local CI](../measurements/2026-10-25/event-sir-validation.json)の21確認がすべて通り、
dev/ASan/UBSanは4,948/4,950、releaseは
4,949/4,951で、失敗0・各構成の既知スキップ2件です。
既存dump全29件とPerfCountも通り、既存期待値は変えていません。
[故障検出記録](../measurements/2026-10-25/event-sir-controls.json)に正確なsource hashと失敗した
テスト名を残しました。READMEの件数はdev JUnitから生成しました。

## 残る接続と承認済み移行

この段階は表現の検証・出力までで、第1層による新形式の実行やfrontの有効化ではありません。
既存のautomatic eventソースと正規event活動フィールドは、引き続き拒否します。
[期待値3件の移行](event-field-expectation-migration.ja.md)は`965806f4`に承認を記録し、
今回はまだ適用していません。最初の2件は実際のeventフィールドproviderと接続してから、
ソースの1件はソース実行全体が揃ってから適用し、実装とは別コミットにします。

次は正規eventフィールドを`ActivationPlan`へ接続し、通常・scoped・captured・input・SSAの
全event保持箇所と、保持スコープ全体のproviderを公開します。未公開の一時値をpinで守り、
第1層から`event.new`・型付きscheduler操作・triggered読みの捕捉を機械的につなぎます。
位置付きのnull待機エラーと全終了経路の後始末を含め、その後frontの生成・コピー・添字操作を
有効にして独自ソース行列全体を検証します。待機や子孫だけでなく、**時間待ちのない**
スコープ再進入・宣言の繰り返しでも物理レコードの再利用が有界なことを検査します。

IRだけの今回の段階でソースのIcarus比較は再実行しておらず、差分が減ったとは扱いません。
以前のIcarus 13合格との差分21件は全機能の棚卸しではありません。
母集団・sv-tests・Linux・速度の測定も更新しておらず、P1全体とIcarus全範囲対応は未完了です。
