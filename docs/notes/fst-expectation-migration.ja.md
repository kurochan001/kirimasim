# FST CLI 期待値の移行

[English](fst-expectation-migration.md) · 2026-11-03。

状態：**2026-11-03に保守者が既存1件の期待値移行を明示的に承認済み**です。
[FST 契約1〜8節](fst-p1-proposal.ja.md)の承認に、この移行は含めず、別途承認を得ました。

変更しない入力は `tests/cli/display_finish.sv`、SHA-256 は
`87760045f8ca4c00e31e5e84248adde4f4831d4a840a1487196f7ad4fe6b57d0` です。移行前の `tests/cli/run_cli_tests.py` の既存行は
`--wave=w.fst` でこの入力を実行し、終了2・空の stdout・FST 未対応の診断を期待していました。

既存 RunOptions 経由の native 実行は `Finish`、正確な stdout `hello\na`、
変数0件の読める FST で成功しました。この設計には静的信号がありません。
`Fst.ExistingRejectedCliInputRunsNativelyBeforeSeparateMigrationReview` が入力・出力・復号ファイルを検査します。
さらに138件の native FST テストが実際の値・時刻・alias・event・制御・force/specify・Context 分離を
検査しており、空のファイルを開けるだけで波形の正しさを主張していません。

承認された変更は、以下の**既存1行だけ**です。

```python
(["--wave=w.fst", DESIGN], 0, "=hello\na", "kirimasim: info: finish at 0 ns"),
```

入力・引数・他の既存 assertion は維持します。ファイルを作るこのケースは一時ディレクトリで実行し、
成果物の存在を確認します。空の設計の復号結果は上記で検査します。非空の値の FST、parse/dump-SIR では
ファイルを作らないこと、不正拡張子・空ベース名には新しい CLI 検査を追加します。既存 VCD トレース・
PerfCount・母集団・P1 条件は不変です。不正拡張子の診断には、従来検査する文頭を保って `.fst` を追加できます。
無関係な過去の診断は移行しません。

根拠は承認済み FST 契約1・8節、WAVEFORM の FST 改定、CLI 2.2・5です。
FST のバイナリ形式はプロジェクトの契約であり、IEEE 必須のファイル形式ではありません。
共用する既存 dump 制御の根拠は IEEE 1800 の21.7です。承認された期待値変更は本体と別コミットにします。
別途承認後に公開 CLI の `.fst` 受理を有効化しました。実際の CLI も同じ入力で終了0・正確な stdout `hello\na`・通常の finish 診断となり、固定版 reader が生成ファイルを読めました。変数0件の正常な FST では階層のテキスト変換が空になるため、空の変換結果をファイル破損とみなしません。追加の CLI 検査は129ビットの0/1/X/Z、時刻、通常/glitch モード、parse/SIR の無出力、不正名を検査して成功しました。
