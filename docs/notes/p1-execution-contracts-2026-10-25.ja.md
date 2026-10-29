# 2026-10-25承認のP1実行契約

[English](p1-execution-contracts-2026-10-25.md)が正です。状態：**承認済み（2026-10-25）**。
配列メソッドの検証後にまとめて提示した具体的な6案を、人が承認しました。
下記の実行契約の節を仕様修正として採用します。観測や根拠は過去の記録として保ち、
テストの期待値を他プロジェクトからコピーしません。英語の契約本文が正です。


**後続の公開状況（2026-10-26）：** 別件のtran期待値移行は承認後に `77c9d188`、
automatic eventのmetadata期待値移行は承認後に `4ba6b63d` で適用し、
[main `304e1aec`](automatic-event-main-integration.ja.md) に公開済みです。
末尾の追記にある実装/移行状態は、その承認時点の記録です。承認済みの実行契約自体は引き続き有効です。

- [固定配列ポート・要素ごとの解決、1–6節](fixed-array-ports-proposal.ja.md).
- [automatic活動の共有・捕捉・自動イベント、1–8節](automatic-capture-proposal.ja.md).
- [イベント制御付きNBA：スケジューラ・寿命・SIR/カーネル境界の契約](event-nba-proposal.ja.md).
- [遅延付き三状態ゲート：修正案の節](delayed-gate-side-proposal.ja.md).
- [完全な強度のMOS・受動網、1–9節](mos-switch-proposal.ja.md).
- [クラスの識別・初期化・所有・実行、1–10節](class-runtime-proposal.ja.md).

既存のP1範囲の詳細を定める承認であり、完了条件は変えません。各仕様から該当する契約を
明示的に参照します。列挙した節では、古い未確定・除外の文より承認済みの修正を優先し、
関係しない既存規則は保ちます。型の合法性、slangの厳格な診断、クリーンルーム、公開C++20、
tier 1のC++ API直接呼出し、formatVersion 0、C ABI 0.3は維持します。

完全な遅延ドライバの公開から、機構ごとに実装します。既存テストの本体・期待値・SIRダンプ・
PerfCount予算の保護は継続し、必要な期待値移行は理由を示した別のレビューにします。
依存・道具のインストールや大域状態の例外は承認に含めません。この承認や有限の比較プローブで
P1完了とは扱いません。

## 2026-10-26承認の管理event改訂

人が別途、[管理eventのsafe point修正案](managed-event-safepoints-proposal.ja.md)の推奨案1を
選択しました。RUNTIME_ABI 6とautomatic捕捉契約5・6・8を修正し、完全なルート公開と
具体的な実行中pinの保持を条件に、同期関数やguardを含むVM命令境界でeventだけを回収できます。
コンテナheapの境界限定、DPIと終了/finalの回収禁止は保ちます。新しい回収タイミングや
ソースのautomatic event生成の有効化前に、完全公開と列挙した証拠をすべて揃えます。
実装の許可であり、完了の主張ではありません。別件の[tran期待値移行](passive-expectation-migration.ja.md)は
未承認・未適用で、期待値・依存・性能予算は変えません。
