# VPI_SPEC：VPI

[English](VPI_SPEC.md) · 英語版が正本で、これはその日本語版です。食い違ったときは英語版に従います。

**改訂（2026-10-24）：** 旧P2→P3・旧P3→P4・旧P4→P5と付け替え、新しいP2（SIR v1）を設けました（tier番号は不変）。[対応表](../notes/phase-renumbering.ja.md)。

**段階の範囲の修正（2026-10-23承認）：** [P1_SCOPE](P1_SCOPE.ja.md)に従い、
後続段階・対象外のうちIcarus対応に必要な部分をP1へ前倒しします。既存の意味の規則は
保ち、不足する詳細は実装前にレビューします。以下の段階表記には本修正を適用します。

優先度：Icarus対応に必要な部分はP1、それ以外はP4。
状態：**最初の実行契約を2026-11-01に承認済み。**

保守者が[P1のVPI契約の1〜6節](../notes/vpi-p1-proposal.ja.md)を承認しました。
本書、CLI 2、SCHEDULER_SPEC 7/17、RUNTIME_ABI 9.6の有効な詳細改定です。
実装状況と有限の参照一覧は、承認とは区別します。

## 対応する VPI の関数

最初のhost API、明示した`--vpi=<file>`の読込み、startup、システムtask/関数の登録は
契約2〜3節に従います。未対応のAPIの形は診断し、この部分だけでVPI全体の同等性は示しません。

## オブジェクト

静的reflection、引数評価時の値と書込みbinding、変換、Context所有、handle/iteratorの
寿命、エラーは3〜5節に従います。最初の変数書込みを超えるnet書込みの統合は残作業です。

## コールバックとその領域

4〜5節は既存の時刻callbackの領域を保ち、同期的な値変更の再入、登録の保持、取り消し、
終了処理を定めます。5節で具体的なallowlist項目を承認した、guard付きのcurrent-Context
ポインタ1個の既存例外を適用します。

受入れには6節のソース・寿命・不正入力・コンパイル変異・構成別の検査を要求します。
[参照一覧](../notes/vpi-reference-inventory.ja.md)は可用性の証拠で、native実装の証拠ではありません。

実装状況：[第1層の最初の実行範囲](../notes/vpi-execution.ja.md)を接続済みです。
[検証記録](../notes/vpi-validation.ja.md)と、未対応API・型の境界を参照してください。
