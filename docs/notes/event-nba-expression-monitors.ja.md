# イベントNBAの式monitorの所有と通知

[English](event-nba-expression-monitors.md)。状態：kernelの区切りを`012af2ca`としてmainへ統合、2026-10-27。
初回の検証はmain `e94315a0`が基です。**ソースからのイベント制御付きNBAを有効にはしません**。
P1とIcarusの機能範囲全体の同等性は未確認です。

## 根拠と共通の規則

根拠は承認済みの[event-NBA契約](event-nba-proposal.ja.md)のscheduler項目3・生存期間・
公開順序、[仕様との対応](p1-execution-contracts-2026-10-25.ja.md)、SCHEDULER_SPEC
9.5/10.8、RUNTIME_ABI 5/6、VALUE_SEMANTICSの値とエッジの規則です。背景の規格箇所は
IEEE 1800-2023の9.4.2・9.4.5・10.4.2・15.5です。通知ごとにORをまとめるのは、
承認済みのプロジェクト方針です。外部ソース、他プロジェクトのテスト・再現コード、規格の
本文・例は使っていません。詳細仕様・依存・C ABI・SIR命令は変更しません。

`Scheduler::registerNbaEvent`は、具体的なslot/名前付きの項に加え、`MonitorSpec`を
受け取ります。通常の待機と独立したNBA購読が、`initializeMonitor`・`updateMonitor`・
`evalMonitorValue`・`evalMonitorBody`・`monitorDependsOnSlot`・`observesEvent`を共有します。
任意幅の2/4値、real、エッジ、`iff`に既存の共通演算を使い、guardが偽でも前回値と依存先を更新します。

独立した所有元が、完了と実行中callbackの終了まで、元のプロセス・monitor状態・managed eventの
pin・heap hook・activation leaseを保持します。通常待機の解除や発行元の終了では外れません。
完了・破棄時は索引、バッファ、弱いwatchを直ちに外します。空きmonitorは未使用のowner番号欄で
連結し、解放ではメモリを割り当てません。owner欄はprocessと64 bit generationの間に入りますが、
レコード全体のサイズが全ABIで同じとは主張しません。

## 公開順序とcallbackからの再入

通常の待機と独立NBAの両方で、最初の初期評価より前に、静的な依存先と静的event値のpinを
すべて公開します。初期評価中のheap読み取りも捕捉し、読んだ直後の書き換えが、弱いwatchを
付ける前だからと失われないようにします。入れ子の評価は読み取り捕捉と出力バッファを分け、
callbackからの追加登録で、現在の出力先を無効にしません。再利用用の大きなバッファを各monitorの
前回値へ配らず、実際の値の幅だけコピーします。

最初の全CIで、既存のコンテナ条件式の待機がループする回帰を発見しました。一時コンテナの
読み取りと、その構築のための書き込みを依存先と変更として拾い、自己通知を繰り返していました。
[修正前後の記録](../measurements/2026-10-27/nba-monitors-work-object-regression.json)に、
未完のCIの観測、保存した修正前binaryのtimeout、修正後に既存の待機9設計が期待値を変えずに
成功した結果を残します。heapのtransactionがcommit後に成功した割り当てを通知し、読み取りの
捕捉でその作業rootを除外します。入れ子でも割り当ての観測を外へ伝え、失敗したtransactionは
rootを通知しません。各セルの生成時刻欄や割り当て番号は追加しません。既存の外部rootへの
初期評価中の書き込みは、別の検査で引き続き通知を確認します。新規2テストは誤実装時にも
走り続けないよう、検出器側の回数を制限しています。

公開前の不正な記述・割り当て失敗では、保持値とcleanupは呼び出し元が持ちます。受理後は、
初期評価中に同期的にキャンセル・終了して、登録関数が戻る前に購読がなくなる場合もあります。
受理後の実行時の割り当て失敗は`Scheduler::reason()`で報告し、受理済みのticketを返します。
cleanupは移転済みなので呼び出し元が再解放してはいけません。公開ヘッダとテストでこの境界を示します。

このkernel callbackの能力を根拠に、ソース言語のevent式から変数へ書ける範囲は広げません。
既存のfrontの制限と拒否の期待値は保存します。

## まとめて確定する通知

直接購読と式の依存先の候補を、捕捉済みの発行順に並べ、同じ所有元を重複させません。
関連する項をソース順にすべて評価し、短絡しません。直接項とmonitorの混在や、複数の式が
一致しても、通知全体が終わってから回数を1回だけ減らします。

heapの原子的な`swapValues`は、両セルを確定してから両rootを通知します。Contextが持つ
「最後のroot」の印で通知をまとめ、最後が監視対象外でも閉じます。activationの書き込みも、
一致するmonitor参照を1通知へまとめます。従来のheap hook呼び出しの順番は維持します。
プロセス・子RNG seed・大域状態・増え続ける通知番号は追加しません。名前付きイベントは
実際の静的/捕捉済み依存先を調べ、同じ所有元に名前付き項があるだけでは、無関係なslot式を再評価しません。

遅延した通知群はContext内の再利用可能なレコードを使います。配信時に参照列をローカルへ移し、
**ユーザーcallbackより前**にレコードを空けます。キャンセル・追加登録で同じ番号を再利用しても、
進行中の走査を壊さず、古い参照を新しい購読へ届けません。callback内の書き込みは、従来どおり
最も外側の走査の終了後に通知します。

最終確認で、通常のcallbackが前方の独立項を消すと、別の通常待機を飛ばす
[共存の回帰](../measurements/2026-10-27/nba-monitors-coexistence-regression.json)を再現しました。
slotと名前付きmonitorは登録済み参照のsnapshotを走査し、callback後に現在の索引を整理します。
追加された項を保存し、過去の通知を遡って届けず、キャンセル済みの参照を戻しません。
Active/Reactive × slot/名前付きの4条件の検査を弱めず、不正なin-place走査へ戻す
コンパイル済み故障も検出します。

## 検証

[main統合の検証](../measurements/2026-10-27/nba-monitors-main-integration.json)に、
main `a0a0d973`と合わせた後のpush前検査を記録します。以下の計測は当時のhashと件数を
そのまま保存します。統合のCIは全21項目成功、dev/ASan/UBSanは各7,080/7,082、
releaseは7,081/7,083で、失敗0・同じ既知skip各2件です。
未完成のSIR/VM作業は、この取り込みに含めません。

[検証記録](../measurements/2026-10-27/nba-monitors-validation.json)に、最終ハッシュ、全local CI、
単体検査、fuzz設定、明示的な期待値保護を記録します。CI全21項目が成功し、dev/ASan/UBSanは各
7,073/7,075、releaseは7,074/7,076、失敗0・同じ既知skip各2件です。新規27件を含め、単体742件も
すべて成功しました。新規kernel 27件は、独立した所有、混在・重複、
偽guard、4値の全遷移、65/129 bit、real、managed eventの付け替え、heap/indexの付け替え、原子的swap、
activation範囲、初期評価中の書き込み、入れ子、同期キャンセル、受理前後の割り当て失敗、
レコード数が増え続けないことを検査します。SystemVerilog設計の新規対応件数には数えません。

[コンパイルする誤実装検査](../measurements/2026-10-27/nba-monitors-controls.json)は、monitor・
具体的な購読・VMの元の保持に32故障を入れます。[初回](../measurements/2026-10-27/nba-monitors-initial-controls.json)
は27/29をコンパイルして検出し、2件は未使用変数の警告で止まったので検出に数えません。
置換を直し、[その2件もコンパイルして検出](../measurements/2026-10-27/nba-monitors-corrective-controls.json)しました。
最終検査は、解放時の割り当てと作業rootの扱いを直した後に全件を再ビルド・再実行し、正の検査より前に
ソースをすべて復元します。

混在項の独立モデルは、両group・12購読・各6項・各周80操作です。100倍で4,000周・48,000購読・
320,000通知を検査します。先行する具体的な購読のモデルも100倍で再実行します。
有限のモデルとの照合であり、7晩連続の値全体gateや言語全体の同等性の証明ではありません。

[母集団・sv-tests・版付きプローブ](../measurements/2026-10-27/nba-monitors-comparison.json)は、
先行する具体的な購読の区切りと比べます。[releaseの対応をそろえた計測](../measurements/2026-10-27/nba-monitors-benchmark.json)
はPERFORMANCE 5に従い、大小の条件を各5回実行します。この3設計は独立NBA購読が動いていない
ときの費用であり、ソースevent-NBAの速度や、全体の高速化を示しません。

AB/BAを交互にした5組の前後の実行時間中央値は、100,000周の`nba_shift`が2.746624/2.719253秒、
100,000の`proc_wait`が0.904969/0.897051秒、500,000更新の`nba_burst`が0.110472/0.113744秒です。
各組で+n=1のwarmupを行い、全sampleの結果行は一致します。burstのpeak RSSは
74.625/74.59375 MiBです。[最初の連続した前後測定](../measurements/2026-10-27/nba-monitors-benchmark-initial.json)
では`nba_shift`に約35%の差が出ましたが、順番を交互にすると再現しませんでした。両方の
host負荷と生のsampleを残します。負荷・順番の影響があるため、静かなhostや全体の速度について
結論は出さず、性能・nightlyのgateの代わりにも扱いません。

## 残るソース境界と新しく観測した差分

型付き`nba.event`のSIR記述、verifier・text、front変換、VMへの転送は残ります。発行元が戻った後も、
新しい一時callback activationで元のRNG・scope・報告の文脈を保ち、戻ったtaskのactivationを
借りてはいけません。RHS・回数・LHSの捕捉順、待機を省く回数の評価、集約値、ソース行列も必要です。
このkernel段階でソースevent-NBAの差分を閉じません。

[独自の通常event式3例](../measurements/2026-10-27/nba-monitors-source-investigation.json)でも、
固定95プローブとは別に差分を観測しました。

- **同じ**呼び出し関数のstatic localを階層参照で直接読み、その関数が初期評価中に私有値を
  変更する場合、nativeの修正前後は`0:1`、Icarus両版は`1:1`です。`Interpreter::depsOf`が
  呼んだ関数のslotを一括して除外し、`keepHeapRead`も私有graphを除外します。直接読みと
  callee内の読みの出所・aliasを扱う共通の依存規則と行列が必要です。kernelの公開順序だけ
  直しても、存在しない依存先は戻りません。
- 呼び出し関数が外側のscalarやqueueへ書く例はnative frontが拒否しますが、Icarus両版は
  コンパイルして`1:1`/`1:11`を出します。SIR_SPEC 7.2とRUNTIME_ABI 5にソースの純粋性の制限が
  残ります。広げる前に、具体的な仕様修正の人による承認と、従来の拒否期待値の別レビュー・移行が
  必要です。承認済みの実行契約6件を、無関係な制限を黙って削除する許可とは扱いません。

参照の観測だけで規格全体の解釈を証明しません。次のソース作業では、直接/callee/alias/activationの
行列を、挙げた仕様・規格の箇所と照合する必要があります。95例の既知差分7件は、残機能全体の件数では
ありません。Linuxと全言語範囲は未確認です。同じgateをLinuxでも実行し、ローカルの区切りをP1完了と
せず、元のP1全範囲を続けます。
