# 部分ポートの実効ネット種別の期待値移行案

[English](port-kind-expectation-migration.md)。状態：**承認待ち（2026-11-01）**。

IEEE 1800-2023 23.3.3.7・表23-1、および2026-10-25承認のMOS/port契約4に沿い、
部分ポートの静的な物理bitの接続ごとに実効ネット種別を決め、暗黙のpull/supplyを一度だけ作ります。
`mergeNetKinds` の規則は全幅ポートと共通です。隣接する未接続bitの種別は変えません。

承認をお願いするのは次の6入力の分類だけです。

- `tests/cli/unsupported_matrix/net_port_meet_wired_top.sv`
- `tests/cli/unsupported_matrix/net_port_meet_wired_generate.sv`
- `tests/cli/unsupported_matrix/net_port_meet_wired_instance.sv`
- `tests/cli/unsupported_matrix/net_port_meet_tri0_top.sv`
- `tests/cli/unsupported_matrix/net_port_meet_tri0_generate.sv`
- `tests/cli/unsupported_matrix/net_port_meet_tri0_instance.sv`

ソースのバイト列は変えず、終了2・未対応診断1行から、終了0・stdoutなし・エラー診断なしへ移します。
実装で6入力とも新しい分類に合うことを確認しました。生成器と生成済みaccepted一覧へ6名を加え、
既存stderr一覧から対応する6行だけを取り除きます。元のポート方向の警告と駆動位置のnoteは抑制せず、
各入力に位置・本文を完全照合する `.warnings` を加えます。

幅変換・明示的な部分inout・uwire・forceの境界、ほかの既存期待値と性能予算は保持します。
承認後、期待値だけを実装と別のコミットにします。pushやP1完了の承認とは扱いません。
