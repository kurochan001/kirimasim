# automatic event：過去の拒否チェックの期待値移行

[English](automatic-event-expectation-migration.md) が正です。Status:
**approved and applied (2026-10-26)**。人がこの具体的な 1 件の移行を承認しました。
実装とは別に適用し、すべてのソース入力を保持します。以下の提案本文と承認前の検証は
決定の記録として保ち、承認要求や失敗の記述は以前の区切りの状態を指します。

## 承認を求める具体的な変更

`tests/tools/check_automatic_capture_metadata.py` の既存入力はすべて同じ内容で
残します。`rejected` の最後の入力は、読み出さない automatic event と別の
automatic 変数 `value` を宣言し、子プロセスが `value` を読みます。現在の
「終了コード 2・標準出力なし・位置付きエラー」という期待を、この入力だけ
「終了コード 0・標準出力が正確に `7\n`・エラー診断なし」へ移します。

この入力を独立した受理ケースとして確認します。ref、const、`ref static` の
拒否ケース 4 件と既存の検査を保持し、最後の案内文を「拒否境界 4 件と
automatic event の保持 1 件」に合わせます。他の期待値、入力本体、ダンプ、
unit assertion、性能予算は変更しません。`src/` の変更を含まない専用コミットとし、
人間の Approved-by トレーラーを付けます。

## 根拠

IEEE 1800-2023 6.8 は、初期化子のない automatic event の宣言実行時に固有の
event オブジェクトを生成する規則です。6.21 と 9.3.2 は、生成した子の活動が
保持する automatic scope の寿命を定めます。承認済みの
[automatic-capture 契約 1/2/5/6](automatic-capture-proposal.ja.md) は、読み出さない
event を含む保持範囲の全 typed field の有効性を要求します。この子は初期化済みの
数値を 1 回読み、7 を表示します。以前の拒否は言語上の誤りではなく実装の境界でした。
考察とテストは独自作成で、外部実装やテストは読まず、規格の本文や例も転載していません。

## 承認前の証拠

作業中の実装は、新規生成した独自のソースケース 176 件を通過しています。
default 構築は配置 8 種 × 形状 4 種の 32 件、task の保持は module/program、
join 3 種、形状 4 種、named/mixed/triggered の待ち、即時/遅延 NBA trigger を
組み合わせた 144 件です。既存 metadata テストの最後の入力自体も、終了コード 0 で
正確に `7\n` を表示します。

[全 local CI](../measurements/2026-10-26/instruction-events-validation.json) は 21 項目中 17 成功で、
失敗した 4 構成の test 項目はいずれもこの同じ期待値 1 件です。dev/ASan/UBSan は
各 5,743 本中 5,740 成功、release は 5,744 本中 5,741 成功で、各既知スキップ 2 件です。
ビルド・方針検査・全 format/tidy・README 生成件数は通りました。コンパイルした 12 故障を
すべて検出し、復元後の関連 288 検査も通ります。通常経路のカウンタは一致し、両参照の
87 プローブも差分が 8→7 件へ減りました。費用の観測と制限は実装ノートに記録します。
この承認は P1 完了の証明、event の再関連付けや他の期待値変更の承認にはなりません。

## 承認後の検証

移行コミットは `4ba6b63d` です。入力の文字列は byte 単位で維持し、既存の拒否境界 4 件も
維持しました。[承認後の全 local CI](../measurements/2026-10-26/automatic-event-approved-validation.json)
は 21/21 成功、4 構成とも失敗 0・既知スキップ 2 件です。上の失敗数は承認前の記録です。
