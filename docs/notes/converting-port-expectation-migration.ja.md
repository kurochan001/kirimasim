# 変換を伴うnetポートの期待値移行案

[English](converting-port-expectation-migration.md)。状態：**承認済み（2026-11-01）**。

IEEE 1800-2023 4.9.6・23.3.3.7、SIR_SPEC 3.5、2026-10-25承認のMOS/port契約4は、
幅・符号・状態の変換や明示的なcastを要するinput/outputを、strongの方向付き暗黙代入として保ちます。
変換する接続をidentityの未実装判定に含めず、既存の通常のネット解決へ接続します。

承認の対象は次の既存3入力の分類だけです。

- `tests/cli/unsupported_matrix/net_port_meet_width_top.sv`
- `tests/cli/unsupported_matrix/net_port_meet_width_generate.sv`
- `tests/cli/unsupported_matrix/net_port_meet_width_instance.sv`

ソースのバイト列を保ち、終了2・未対応診断1行から、終了0・stdoutなし・エラー診断なしへ移します。
実装で3件とも新しい分類に合うことを確認しました。accepted一覧へ3名を追加し、stderr一覧から
対応する3行だけを取り除きます。元のポート方向の警告と駆動位置のnoteは保持し、隣接する3つの
`.warnings` で位置・本文を完全照合します。既存9件の同形式の警告期待値は変えません。

ほかの既存期待値・性能予算は保持します。保守者は2026-11-01に、この3入力の移行を明示的に承認しました。
期待値の変更を実装と別コミットにします。
この移行はpushやP1完了の承認ではありません。slangへの投稿は保留のままです。
