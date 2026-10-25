# 実行時indexのeventを拒否する旧入力の移行

[English](indexed-event-expectation-migration.md)が正。Status: **approved (2026-10-26)**。
利用者は、実装と分けた以下の期待値移行だけを承認しました。
管理eventの命令間safepoint修正案やautomaticの既定event生成を承認するものではありません。
以下は元の提案と承認前の検証記録です。`src/`変更を含まない独立コミットで移行し、
全CLIと全local CIを再実行します。

## 適用と検証

`c53ccfe7`に承認を記録し、`28f5c972`で`src/`変更を含まない3ファイルだけの移行を適用しました。
生成規則の版19と全ソース入力は不変です。旧入力のSHA-256が変わらないこと、受理項目1個の追加と
診断6行の削除だけで、他の全診断が保たれることを確認しました。独立したCLI全14件も通りました。

[移行後の検証](../measurements/2026-10-26/indexed-event-migration-validation.json)はlocal CI全21確認成功です。
dev/ASan/UBSanは5,139/5,141、releaseは5,140/5,142で、各失敗0・既知スキップ2件です。
既存dump・PerfCount・生成件数も通りました。この承認は別のevent命令間safepoint修正案を
有効にするものではありません。P1は未完了です。

## 理由と証拠

IEEE 1800-2023 7.4.5・9.4.2・15.5、承認済みautomatic捕捉契約5/7、
SIR_SPEC 3.2.1/7/8と型付きeventのVM接続により、固定配列からloadしたevent選択が必要です。
名前付き待機は具体的対象を1回選択し、後からindexを変えるだけではtriggerになりません。
既存の恒久eventを選ぶために、管理対象の新規確保や新しい回収時点は必要ありません。

frontの新しい接続は、既存の型付きplace/index変換を使い、妥当な定数static選択の短い表現を
保ちます。変更していない旧ソースは終了0・出力なしで、従来のCLIの情報行フィルタ後の診断も
ありません。別の独自smokeソースで、index変更後も元の対象を時刻2に待つこと、
不正な選択の`.triggered`がfalseとなること、65bitのoverflow遅延でもnull triggerが
何も予約しないことを確認しました。新規ソース行列全体と全CIはこれから検査し、
この時点でP1全体やautomatic eventへの対応済みとは扱いません。

AI_POLICYの期待値の規則は、変更の理由と規格の節を示し、人が承認すると定めています。

**検証更新（2026-10-26）：** [ソース行列](indexed-event-execution.ja.md)の全160設計と方針境界16件が通り、
コンパイルした4故障も検出しました。全CIでは予告した旧CLI期待値の失敗を保持しており、
その検証時点では以下の移行は未承認・未適用でした。

## 依頼する変更の全件

`src/`変更を含まない別コミットで、次だけを変えます。

1. `tools/gen_unsupported_matrix.py`の`ACCEPTED`に`event_var_index`だけを追加します。
   ソースの生成結果は変わらないため、生成規則の版19は保ちます。
2. `tests/cli/unsupported_matrix.accepted`を再生成し、その名前1個を追加します。
3. `tests/cli/unsupported_matrix.stderr`を再生成し、`event_var_index.sv`の
   未対応index診断6行（7・10・13・17・22・27行。列は7、22/27行だけ9）だけを除きます。
4. `tests/cli/unsupported_matrix/event_var_index.sv`は、全6実行場所を含め1byteも変えません。
   他の入力・受理項目・診断も保ちます。置き換えの`.expected`は追加しません。
   既存CLI実行器が、受理した旧入力に終了0・標準出力空・情報行以外の標準エラー空を要求します。

新規の独自ソース行列が通ってから適用します。生成器、旧ソースのbyte不変、
診断ちょうど6行と受理項目1個の変更を確認し、全CLIと全local CIを実行します。
承認前に旧失敗を黙って除外しません。依存・性能予算・無関係の期待値は変えません。
