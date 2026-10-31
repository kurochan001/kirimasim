# class の実行：ソースからの接続

[English](class-source-execution.md)

[承認済みの契約](class-runtime-proposal.ja.md)に沿い、front から[VM の実行](class-vm-execution.ja.md)へ
ソースの class を接続しました。基本のオブジェクト・構築の境界を外した段階で、契約全体や P1 の完了ではありません。

## canonical なグラフと共通の変換

`Lowerer::discoverClasses` は、実行する命令を作る前に、参照する canonical な class の閉包を集めます。
ID は順の決まった遭遇で割り当て、map の走査順から作りません。再帰するフィールド・基底・interface より先に
仮の行を登録します。別名は同じ行を使い、異なるパラメータの特殊化は別の行になります。
必要な構築・仮想メソッド・呼ばれるメソッドを調べ、未使用の手続きの静的な初期化も落としません。
static だけの呼び出しや未使用の automatic のシグネチャから、不要なオブジェクトのレイアウトを作りません。

`classOf` は宣言元・次元・const・初期化子・継承を記録します。既存の static の slot は同じ識別子を保ちます。オブジェクト経由の static 参照も、元の宣言の格納域を使います。null を参照外しせず、受け手の式の副作用は一度だけ評価します。固定配列・構造体・コンテナ・ref も共通の格納域の変換を通します。
構築は基底、宣言元の初期化子、本体、段階の完了の順です。明示的な `super.new` より前のローカル宣言も保ちます。
固定配列の初期化子は宣言順の全要素を渡します。instance const は構築の権限で初期化し、実行回数はカーネルで検査します。

`classPlace` が receiver と添字を一度だけ保持し、通常の place・load・store の経路で数値の field ID を使います。
隠したフィールドも宣言元を保ちます。引数・戻り値・copyback は共通の呼び出し処理を使い、receiver は引数の効果より先に保持します。
fork は同じ暗黙の receiver の束縛を引き継ぎ、宣言元のメソッドの終了後も保持できます。

仮想呼び出しは全体のシグネチャと動的な表を使い、純粋仮想メソッドの架空の本体は作りません。
`super` と明示的な class の修飾は直接呼び出しです。変数の package 修飾で仮想呼び出しを失いません。
仮想 task の slot は待機しうる ABI に統一し、呼び出し元にも待機を伝えます。共変の戻り値は slot の view で受けてから
静的に選ばれた戻り値の view へ変換します。呼ばれうる実装も、通常の読み書きの呼び出しグラフへ含めます。

浅いコピーは動的な class を保って構築を再実行しません。null・比較・真偽・条件選択は既存の class の意味を使います。
関数形式の `$cast` は対象の参照を source より先に保持し、失敗時に書き込まず、型付きとリテラルの null を区別します。
`srandom` はオブジェクトの seed を設定します。対応済みの queue・動的配列の class 要素も型と所有を保ちます。

## 検証と残る境界

生成器 `gen_class_object_matrix.py` の独自374ソースは、unit/module/package/interface/program/generate、
静的・automatic のハンドル、幅1・32・65・129・257と二値・四値、固定要素、real/shortreal/string、
初期化と const、継承、隠したフィールド、interface の呼び分け、共変の戻り値、仮想 task の待機（待機しない基底の super 呼び出しも含む）、引数・戻り値、
cast の対象保持、入れ子の辺、条件選択、引数評価順、待機中とメソッド・fork 終了後の GC を確かめます。
追加7単体テストは canonical な型、架空の interface 本体がないこと、AST 破棄後の実行、コピーと seed、
null の検査時機、割り当て前の拒否も調べます。17個の変異を追加し、既存の return の lexical cleanup の変異も性質を保って移します。
実行時の後処理で隠れる差も、SIR_SPEC 7.2 の return 前の named scope の終了を制御フローから検査して捕まえます。

既存の static の境界検査のうち、`b=new` を未対応と期待していた1件は、保守者の承認を受けて、終了0・標準出力なし・
エラー診断なしを検査する期待へ変えました（IEEE 1800-2023 8.7）。元のソースと他の拒否検査は保ち、別コミットにしました。
既存スイート本体・golden 出力・性能予算は変えていません。

managed event・unpacked 構造体の instance フィールド、property/コンテナ要素の ref、property の部分書き込み・
NBA・遅延対象、class の書式、task 形式の cast、組み込みの同期・弱参照 class、パラメータ化した static 格納域は残ります。
感度によるプロセス、依存する値の変更待ち、monitor、procedural override、イベント回数付き NBA、pattern 書式、
標準外の初期値を class オブジェクトと組み合わせた設計は実行前に拒否します。依存・private フィールド・event の意味が
未接続のまま動かさないための境界です。残る仕組みを含めた class 全体の完了とは扱いません。
