# tier 1で字句的なautomatic環境を実行する

[English](automatic-vm-execution.md)が正。状態：現在表現できる値のhomeについてInterpreterへ
接続、2026-10-25。frontによるソースの捕捉は未有効です。

## 契約と実行

[環境](automatic-vm-environment.ja.md)・[値の転送](automatic-vm-frame-access.ja.md)・
[scoped ref](automatic-vm-scoped-refs.ja.md)をInterpreterへ接続します。
根拠は承認済み[automatic捕捉契約](automatic-capture-proposal.ja.md)の1〜4/6〜8節、
[仕様との対応](p1-execution-contracts-2026-10-25.ja.md)、SIR_SPEC 7/8・RUNTIME_ABI 4/6・
IEEE 1800-2023 6.21/9.3.2/13.5.2です。実装・テストは独自作成で、他シミュレータの
実装・テストは読みもコピーもしていません。依存・CLI設定・公開C ABI・物理フレームABIは加えません。

slot確保前のメタデータ検証を維持し、妥当な新homeを一律に拒否する処理を外しました。
アドレスを固定したplan・参照shapeは環境より長く生存します。各VarsIdは必要に応じて環境を持ち、
通常のbodyでは環境の確保を要しません。Scoped/Capture行に通常の値のワードを割り当てず、
framePlace/refPlaceが実際のbindingと範囲を単一の値アクセス境界へ渡します。
変更可能な値を二重に持たず、正規化と通知の意味はkernelに置きます。

仮引数をコピー入力する前にrootを所有し、promoteしたoutput仮引数を言語上の既定値へ初期化します。
内部の安全な0を宣言の初期化とは扱いません。宣言のloweringは引き続き明示的に必要です。
通常のcall/task復帰ではoutputを読み、一度コピー戻してからcalleeの実行所有権を解放します。
残った子がoutputを後で変えてもコピー戻しを繰り返しません。同期・遅延呼び出しは確保失敗時も
後始末のguardで現在body・呼び出し深さ・scope・所有権を戻します。

activation.enterは新しいブロックの実体を作り、leaveは実行所有権と借用記述子のticketを解放します。
名前付き制御scopeは実際のVarsIdと字句的深さを保存し、leave/disableでそこへ巻き戻します。
終了時は待機・display callbackを外してからowner/importを解放します。データの所有権を
tryReleaseの親子・報告・キューの都合とは分け、programによる停止も同じ後始末を通します。
実行全体の後始末では通常slotのiff待機も無効化してからVMフレームを破棄します。

forkは外側processのフレームではなく、実行中のtaskを含む現在のbodyを使います。
全子の環境を解決してから実行可能な子を作り、公開先の配列を予約し、仮のprocessを全登録・
キュー投入した後で親のRNG・出生番号・join状態を確定します。失敗時は仮のprocessを止め、値を
解放します。古いキュー記録には既存のスケジューラ/制御の後始末だけを残します。
記憶域のためのprocessや乱数の抽選は増やさず、成功時はソース順に子ごとに一度RNGを作ります。

参照先は不変の宣言元と世代付き記述子で解決します。readonlyも通常・slot・cell参照のpayloadへ
持たせ、変更前に検査します。仮引数を再利用しても子のref bindingは不変です。
staticなactualと「ローカルから来た」という分類を区別し、ref staticと通常refのjoin制限を保ちます。
借用refに所有leaseを足さず、子がデータを保持していても作成scopeの終了でticketは無効になります。

guard/watchは待機元の実際の登録を使い、空・未読scopeを含む環境全体を一時pinします。
待機には、書き込み先を除いた全分岐の静的activation範囲と、所有bindingの実際の読み取りを登録します。
共有されたref actualもactivation範囲へ解決し、container変数の置換とheap内容の観測を両方保ちます。
捕捉した借用refは型付きrootの走査へ渡し、promoteした値と延長scopeの未読フィールドはkernelが
直接rootを提供します。callback入口から確保失敗の例外を漏らしません。

## 検証

独自19実行テストで幅/状態/refの24組（7〜257ビット）、全join、親子/兄弟の書き込み、親の復帰、
再進入、多段転送、両call経路、output既定値、仮引数再利用をまたぐref static/const ref、
共有actualへのref、task内guard、未実行分岐の静的依存、ownerと制御の独立した寿命、未読heap root、
scope/programのキャンセル、VM復帰後のiff待機を調べます。kernel確保予算80通りでは、構築/fork全体の
失敗がResourceLimitを返し、子の結果を公開せず全scope leaseを解放することを確認します。
Interpreterの通常C++コンテナの全確保点を注入した検査ではなく、部品のPMR失敗は前のノートで別に検証します。

新規10・既存の環境/guard 2・移動した制御/stacktrace 2のコンパイルした14故障を検出しました。
出生番号の故障は最初のdisable行列では残りました。先の子を一部だけ回収し、別の古い子を残したまま
後のnamed blockをdisableするテストを加え、その古い子を誤って止める故障を検出しました。
ビルド失敗は動作の故障を検出した結果に数えません。

`b17234e4`の[local CI全体](../measurements/2026-10-25/activation-interpreter-final-validation.json)は
全21項目成功です。dev/ASan/UBSanは4,680/4,682、releaseは4,681/4,683成功、失敗0・既知2スキップです。
[故障注入の記録](../measurements/2026-10-25/activation-interpreter-negative-controls.json)には
出生番号の故障が最初に残った試行と、その後の検出を両方残し、18/19テストそれぞれのハッシュを記載します。
その間に製品のソースは変えていません。既存suite本体・期待値・SIRダンプ29件・PerfCount基準も不変です。

[87 probeの再実行](../measurements/2026-10-25/icarus14-activation-interpreter-probes.json)では
native/14.0の全判定・成功時の判定対象バイトが不変で、過去の13.0記録への成功差分22件は残ります。
正確に展開した[P1母集団](../measurements/2026-10-25/activation-interpreter-population.json)は
1,451/1,466成功、失敗0・未対応15件、固定した
[sv-tests](../measurements/2026-10-25/activation-interpreter-svtests.json)は244/344で全native判定を維持します。
この2群のLinux・他エンジンは測り直さず、新しくcleanな13.0をビルドしたとは主張しません。

CI終了後の[通常の輸送経路の重点観測](../measurements/2026-10-25/activation-interpreter-speed.json)は
交互5組で、調整後wall timeが0.2500秒から0.2599秒へ約+4.0%、カウンタはすべて同じです。
peak RSS中央値は9.1875から9.203125 MiB（+16 KiB）でした。小標本で信頼区間ではなく、
シミュレータ比較全体でもありません。標準4負荷・ソース捕捉・Linuxは時間を測らず、性能予算も変えません。
有限の比較だけでIcarus全範囲を証明しません。

## 残作業

frontはソースのautomatic fork捕捉を引き続き拒否し、新しいhomeをまだ生成しません。
次に字句的なpromote/captureのloweringを実装し、宣言順、loop/block寿命、ref修飾、コピー戻し、
キャンセル、monitor、typed rootを含む承認済みソース行列全体を検証します。
手組みSIRの実行だけでソース機能の差分を解消したとは数えません。

所有するautomatic event/ref/objectの実提供は未対応で、借用ref仮引数とは別です。
契約5節のevent構築/再束縛は今回実装しません。完全なSSA参照由来と、参照記述子の全世代消費は
未確認で、後者の確認方法は前の参照ノートに記載しています。不正SIRへの防御2件は権限/寿命の検査であり、
そのように逃がしたrefや書き換え可能なconst refを合法なSystemVerilogと認めません。
P1全体・Icarus 13.0の全範囲への対応は未完了です。
