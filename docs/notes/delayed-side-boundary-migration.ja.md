# 遅延付きsideの境界テストの期待値移行

[English](delayed-side-boundary-migration.md)が正です。状態：**承認済み（2026-10-25）、取り込み済み**。
実装検査の後、人がこの具体的な期待値移行を別に承認しました。

6案の実行契約は2026-10-25承認済みですが、CLAUDE.mdとAI_POLICYにより既存期待値の変更は
別にレビューします。分離した`codex/delayed-side-expectations`（基点`39cacb37`）には
本体の変更を含めません。

変更対象は`tests/tools/check_delayed_gate_boundary.py`です。遅延あり6ケースの未対応拒否を、
5行の厳密な出力比較へ変更します。既存の遅延なし6ケース、幅1/65/257、直接・短絡関数の
両制御、終了コードとエラーの検査は保ちます。スキップや既存suite・expected・PerfCountは
変更しません。

strong 0とweak 1の解決を確定後、Hへ変更して最小遅延前は0、遅延後は1を確認します。
HからLへの変更でも遅延前は1を保持し、遅延後はXです。各比較は1を出力し、ラベルを
settled・pending・H・L_pending・Lとします。期待値は参照実行の出力でなく承認済み契約から
導きます。根拠はIEEE 1800-2023 28.6、28.16/Table 28-9、VALUE_SEMANTICS 5.8、
SCHEDULER_SPEC 12、SIR_SPEC 5.4です。同じ4値への射影を持つ遷移の時間は、明示した
プロジェクトの選択であり、Icarusの観測で置き換えません。

`git show codex/delayed-side-expectations`と[承認済み契約](delayed-gate-side-proposal.ja.md)で
レビューできます。実装は別に行い、新規のカーネル・SIR・ソーステストと誤実装で検証します。
拒否・成功のどちらでも許す分岐やアサーションの弱化で移行してはいけません。
