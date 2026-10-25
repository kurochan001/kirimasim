# 共有automatic記憶域に向けたVMの値アクセス

[English](automatic-vm-frame-access.md)。Status: アクセス経路の移行、2026-10-25。
automatic環境の構築は実装途中です。

後続の[字句的環境の部品](automatic-vm-environment.ja.md)でscopeの所有と子の環境全体を構築し、
直接検証しました。Interpreterへの登録と不変のscoped refの構築は残作業です。


現在の接続状況：[Interpreterでの実行](automatic-vm-execution.ja.md)を追加しました。
以下は各部品の段階の記録であり、その時点のVM拒否・残作業は当時の状態です。
現在は妥当な表現済みSIR homeを実行できますが、frontによるソースの捕捉はまだ有効にしていません。

## 契約と変更

承認済み[automatic捕捉の契約](automatic-capture-proposal.ja.md)の2–4/6–8節に
必要な値の受け渡しを実装します。[承認記録](p1-execution-contracts-2026-10-25.ja.md)の
対応先はSIR_SPEC 7/8とRUNTIME_ABI 4/6、言語上の参照はIEEE 1800-2023の
6.21、9.3.2、13.5.2です。外部実装・外部テスト・新しい依存は使いません。
物理フレーム配置、formatVersion 1、新しいC ABIも決定しません。

以前の`frameVar`・`frameElem`・`refWord`は変更可能な語への参照を返し、
スカラー、幅広の値、メモリファイル、ファイル入力、走査、seed更新、引数の
受け渡しが直接書き込んでいました。スカラー代入だけ共有bindingへ変えても、
他の経路が共有実体や通知を通らず書き込むことになります。

`detail::FrameValue`は1つの値/要素を受け渡す短命なビューで、通常の語を借りるか
不変のkernel activation bindingを使います。記憶域・leaseを所有せず、変更可能な
値のキャッシュも持ちません。bindingの読み取りは`ActivationStore::read`、書き込みは
`Scheduler::writeActivation`を通し、幅広い値全体または選択したビット範囲を渡します。
正規化・同一性・権限・観測の意味はkernelで決めます。通常の経路は語のコピーと
既存のkernelの選択演算を使い、ビュー自身は割り当てをしません。refのW4の両半分も保ちます。

命令・ファイル・走査・引数の既存経路を共通のアクセスへ移しました。
packedの部分書き込みは実際に覆うビットだけ、`$fread`は読み取ったバイトの範囲だけを
一度で書き込みます。残りの値を読み直して書き込みを作ることはしません。
配列全体は宣言順の要素ごとに受け渡します。`bindArgs`・`readOuts`・`writeOuts`は
登録した論理activationを使います。待つcallと同期callの両方で、コピー戻しを一度行ってから
calleeを解放します。taskと遅延callの入口もコピー入力前に登録しますが、登録だけでは
まだautomatic rootを構築しません。

## 検証と制限

独自7テストは通常/共有の選択270通りを独立したビット単位モデルと比較し、refの全payload、
不正buffer/範囲、readonly、選択した読み書きの依存範囲、Active/Reactiveで幅広い値が
揃ってからの通知、二値の正規化、スコープ再利用後の古い世代の拒否を調べます。
手組みSIRの24通りで通常/refの配列アクセス・範囲外へはみ出す部分書き込み・スカラーの
コピーを実行し、別の幅広いフレーム行を置いて論理行からの位置の変換も検証します。
通知・上位chunk・要素の同一性・readonly・選択読み取りの故障定義を5件追加しました。
既存のwait-ref位置と`$fread`バイト順の2故障は、同じ誤りを入れる位置だけ移しました。
[コンパイルした9故障](../measurements/2026-10-25/vm-frame-access-negative-controls.json)は
すべてビルド成功後に実際のunit/ソーステストで検出しました。通知省略の初回の故障は
未使用memberのビルド警告で止まったので実行時の検出に数えず、修正した故障で通知テストを失敗させました。

`Act`から不要な語ベクトルへのポインタを除いたため、既存のtask guardのフレーム選択と
override初回の感度登録の故障も移しました。前者は既存ref待機行列で検出しましたが、
後者は既存のassign/force/overrideの検査では検出できませんでした。従来の例が調べる
後続評価には既に感度が登録されていたためです。module/generate/interface、変数force/
net force/procedural assign、二値/四値の関数値、直接/式の中の呼び出しで、独自36ソースを
追加しました。net宣言は四値のままです。0に安定してからの初回評価が1を書き込み、
再評価を必要にします。36件は成功し、初回評価後に感度を登録する故障で実際の新テストが
失敗しました。既存期待値・スイート本体は変えていません。

別の[ソース観測](../measurements/2026-10-25/override-initial-observations.json)では36件とも
nativeは`seen=0`、`seen=1`、`value=1`、実測した両参照は`seen=0`、`value=1`だけを出しました。
参照のコンパイラはRHSを一度だけ評価する旨を出しています。ただしキャッシュの13.0は
`v13_0-dirty`と表示され、ビルド元の変更内容は**未確認**です。版を固定したP1の条件に
使う前に、変更のないv13_0ビルドで再実行します。既存87プローブとは別の観測であり、
その版や網羅性の証拠を置き換えません。

[最終検証](../measurements/2026-10-25/vm-frame-access-final-validation.json)には
`9b30d6d2`のlocal CI全21項目の結果をまとめました。dev/ASan/UBSanは4,629/4,631、
releaseは4,630/4,632成功、失敗0・既知2スキップです。初回dev/releaseの失敗は
追加36ソースと生成器1件をREADMEの生成件数へまだ反映していなかったことだけでした。
件数を再生成し両構成を再実行すると成功しました。間に実行系のソースは変更していません。
初回のclang-format・clang-tidyなどの静的検査も成功し、既存SIRダンプ29件と
PerfCountの基準は変えず成功しています。

[native sv-testsの再実行](../measurements/2026-10-25/vm-frame-access-svtests.json)は
344件の全判定と244件の成功を維持しました。他の作業ツリーに依存せず、版を固定した
公開テストのarchiveを使っています。[P1母集団の正確なarchive](../measurements/2026-10-25/vm-frame-access-population.json)も
1,451/1,466成功、失敗0・未対応15件です。[87プローブの再実行](../measurements/2026-10-25/icarus14-vm-frame-access-probes.json)は
native/14.0の全判定と成功時の比較出力を維持し、今回の14.0と以前記録した13.0の
両方に対する差分は22件、14.0の期待値相違は7件のままです。この一覧の13.0参照と
sv-testsの参照エンジンは再実行していません。以前の可搬記録にない失敗時の出力の一致は主張しません。

独自のautomatic関数・幅広配列・refによる[releaseの限定測定](../measurements/2026-10-25/vm-frame-access-speed.json)では、
macOS arm64で5組を交互に実行し独立したchecksumを確かめました。n=1の起動中央値を
引いたn=200,000の中央値は`48b8c315`の0.2234秒から`9b30d6d2`の0.2377秒へ約6.4%増え、
最大RSSの中央値は両方9.1875 MiBです。VM命令9,400,020を含め、報告する全カウンタが
一致しました。内部の値コピー回数は測っていません。PERFORMANCE 1/3/4に従い、時間は
記録だけに使い、合否や回数基準を変更しません。1台の限定測定で信頼区間・夜間7回の基準はなく、
標準4ベンチ・他シミュレータ・Linuxの再計測ではありません。

現在VMのSIRからの位置解決は通常の語のビューだけを選びます。
[新しいSIR home](automatic-sir-metadata.ja.md)は明示的に拒否したままで、frontも
生成しません。binding版は追加テストで直接検証しており、新しく受け入れたソースの実行では
ありません。この段階でIcarus差分の解消やP1完了とは扱いません。

次はscoped/captured homeの不変bindingへの解決と、root/block/子の環境全体の
一括構築を行い、字句的extent全体を保持し、制御フレームとは独立に実行所有権を解放します。
refには親の現在のbinding表へのポインタではなく、不変のscopedな参照先と権限が必要です。
guard/watchは待機元の実際の論理activationで依存を解決します。宣言の初期化、引数bindingの
初期化、コピー戻し、return/disable/programの解放、型付きルートを揃えてから実行拒否を外し、
frontの捕捉を有効にします。管理するeventなどのフィールドは実際の提供元も必要です。
