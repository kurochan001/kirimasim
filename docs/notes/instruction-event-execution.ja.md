# automatic event の構築と公開済み命令境界での回収

[English](instruction-event-execution.md) が正です。状態：`e909f022`で実装し、ローカル検証済み、
2026-10-26。過去の拒否期待値 1 件の移行は承認後に `4ba6b63d` で適用し、
承認後の local CI 全 21 項目が通りました。

## 承認後の最新検証

期待値だけの移行 `4ba6b63d` の後、[全 local CI](../measurements/2026-10-26/automatic-event-approved-validation.json)
が 21/21 成功しました。dev/ASan/UBSan は各 5,743 本中 5,741 成功、release は
5,744 本中 5,742 成功で、各失敗 0・既知スキップ 2 件です。全入力本体と拒否境界 4 件を
保持しました。実装と unit のソースは `e909f022` から不変で、以下の比較・費用・故障注入の
記録を継承します。以下の移行前の失敗はこの承認で解消済みです。P1 全体は未完了です。

## 契約と実装

根拠は、承認済みの [automatic-capture 1/2/5/6/8](automatic-capture-proposal.ja.md)、
[managed-event safepoint 修正](managed-event-safepoints-proposal.ja.md)、
RUNTIME_ABI 6、SIR_SPEC 3.2.1/5.1/7/8 と IEEE 1800-2023
6.8/6.21/9.3.2/13.5/15.5 です。実装・生成器・故障は独自作成で、外部シミュレータの
ソースやテストを読まず、規格の本文・例も転載していません。依存、オプション、大域状態、
SIR/C ABI の版は追加していません。

front の宣言既定値の経路で、初期化子のない automatic scalar event と、固定配列の
各要素へ、宣言順で個別の `event.new` を出します。宣言の再実行ごとに固有の object を
作ります。既存の lexical environment が、読み出さない event 要素を含む範囲全体を
子孫の活動まで保持します。対象選択、待ち、triggered は既存の型付き place の共通処理を
通り、VM がソースの配置を見て意味を変える分岐は追加しません。front の automatic event
値仮引数、初期化子の別名、automatic unpacked struct の event field は未対応です。
event の再関連付け、比較、event を含む動的コンテナも有効にしません。

VM は安定した変数表 ID の連結スタックに、ネイティブ実行中の活動を確保なしで公開します。
process/task、同期呼び出し、guard/watch、入力束縛、出力書き戻し、発行元の終了後に
走る確定済み deferred call を含みます。再配置しうる語・登録 vector 内のポインタは
保持しません。ネイティブ実行が借りている callback の frame 記録は解放を遅らせ、
RAII で公開を外してから活動を登録解除します。

型付きの入力・出力 snapshot 全体を、callback が可能なコピーより先に既存の転送 pin で
保持します。入力 pin は全引数を callee field に公開してから外します。戻り値の pin は
SSA 領域の終了に先立ち取得し、出力 callback をまたぎ caller が値を置くまで保持します。
束縛途中の宛先にも転送元全体の pin があり、部分的な root 集合を collector に渡しません。
ネイティブスタック、生存 frame、保持した lexical 範囲、参照、deferred 引数、型付き slot、
全 active SSA 領域、カーネルの操作 pin を使います。不透明な数値から event を推測せず、
未対応の class/DPI/object 型を追跡済み扱いにはしません。

`event.new` が物理行を増やす前に、再利用行がなければ、通常実行の許される時点で回収します。
終了/final の方針はカーネルの問い合わせにまとめました。`runFinal` は実行中に一時的に
`ending()` を戻すので、それだけの判定では不十分です。final 中の回収は追加しません。
世代上限での行の引退と資源制限の規則も保ちます。回収はソースの callback を実行せず、
命令を再実行せず、時刻・領域も進めません。この命令境界ではコンテナ heap を回収しません。

SSA の event 語は body の終了まで保守的に保持します。最終使用直後に捨てる生存解析では
なく、loop の同じ一時値が前回の object を上書きまで保持する場合があります。有界性は
静的な型付き一時語、生存 field/保持範囲、操作 pin、世代引退行に依存し、loop 回数には
依存しません。検査した 1 event の loop は物理行 2 個以内です。

copy-in/copyback の共通処理では、確定した各書き込みの後に、新たな終了・起源 program の
停止を確認し、残りのコピーや callee/継続の副作用を止めます。確定済み report や final の
ように、元の所有者の終了後でも開始できる呼び出しとは区別します。同期呼び出しの所有は
copy-in の callback より先に登録します。SIR_SPEC 8、automatic-capture 3/6 の寿命・
転送規則を共通の呼び出し境界で実装し、scalar/配列と各記憶域へ適用します。

## テストとレビュー境界

VM 新規 12 テストは、検証器を通した独自 SIR 32 設計です。待機のない process/function/task
loop、lexical 再入、読み出さない配列、深さ 64 の再帰、入れ子/reentrant guard、
確定済み deferred 入力、通常の native field、出力 callback 中の戻り値 pin、両領域の
遅延 named/mixed 待ち、同期/task の入力/出力 callback による終了、子孫の保持、
コンテナ heap の除外、final、確保失敗と再利用を含みます。意味と物理行の上限を検査し、
追加の registry 1 テストで、再利用行の問い合わせが実際の回収・消費に対応することを確認します。

独自ソース行列は 176 設計です。既定値は配置 8 種 × 形状 4 種で、各 64 回の待機なし
宣言再入を検査します。保持 task は module/program、join 3 種、形状 4 種、
named/mixed/triggered 待ち、即時/遅延 NBA trigger を組み合わせます。既存の範囲全体の
拒否入力も正常実行するようになりました。[期待値 1 件の移行](automatic-event-expectation-migration.ja.md)
は別途承認後に `4ba6b63d` で適用し、入力を保持したまま正常終了・正確な出力・
エラーなしを検査します。

**期待値移行前の記録：** [全 local CI](../measurements/2026-10-26/instruction-events-validation.json) は 21 項目中 17 成功です。
4 構成すべての全体ビルドが通り、dev/ASan/UBSan は各 5,743 本中 5,740 成功、release は
5,744 本中 5,741 成功で、各既知スキップ 2 件・失敗 1 件です。失敗はすべて承認待ちの
同じ metadata 拒否期待値で、方針・全 format/tidy・生成件数・既存期待値保護は通りました。
故障の置換位置 791 件もすべて適用可能です。既存入力本体、dump、PerfCount の期待値は維持します。

[新規 12 故障](../measurements/2026-10-26/instruction-events-controls.json)はすべてコンパイルでき、
テストで検出しました。コンパイル失敗は検出扱いにせず、元の byte に戻した後の関連 288 検査も
通りました。native root/祖先、SSA、回収、戻り値/入力 pin の欠落、コンテナ/final の誤回収、
キャンセル後の転送続行、null 初期化、配列要素での同一 object 共有を検出します。

[87 プローブの再比較](../measurements/2026-10-26/instruction-events-comparison.json)で、両 Icarus
参照との差分は 8→7 件になりました。以前の成功の判定用出力と参照の全状態を保ち、不一致は
4/7 件のままです。[固定母集団](../measurements/2026-10-26/instruction-events-population.txt)は
1,466 本中 1,456 成功・失敗 0・未対応 10、native のみの
[sv-tests](../measurements/2026-10-26/instruction-events-svtests.json)は 344 本中 244 成功を維持します。
sv-tests の参照側と Linux は再計測していません。

[通常経路を交互に 5 組計測](../measurements/2026-10-26/instruction-events-speed.json)した時間差は、
frame/call が +1.2%、静的 named event が +0.1% で、全カウンタは一致しました。大きい入力の
RSS 中央値は 9.19→9.27 MiB と 9.19→9.19 MiB です。
[新経路は各サイズ 5 回](../measurements/2026-10-26/instruction-events-scaling.json)、宣言再入
1・2,000・20,000・200,000 回を測り、scalar/3 要素配列の最大入力の中央値は
0.0700/0.2690 秒、各サイズの RSS 中央値は 9.1875 MiB でした。PERFORMANCE 5 に従う
1 台での観測で、実時間の合否判定・信頼区間・7 夜の baseline ではありません。物理行の上限は
VM テストで検査し、RSS だけを行数の証明にはしません。

root 公開の作業領域は既存の
標準確保を使い、失敗は sweep より前に resource limit で捕捉しますが、各作業領域の
実際の確保失敗を決定的に注入する検査は**未確認**です。大域状態や隠しオプションを
追加せず、確保元を制御できる publisher によって確認します。registry/転送の失敗、
provider 失敗時の非 sweep、Context 確保失敗と再利用の検査とは区別します。

P1 は未完了です。event の値・仮引数・struct と [P1_SUPPORT](../P1_SUPPORT.ja.md) の
他の必須機能が残り、有限のプローブから Icarus 全面同等とは判定しません。実装と承認済み移行は
[main の `304e1aec` で公開済み](automatic-event-main-integration.ja.md)です。以前の JSON の
ローカル/未統合という記述は公開前の区切りを指し、その後の hosted 検証は公開ノートへ記録します。
