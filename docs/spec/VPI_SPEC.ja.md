# VPI_SPEC：VPI

[English](VPI_SPEC.md) · 英語版が正本で、これはその日本語版です。食い違ったときは英語版に従います。

**読み方：** [VPI](../GLOSSARY.ja.md#vpi)は登録した呼び出しやコールバックを通じてC pluginからシミュレーションを読み書きする仕組みで、実行できる範囲は[pluginの手順](../USER_MANUAL.ja.md#c-pluginのvpiを使う)から確認してください。

**改訂（2026-10-24）：** 旧P2→P3・旧P3→P4・旧P4→P5と付け替え、新しいP2（SIR v1）を設けました（tier番号は不変）。[対応表](../notes/phase-renumbering.ja.md)。

**段階の範囲の修正（2026-10-23承認）：** [P1_SCOPE](P1_SCOPE.ja.md)に従い、
後続段階・対象外のうちIcarus対応に必要な部分をP1へ前倒しします。既存の意味の規則は
保ち、不足する詳細は実装前にレビューします。以下の段階表記には本修正を適用します。

優先度：Icarus対応に必要な部分はP1、それ以外はP4。
状態：**最初の実行契約を2026-11-01に承認済み。**

保守者が[P1のVPI契約の1〜6節](../notes/vpi-p1-proposal.ja.md)を承認しました。
本書、CLI 2、SCHEDULER_SPEC 7/17、RUNTIME_ABI 9.6の有効な詳細改定です。
実装状況と有限の参照一覧は、承認とは区別します。

**変数releaseの結果の修正（2026-11-03）：** `vpiReleaseFlag`は、nullでない`value_p`に
解除後の値を返します（38.34）。既存の読取り変換と所有bufferを使い、拒否時は呼出し元の出力を
保持します。[実装と確認](../notes/vpi-release-results.ja.md)。

## 対応する VPI の関数

最初のhost API、明示した`--vpi=<file>`の読込み、startup、システムtask/関数の登録は
契約2〜3節に従います。未対応のAPIの形は診断し、この部分だけでVPI全体の同等性は示しません。

## オブジェクト

静的reflection、引数評価時の値と書込みbinding、変換、Context所有、handle/iteratorの
寿命、エラーは3〜5節に従います。[net全体の改定](../notes/vpi-net-write-execution.ja.md)で検証済みの物理権限を接続し、netの部分・遅延書込みと受動接続のdepositは残作業です。

## コールバックとその領域

4〜5節は既存の時刻callbackの領域を保ち、同期的な値変更の再入、登録の保持、取り消し、
終了処理を定めます。5節で具体的なallowlist項目を承認した、guard付きのcurrent-Context
ポインタ1個の既存例外を適用します。

受入れには6節のソース・寿命・不正入力・コンパイル変異・構成別の検査を要求します。
[参照一覧](../notes/vpi-reference-inventory.ja.md)は可用性の証拠で、native実装の証拠ではありません。

実装状況：[第1層の最初の実行範囲](../notes/vpi-execution.ja.md)を接続済みです。
[検証記録](../notes/vpi-validation.ja.md)と、未対応API・型の境界を参照してください。

**net全体への書込みの改定（2026-11-03承認）：** [net書込み契約1〜8節](../notes/vpi-net-write-proposal.ja.md)が、検証済みの物理的な権限を通す、point netの即時depositとnet全体のforce/releaseを定めます。この範囲では、最初の契約のnet書込み拒否より優先します。[nativeの範囲は実装済み](../notes/vpi-net-write-execution.ja.md)です。2023年版の直接照合は未確認です。物理origin件数の1アサーション・2ケースは別途承認を受けて移行し、ほかの既存期待値移行の規則は維持します。
