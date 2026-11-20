# P2設計レビュー用記録 — 高速化後も観測できる信号変化と待機動作

2026-11-21、kirimasim 0.1.3 の `main`（`2da5cea30d7e4e780135274daeafb1d5dd978609`）を macOS arm64 の release ビルドで確認した。対象は現在の第1層インタプリタと実行カーネル。既存の関連テストは **44件中44件成功**。別途、同じ時刻に信号を `0 → 1 → 0` と変える小さな回路でも、kirimasim と Icarus の両方が立上りと立下りを各1回検出した。

## 確認した振る舞い

- **同一時刻の途中の変化を観測できる。** パルスの最終値が初期値に戻っても、先に登録した立上り待ちと立下り待ちがそれぞれ再開した。波形については、別の既存テストで `--wave-glitches` 指定時に同一時刻の `1 → 2 → 1` がVCDに順番どおり残った。[波形テスト](https://github.com/kurochan001/kirimasim/blob/2da5cea3/tests/suite/proc/dump_wave_glitches.sv)
- **クロッキングブロックは採取時点の値を保持する。** `input #1step` では同じ時刻にActiveやNBAで値が変わり、再びクロックが立ち上がっても、最初のサンプルを参照できた。次の時刻ではサンプルが更新された。[クロッキングブロックのテスト](https://github.com/kurochan001/kirimasim/blob/2da5cea3/tests/suite/cb/input_same_slot.sv)
- **駆動強度だけの変化を区別できる。** 解決後の論理値が同じでも強度を監視する内部の待機には変化が届き、通常の値変化待ちやVCDに余分な変化は出なかった。[強度待ちのテスト](https://github.com/kurochan001/kirimasim/blob/2da5cea3/tests/unit/strength_selection_wait_test.cpp#L121)・[強度の通知と保存のテスト](https://github.com/kurochan001/kirimasim/blob/2da5cea3/tests/unit/strength_store_test.cpp#L150)
- **待機の打ち切りが効く。** `disable fork` で、親が先に終了した孫プロセスも打ち切られ、予定していた遅延後に再開しなかった。カーネルでは終了時に待機の世代番号を進め、古い起床予約を無効にする。[シミュレーションテスト](https://github.com/kurochan001/kirimasim/blob/2da5cea3/tests/suite/proc/disable_fork.sv)・[Scheduler::kill](https://github.com/kurochan001/kirimasim/blob/2da5cea3/src/kernel/sim.cpp#L421)

## 高速経路で確認した箇所

単純な連続代入の高速経路 `runTransfer` も `writeSlot` から共通スケジューラに書き込む。カーネルの64ビット以下の書き込み経路は4状態の変更前後の値を比較し、エッジ待ちの判定に両方を渡す。クロッキングブロックやVPIなど、信号を観測する機能の登録状況が通知経路の選択に関わる。[VMの転送処理](https://github.com/kurochan001/kirimasim/blob/2da5cea3/src/vm/vm.cpp#L4819)・[VMの書き込み先](https://github.com/kurochan001/kirimasim/blob/2da5cea3/src/vm/vm.cpp#L2133)・[カーネルの書き込みと通知](https://github.com/kurochan001/kirimasim/blob/2da5cea3/src/kernel/sim.cpp#L1952)

## 再実行手順

対象コミットの kirimasim リポジトリ直下で実行する。確認環境は AppleClang 21.0.0.21000334、CTest 4.4.4、Icarus Verilog 14.0 開発版（`s20260301-411-gca6b8d376`）。

```sh
cmake --preset release
cmake --build --preset release
ctest --test-dir build/release -j 6 --output-on-failure --no-tests=error \
  -R 'suite/cb/|Strength(Wait|Store)\.|Scheduler\.(FourStateEdgesMatchTheReference|PosedgeOnFourStateSlots|KilledProcessNeverResumes|Mixed.*Wait.*FirstItem)|suite/proc/(disable_fork|dump_wave_glitches)$|suite/sched/named_fork_disable_sequential_returned_scope$|Vpi.*(Change|Callback)'
```

このコミットでの出力は `100% tests passed out of 44`。次のパルス比較はその44件とは別に実行した。

```sh
(
  set -eu
  pulse_dir=$(mktemp -d)
  trap 'rm -rf "$pulse_dir"' EXIT
  cat > "$pulse_dir/pulse.sv" <<'SV'
module top;
  logic line = 0;
  int rises, falls;
  initial begin @(posedge line); rises++; end
  initial begin @(negedge line); falls++; end
  initial begin
    #1; line = 1; line = 0;
    #1; $display("line=%0d rises=%0d falls=%0d", line, rises, falls);
    if (line !== 1'b0 || rises != 1 || falls != 1) $fatal;
  end
endmodule
SV
  build/release/src/driver/kirimasim "$pulse_dir/pulse.sv"
  iverilog -g2012 -o "$pulse_dir/pulse.vvp" "$pulse_dir/pulse.sv"
  vvp "$pulse_dir/pulse.vvp"
)
```

両方の標準出力は `line=0 rises=1 falls=1` で、終了コードはともに0。

## P2レビューへの引き継ぎ

この結果は、現行の対応機能についてP2に進む判断を支持する。検証範囲は上記のテストとコード経路に限られる。並行アサーションなどの未実装機能と、P3で導入する生成コードの同等性は今後の確認事項となる。

P2の最初の設計レビュー（M0）には、クロックの立上りと同じ時刻に `q <= d` が実行され、並行アサーションが更新前の `q` をサンプリングする例を持ち込む。SIRが観測対象と採取時点をどう表し、カーネルがいつ値を保持し、最適化側がその必要性をどう知るかをたどる。P3の承認済み仕様には、解析情報が欠けたプロセスを最適化対象から外し、初版では最適化した回路内の書き込みも共通カーネルを通す方針がある。この例でP2の設計とその方針の接続を確認できる。[P2設計案](https://github.com/kurochan001/kirimasim/blob/2da5cea3/docs/notes/sir-v1-proposal.ja.md)・[P3のコード生成仕様](https://github.com/kurochan001/kirimasim/blob/2da5cea3/docs/spec/CODEGEN_SPEC.ja.md#L57)
