# event宣言の初期化とobjectの共有

[English](event-initializer-execution.md)が正です。状態：2026-10-27、実装と対象検査が通りました。
承認済みの過去の拒否期待値3件を別コミットで移行し、全体検証を続けます。P1は未完了です。

## 契約と一つの変換入口

根拠は承認済みautomatic-capture契約5/6、SIR_SPEC 7.4/8、RUNTIME_ABI 6、
IEEE 1800-2023 6.8/6.21/10.9/13.5/15.5です。対応する入力/初期値は、新変数の待機より前に
同じevent objectを共有します。実装・テスト・故障は独自で、他のシミュレーターのソース/テスト、
規格本文/例、新しい依存やインストールは使っていません。

入力専用だったhelperを`Lowerer::lowerEventCopy`へまとめ、入力copy-in・遅延snapshot・
宣言初期化が同じ入口を使います。名前付き変数、固定配列の選択、nullは既存の型付きplace/indexを
使います。コピーに`event.new`は出さず、nullはゼロです。通常のevent式や再関連付けを
有効にする入口ではありません。

automatic scalarは宣言へ入るたびにこの入口を使い、static scalarは従来の時刻0前の
初期化processで一度だけ使います。承認済みのstatic初期化順序と位置付き診断を保ちます。
他のscalar型は従来の変換経路と診断位置のままです。

`assignArray`に、二つの宣言初期化呼び出しだけから`initializeEvents`を明示します。
assignment patternの再帰でも許可を引き継ぎ、末端で同じevent値変換を使います。
他の呼び出しの既定値はfalseです。既存の配列全体のコピーは変更していません。
これはfrontの変換文脈であり、実行時オプションや値の別の意味ではありません。
入れ子・反復・既定nullのpatternも、既存の左端からの走査と右辺評価後の書き込みを保ちます。

VM/kernel、SIR命令/版、C ABIは変更していません。既存の型付きfield、SSA root、転送pin、
lexical extent、保留操作が、呼び出し・親のreturn・fork子孫・回収中も共有objectを保持します。
VMにソースの実行場所を判定する分岐は足していません。

## 期待値移行前に完了した検査

[生成器](../../tools/gen_event_initializer_matrix.py)は208設計を追加します。
8か所とlegacy/managed値を組み合わせたscalar/添字/nullの64件、副作用を持つ添字による
static初期化の一度だけの評価8件、automatic/static配列pattern 64件、module/program・
全3join・3待機・legacy/managedの起源・Active/NBA triggerによる保持子孫72件です。
automaticは時刻を進めず宣言へ繰り返し入り、子孫の検査では親が戻れる状態でchurn中に回収します。

[新しい契約検査](../../tests/tools/check_event_initializer_contracts.py)は位置付き拒否14件
（scalar代入/比較、不正な数値整形、pattern再代入、null待機）と、SIRの生成数4設計を確認し、
別名/nullが余分なobjectを生成しないことを検査します。新規CTest 210件が通りました。

コンパイルした7故障をすべて検出しました。新規の初期化2件、既存のevent入力4件と、
既存の配列pattern snapshot故障です。snapshot故障は同じコンテナsnapshotを取り除くもので、
局所型名のリファクタに合わせてanchorだけを変更し、既存8検査で引き続き検出しています。
ソースを完全に戻した後、選んだevent/集約値の386検査がすべて通りました。
[故障の記録](../measurements/2026-10-27/event-initializer-controls.json)にdigestと検出テストを残します。

実装コミットでは既存期待値を保ちます。過去の初期化拒否3件は、人の承認済み移行を別コミットで
適用します。その後に全CIと生成件数を記録します。この時点では統合全体の成功を主張しません。

## 制限

scalarとassignment patternの宣言初期化を追加した変更です。既存の配列全体のコピー処理は
そのままで、既存の待ちがある変数の再関連付けの意味を新しく決めていません。
通常のscalar再関連付け、比較、event値の条件式/戻り値、automatic structのevent field、
eventを含む動的コンテナは残作業です。性能予算、final回収の除外、保守的なSSA寿命は変えません。
有限の行列や比較probeではP1全体の同等性を示せません。
