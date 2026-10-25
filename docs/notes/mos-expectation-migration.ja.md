# 既存 nmos 拒否入力の期待値移行

[English](mos-expectation-migration.md) が正。Status: **proposal (not approved)**。

## 理由と実測

IEEE 1800-2023 28.7/28.10/28.13/28.16 と承認済みの
[MOS/switch 契約 1/6/7](mos-switch-proposal.ja.md) に沿い、6 種の方向付き
MOS を front から既存 SIR 命令と共有カーネルへ接続した。独自ソーステストで、
静的強度、制御式の捕捉、配列データの選択、プリミティブ配列、三状態の初期
side フィールド、慣性遅延を確認している。契約の部分実装であり、受動網、
登録したネットの force、完全な強度区間の表示は未完成である。

変更していない `tests/cli/unsupported_matrix/switch_mos.sv` は終了コード 0、
標準出力なし、警告・エラーなしとなる（通常の情報バナーと終了時刻のみ）。
module・generate・子 module の全インスタンスが受理される。特定の入力の形で
拒否を維持するのは CLAUDE.md の機構単位の規則に反する。古い拒否期待を廃止し、
同じ入力を無出力で正常終了するテストとして継続する。

## 承認を求める変更の全件

人の承認後、`src/` を含めない別コミットで、次だけを実施する。

1. `tools/gen_unsupported_matrix.py` の `ACCEPTED` に `switch_mos` だけを追加する。
   ソース生成版 19 とソースの生成規則は変えない。
2. `tests/cli/unsupported_matrix.accepted` を再生成し、この名前だけを追加する。
3. `tests/cli/unsupported_matrix.stderr` を再生成し、`switch_mos.sv` の
   12 行 10 列・4 行 8 列・9 行 8 列の 3 診断だけを削除する。
   双方向の `switch_bidirectional` の 3 診断と、それ以外はすべて保つ。
4. 全入力ソースをバイト単位で保持し、ほかの期待値も変えない。標準出力の
   期待ファイルは追加しない。既存 CLI ランナーが、隣接 `.expected` のない
   受理入力に無出力での正常終了を要求する。
5. CLI 全体と local CI を再実行し、実際の結果を記録する。

AI_POLICY は期待値の変更に理由・規格節番号の提示と人の承認を要求する。
今回の申請はこの移行だけであり、別の仕様変更・依存・イベントの命令単位
safepoint・公開・push を承認するものではない。まだ適用していない。

[未適用の変更全体](mos-expectation-migration.patch)は `git apply --check` が通る。
受理名を足したメモリ内の生成では、全ソースを保持し、生成する一覧2ファイルだけが変わる。

## 移行前の全検証

[実装記録](source-mos-execution.ja.md)の全local CIは21項目中17成功。4構成の
テスト項目は `cli/options` だけが失敗し、出力にもこの未変更入力と、
欠ける診断がこの3件だけと記録される。他のCTestは通過、または同じ既知スキップ2件。
移行は未承認・未適用のままで、全CIの成功とは扱わない。
