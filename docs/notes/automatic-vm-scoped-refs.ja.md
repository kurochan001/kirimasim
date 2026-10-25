# VM の不変な scoped ref

[English](automatic-vm-scoped-refs.md) が正。状態：参照の部品を実装し直接検証、2026-10-25。
Interpreter への全体の接続は残作業です。

## 契約と表現

承認済みの [automatic 捕捉の契約](automatic-capture-proposal.ja.md) 1〜4・6〜8 のための
参照先解決を実装します。[承認記録](p1-execution-contracts-2026-10-25.ja.md) が対応付ける
SIR_SPEC 7/8、RUNTIME_ABI 4/6、IEEE 1800-2023 6.21・9.3.2・13.5.2 が根拠です。
実装とテストは独自作成です。外部の実装・テスト、新しい依存、公開 C ABI、SIR の版、
物理フレーム ABI の変更はありません。

`ActivationOrigin` は実際の kernel binding と、それを宣言した plan・frame 行を持ちます。
捕捉と子孫への転送では、途中の捕捉行へ置き換えず、readonly の制限も含めて元を保ちます。
親が return したり同じブロックに再進入したりしても、子孫の参照先は変わりません。
plan が借りる SIR は不変とし、環境・参照表がなくなるまで plan のアドレスを変えません。

`FrameLayout` を Interpreter と参照解決で共用し、通常フレームの位置を1か所で計算します。
参照の選択用に非公開の論理ワード位置も持ちます。Scoped・Capture 行に通常の値のワードや
通常の typed root は割り当てません。従来の通常配列・wide 値・ref/container の配置は維持します。
これは実装上の位置であり、P2 のフレーム ABI を決めるものではありません。

`ScopedReferences` は plan の所有行を論理位置で並べ、実際の scope 内の field 番号へ対応付けます。
記述子は binding・宣言元の shape/行・選択位置・作成時の深さを持ちます。
参照を進めると、論理位置から実際の field・配列要素へ解決し、同じ字句的 extent に留まることと
readonly を確認します。SIR の隣の行が kernel でも隣の field とは仮定しません。
通常行・配列の仮置きワードや別の字句的 scope への移動は拒否します。

表は32ビットの添字と32ビットの世代を使い、利用時には世代と kernel binding を検証します。
body が持つのは記述子の ticket であり、参照先の値の所有権ではありません。
scope の巻き戻し・return で制御フレームの回収とは独立に ticket を解放します。
同じ body の同一位置・権限・深さの記述子は共用し、解放後は世代を増やして再利用します。
最大世代に達した項目は再利用しません。body 同士の ticket は独立です。
表は永続する lease を取らず、無効になる借用 ref の寿命を延ばしません。
隣の field を選択するときだけ使う短い lease も権限を維持します。

shape と ticket の割り当ては注入可能な PMR resource を通します。失敗時に ticket を公開したり、
再利用項目を消費したり、空の所有者行を残したりしません。再試行できます。
後始末では既存の項目を詰めて消すだけにし、noexcept メソッドから例外を漏らしません。
resource と store は表より長生きさせます。数値 token は Context 間で移せません。
shape と型付き origin は異なる store を拒否しますが、任意の数値の出所を判別できるとは主張しません。

`FrameValue` は binding 内の物理開始ビットと明示幅を持てるようにしました。
末尾ワードや shortreal で、誤って field 全体を要求しません。読み書きは従来の kernel の
単一ストレージ・通知境界を通ります。不変の借用 ref ビューは W4 の両側の参照 payload だけを
コピーし、binding の書き換えを拒否します。参照先の SystemVerilog の値はコピーしません。

## 検証と限界

独自11テストで、幅1〜257の配列ワード選択57通り、物理位置付き部分書き込み、shortreal のビット、
field 順の並べ替え、論理位置の穴、親の return/再進入、多段転送、readonly/writable の共存、
無効・古い・別 store の識別子、body ごとの後始末、記述子2項目でのブロック再進入10,000回、
shape/ticket の各割り当て失敗点、不変 ref payload を検証します。
既存の環境・値のテストも合わせた重点39テストが通ります。

[コンパイルした故障注入](../measurements/2026-10-25/scoped-reference-negative-controls.json)の
9種類の新しい故障は、世代検査の省略、要素・物理ワード・狭い幅の取り違え、extent 越え、
異なる権限の共用、重複排除の省略、scope ticket の解放漏れ、子 plan への参照先変更です。
移動した配列配置・環境 origin・通知/要素選択の4故障も実行し直します。
ビルド失敗は動作の故障を検出した結果に数えません。

`589bd26b`の[local CI全体](../measurements/2026-10-25/scoped-reference-final-validation.json)は
全21項目成功です。dev/ASan/UBSanは4,661/4,663、releaseは4,662/4,664成功、
失敗0・従来の2件スキップで、SIRダンプ29件・既存PerfCount基準も変えず通ります。
[通常フレームの費用の重点観測](../measurements/2026-10-25/scoped-reference-speed.json)は
交互5組で、調整後wall timeが変更前`652159b9`の0.2359秒から変更後`589bd26b`の0.2388秒へ
約+1.3%、カウンタとpeak RSS中央値9.1875 MiBは同じです。小標本で信頼区間ではなく、
通常のシミュレータ速度比較でもありません。scoped refの実行・標準4ベンチ・他シミュレータ・
Linuxは時間を測らず、予算も変えません。

この**参照記述子**の全32ビット世代を消費する検査は**未確認**です。
以前の kernel ストレージの全世代検査は別の表の証拠です。
確認方法は、実際の世代上限まで同じ記述子を取得・解放し、再利用停止と古い token の拒否を
確かめることです。製品に世代を減らす隠し設定は導入しません。

[87 probe の再比較](../measurements/2026-10-25/icarus14-scoped-reference-probes.json)ではnative/14.0の
全判定と成功時の判定対象バイトが従来どおりで、過去の13.0記録に対する成功差分は22件残ります。
正確なコミットを展開した[P1母集団](../measurements/2026-10-25/scoped-reference-population.json)は
1,451/1,466成功、失敗0・未対応15件、固定した
[sv-tests](../measurements/2026-10-25/scoped-reference-svtests.json)は244/344で全native判定が不変です。
sv-testsは最初の展開に設定が欠けて実行前に失敗し、固定コミット全体を展開し直して検証しました。
この2群のLinux・他シミュレータは測り直しません。有限のprobeはIcarus全範囲の証明ではありません。
既存の期待値・ダンプ本文・PerfCountの予算は維持します。

Interpreter が現在使うのは共通の通常レイアウトだけです。新しい activation メタデータを拒否する
ロード時の検査を維持し、環境と scoped ref の記述子はまだ Interpreter から構築しません。
ソースの fork 捕捉・automatic event は未対応で、Icarus の差分を解消したとは数えません。
次に plan/環境と VarsId、scope 命令、fork の一括準備、call の入出力コピー、return/kill/program の
後始末、guard callback の所有権、静的/動的な待機依存、型付き ref root を接続してから拒否を外します。
その後にソースの lowering を有効にし、承認済みの全体のソース行列を検証します。
