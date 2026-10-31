# 固定配列要素のポートの期待値移行案

[English](array-element-port-expectation-migration.md)。状態：**承認済み（2026-11-01）**。

IEEE 1800-2023 7.4・23.3.3.7、および2026-10-25承認のMOS/port契約4と
固定配列契約1–2に沿い、`Lowerer::staticNetBits` が固定配列の要素を宣言順の物理位置へ対応付けます。
既存のidentity接続で個々のドライバと強度を保ちます。

変更の承認をお願いするのは、次の既存3入力の分類だけです。

- `tests/cli/unsupported_matrix/net_port_meet_array_top.sv`
- `tests/cli/unsupported_matrix/net_port_meet_array_generate.sv`
- `tests/cli/unsupported_matrix/net_port_meet_array_instance.sv`

各入力のソースのバイト列は変えず、終了2・未対応診断1行から、終了0・stdoutなし・エラー診断なしへ移します。
生成器のaccepted集合と生成済みaccepted一覧へ3名を加え、`tests/cli/unsupported_matrix.stderr` の
対応する3行だけを取り除く変更です。実装で3入力とも新しい分類に合うことを確認しました。
wired・tri0・幅変換等のほかの拒否、既存スイート、性能予算は保持します。
承認後、期待値だけを実装と別のコミットにします。pushやP1完了の承認とは扱いません。

保守者が2026-11-01に、この3入力だけの期待値移行を承認しました。

元のポート方向の警告と駆動位置のnoteは保ちます。3入力それぞれに生成した `.warnings` を添え、CLI判定器が位置・本文を完全に照合し、エラー診断は引き続き拒否します。このファイルがない既存の受理済み入力は診断なしを要求するままです。承認条件はエラー診断なしであり、slangの警告を抑制する変更ではありません。
