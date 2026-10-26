# カーネル実行中の管理eventの所有

[English](managed-event-runtime.md). Status: カーネル実装の区切り、2026-10-25。
この区切りではソースからのautomatic event実行は未対応でした。

**現在の公開状況（2026-10-26）：** [main の `304e1aec`](automatic-event-main-integration.ja.md)は、
automatic scalar/固定配列 event の生成と公開済み命令境界での回収を含みます。以下のソース無効化や
公開手順の未完了という記述は、この部品の以前の区切りを指し、現在の実装の状態ではありません。

根拠は[承認済みautomatic捕捉契約](automatic-capture-proposal.ja.md)の5/6/8、
[仕様との対応](p1-execution-contracts-2026-10-25.ja.md)、SIR_SPEC 3.2.1/7/8、
RUNTIME_ABI 4/6、SCHEDULER_SPEC 9.4/9.5とIEEE 1800-2023 6.8、6.21、15.5.3です。
[管理表の段階](managed-event-registry.ja.md)を拡張します。実装とテストは独自に作成し、
他シミュレータの実装・テストコードは読んでいません。依存・C ABI版・SIR形式版は変えません。

## 所有の境界

`EventValue`はnull、永続legacy handle、管理tokenを表します。
`Scheduler::resolveEventValue`は古いtokenと、世代を持たない管理オブジェクトの別名を拒否します。
型付き待機は対象を一度解決し、登録前にpinを取得します。無効/nullの名前付き対象と
準備中の割り当て失敗では現在の待機を保ちます。初回monitor callbackが待機を
キャンセル・置換しても、古いpinを復活させません。

`triggerValue`は同期callbackと遅延通知が終わるまで対象を保持します。
`triggerNbValue`は発行元のフレーム・プロセスレコードと独立して具体的対象を所有し、
プロセスID再利用時もNBA/Re-NBAの起源を保ちます。保留レコードはserial付きticketと
侵入型free listを使い、64ビットserialが尽きたレコードは退役します。遅延のoverflowで
準備したpinを解放し、nullのtriggerは何もしません。配送時はレコードを再利用可能にする前に
pinをローカルへ移し、callbackでのキャンセル・回収・表の拡大と再利用に耐えます。

monitorは型付きevent依存を明示でき、実際の`triggeredValue`の読みも捕捉できます。
公開時に同じオブジェクトを重複排除し、添字による選択変更後は新しい依存を保持して
古い対象からの通知を除外します。自己キャンセルするcallbackでも旧依存を実行中は保持します。
捕捉しない入れ子monitorの読みは外側に混ざりません。最初の捕捉エラーを維持し、
時刻が進んだだけではtriggered状態のmonitorを起こしません。

起床・再待機・キャンセル・プロセス/program完了・スケジューラ終了で、登録を切り離した後に
所有pinを解放します。実行中callbackのローカルpinは戻るまで保持します。自然な静止時は
外部からの再開用に妥当な待機を保持するため、将来のVM全実行終了処理では明示的に
キャンセルする必要があります。Contextはpinより長く生存し、tokenはContext内だけで使います。

## 検証と限界

新規`ManagedEventRuntime`23テストが通ります。両実行group、混合待機/iff、動的・静的
monitor依存、入れ子と自己キャンセル、具体的な遅延対象、発行元レコード再利用、終了、
overflow、割り当て失敗とレコード再利用を含みます。後述のallocator初期化修正前の
`77cb29bd`で、実際にコンパイルした16故障を検出し、コンパイル失敗は検出に数えません。

キャンセル済み旧ticketを配送する故障は、当初のテストを**通過しました**。回数と最終時刻だけでは
tick 3でなく1に起きたことを検出できませんでした。実際の起床時刻を記録・検査することで、
同じ実装ソース上の同じ故障を検出しました。その後、エラー保持・後始末の変更と2テスト追加を
経て最終16故障を検査しています。別の新規回帰テストでは、割り当てエラーが後続の無効な読みに
上書きされ、`ResourceLimit`でなく`Internal`になる問題を再現し、最初のエラーの保持で
修正しました。[記録](../measurements/2026-10-25/event-runtime-negative-controls.json)に
当初の見逃し、実装/テストhashと最終検出を残しています。

その後の全clang-tidyで、再利用する`Process`のPMR代入にallocator次第の例外経路を
指摘されました。`8b652e3d`では空レコードをContextのallocatorで再構築し、待機世代を保ちます。
対象clang-tidyと23テストは通っています。母集団・故障検出記録には実行時の旧hashを残し、
この修正後の実行だったことにはしていません。

再測定した[87プローブ](../measurements/2026-10-25/icarus14-event-runtime-probes.json)の
native/参照の状態と合格出力はすべて同じで、過去のIcarus 13合格との差分は21件です。
固定[P1母集団](../measurements/2026-10-25/event-runtime-population.json)は1,451/1,466、
失敗0・未対応15、固定[sv-tests](../measurements/2026-10-25/event-runtime-svtests.json)は
244/344のままです。cleanなIcarus 13.0・Linux・速度比較は再測定していません。

[統合後の全local CI](../measurements/2026-10-25/event-runtime-validation.json)は全21確認が通り、
dev/ASan/UBSanは4,940/4,942、releaseは4,941/4,943で、
失敗0・各構成の既知スキップ2件です。公開C++20ヘッダ・PerfCount・既存SIR dump全29件・
静的検査も通ります。旧3期待値は別途承認済みの`11096dc3`で移行し、ソース入力は維持しました。
[別途コンパイルした再利用故障](../measurements/2026-10-25/event-runtime-reuse-control.json)で
待機世代の初期化誤りを検出し、復元後は既存1件と新規23件が通りました。当初の未使用変数による
コンパイル失敗は記録に残し、動作の故障検出には数えていません。統合前の失敗もCI記録に残します。

PMRの失敗予算はスケジューラの通常コンテナ全体を枯渇させる検査ではありません。
64ビット保留ticket全域の枯渇実行も未確認で、その主張には退役条件の確認と専用の全域検査が
必要です。キャンセルしたticketは、通常の消費まで古いキューメタデータとして残り得ます。

## 残るソースへの接続

automatic eventの生成と型付きオペランドは、SIR/verifier/テキストとVM/frontへの接続が
必要です。通常・捕捉・SSA・inputのevent保持箇所の全root公開とsafe pointをつなぎ、
未公開値は一時pinで保持してからソース宣言を有効にし、独自ソース行列全体を実行します。
eventの再束縛/比較、eventを含む動的コンテナ、公開VPI event値は別契約が必要です。
P1とIcarus全範囲対応は未完了です。マージのための停止点をこのカーネル段階に置きます。
