# automatic記憶域に対する待機の依存

[English](automatic-storage-observation.md)。Status: カーネル接続を実装、
2026-10-25。ソースの捕捉への接続は残作業です。

## 契約と実装

承認済み[automatic捕捉の契約](automatic-capture-proposal.ja.md)の2/4/6/8節に従います。
[承認記録](p1-execution-contracts-2026-10-25.ja.md)でSCHEDULER_SPEC 9.5と
RUNTIME_ABI 4/6に対応付けています。言語上の根拠はIEEE 1800-2023の6.21、9.3.2、9.4です。
他のシミュレータの実装やテストは読まず、依存・大域状態・外部C ABIは追加していません。

以前は[記憶域基盤](automatic-storage-foundation.ja.md)の変更通知をスケジューラが
受け取っていませんでした。`Scheduler::changedActivation`が正規化後の確定した変更を
既存の順序付き遅延通知キューへ渡します。`writeActivation`は型付き書き込み後に
次の命令までに通知を処理し、storeのhook内でソースの評価は実行しません。
native callbackも観測可能な書き込みにこのラッパーを使います。storeへの直接書き込みを
まとめる場合は、次の観測可能な操作までに呼び出し側が明示的にflushします。

`MonitorSpec::activation`は字句的なextentのbinding、実行時の読み取り捕捉、
独立した静的感度範囲を持ちます。`ActivationStore::region`は値を読まずhookも呼ばずに、
Context・世代・フィールド・要素・ビットの範囲を検証します。登録は式の評価前に
異なるextentのleaseを一括で取得し、失敗時には途中の取得を取り消します。
複数フィールドでも実体は共有し、スコープの全型付きフィールドを保持します。
別Context・古いbinding・不正範囲は評価前に拒否します。

再評価に成功するたびに実行時の読み取り範囲を置き換えます。未実行の分岐も含む
静的範囲は維持し、呼び出した関数のprivateなautomatic extentは自動的には取り込みません。
値・辺・iff・真偽・Active/Reactiveの判定は既存の共通スケジューラを使います。
コンテナ変数はbindingの置き換えと現在のheap構造/セルの両方を観測し、
型付きルート提供が物理フィールドを一度だけ保持します。

待機の完了・置き換え・取り消しは依存を外してleaseを解放します。
登録serialで再利用したmonitorへの古い通知を防ぎます。評価中は一時的なleaseを持ち、
自身の待機を取り消してもcallbackが戻るまでbindingを解放しません。
最後の所有者が消えたらhookを戻します。観測用領域の確保失敗は既存の
resource-limit経路で処理し、確定済み書き込みは巻き戻しません。
型付きルートの非割り当てvisitorは変更していません。

## 検証と残作業

独自の`ActivationScheduler` 14テストが成功しました。4値の辺32組、幅広フィールド、
独立要素/スコープ/Context、readonly、正規化、静的範囲、親の終了、自己キャンセル、
名前付きイベントとの混合、guard中の書き込み、コンテナ置き換え、ルート回収、
登録の各確保失敗点と通知の確保失敗を検査します。1万回の待機置き換えでも
登録領域が増え続けず、lease数が元に戻ります。

初回評価の追加検証では、範囲を登録する前に値を読んで書き換え、古い値を返す式の
通知が失われました。追加テストは修正前に失敗し、修正後に成功しています。
評価途中の読み取り範囲も通知照合に使い、入れ子の捕捉は戻ると元に戻します。
通知は待機全体の登録後に処理します。`e59c3dd9`での
[コンパイルした6故障](../measurements/2026-10-25/activation-wait-negative-controls.json)は
すべて実際の追加テストで検出し、元のソースに戻したビルドも成功しました。
`e9bec784`での[7個目のコンパイルした故障](../measurements/2026-10-25/activation-wait-initial-control.json)は
評価途中の捕捉を落とし、初回評価テストが失敗しました。元に戻すと追加14テストが成功します。

`efb3f3d1`の[最終検証](../measurements/2026-10-25/activation-wait-final-validation.json)で
local CI全21項目が成功しました。dev/ASan/UBSanは4,569/4,571、releaseは4,570/4,572成功、
失敗なし、従来どおり2件スキップです。初回のclang-tidyが検出した新テストの2点は修正済みです。
ルートの検証は非割り当ての固定領域へ変え、一度だけ訪問・正しいtoken・回収成功の比較を維持します。
既存期待値・SIRダンプ29件・PerfCount基準は変更していません。

同じ87項目の[比較の再実行](icarus-13-comparison.ja.md)は全エンジンの判定と成功時の比較出力を
維持しました。nativeのsv-testsも344件の判定と244件の成功を維持しています。
sv-testsの参照エンジンとLinuxは再実行せず、この有限の検査で全面的な同等性は主張しません。
[母集団の再実行](../measurements/2026-10-25/activation-wait-population.json)は
新しい作業ツリーを使わず、選択時の試験版`179c55de9c83`のarchiveで条件を固定しました。
1,451/1,466成功、失敗0、未対応15件です。

SIRのscope home/命令、frontの字句的捕捉、VMのbindingアクセスと巻き戻し、ref、
automatic event、automaticな表示callbackは残作業です。既存ソースの捕捉診断は維持します。
Icarusが通る差分の解消は主張せず、有限の比較では両参照とも22件が残ります。
P1全範囲とソースの受け入れ行列は未完了です。今回の実行時間の再計測は未実施で、
PERFORMANCEの既存手順を引き続き使います。

## 次の接続箇所

VMの`frameVar`・`frameElem`・`act_.vars`直接参照は可変のフレーム語を返します。
scope homeを使うには、スカラー/幅広/packed値・ref・走査・バイナリ入力・乱数seedと
引数のコピー戻しを共通の型付き読み書きへ接続する必要があります。通常フレームにも
同じ値を複製すると、共有実体と通知の契約を破ります。`Lowerer::lowerFork`は現在、
子を新しい`Body`で開始します。未対応診断を外す前に字句的捕捉のメタデータ、
スコープ全体の昇格、入るたびの所有権が必要です。SIRはhome・捕捉互換性・所有権の
巻き戻しを検証し、呼び出しのroot記憶域は引数初期化より前に作ります。
これらは承認済み契約の残作業で、対応済みソース機能や追加の修正案とは扱いません。
