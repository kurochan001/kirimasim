# automatic eventと呼び出し転送のmain公開

[English](automatic-event-main-integration.md)が正です。状態：**mainで公開済み、
2026-10-26**。実装の区切りは `304e1aec` です。
リモートmainの参照と完全なcommit IDの一致を
[公開時点の記録](../measurements/2026-10-26/automatic-event-main-publication.json)で確認しました。

## 公開した範囲

mainを `1c970f5d` から既存6コミットでfast-forwardし、実装と承認済み期待値の履歴を
別々に保ちました。

- `1d2b7e6e` / `ee71fdd0`：eventの引数/copyback snapshotと戻り値pin、部品の証拠と制限。
- `e909f022` / `954b5350`：native/SSA/転送のroot公開、物理event行の増加前の回収、
  automatic scalar/固定配列の宣言既定値、独自ソース176件・SIR32設計の証拠。
- `4ba6b63d`：別途承認した過去の拒否期待値1件の移行。全入力本体と厳格なref拒否境界4件を保持。
- `304e1aec`：承認後のCI成功、生成件数、英日検証・状態文書。

仕組みは[現在の実行と制限](instruction-event-execution.ja.md)に記録します。
automatic eventの値仮引数・初期化子の別名・automatic structのevent fieldと、
event/objectの広いインターフェースは部分対応です。依存、SIR版、C関数表のABI版は追加せず、
性能予算も変えていません。

## 検証と公開状態

[承認後のlocal CI](../measurements/2026-10-26/automatic-event-approved-validation.json)は
`4ba6b63d`で **21/21成功**です。公開した `304e1aec` はその後の文書変更だけで、
実行コードとテストのbyte列は同じです。

- dev/ASan/UBSan：各5,743本中5,741成功。
- release：5,744本中5,742成功。
- 各構成とも失敗0、既知スキップ2件：
  `suite/cb/input_sample_1step` と `suite/nets/net_port_drivers_part`。
- 全format/tidy、方針、期待値分離、生成件数の検査が成功。

`304e1aec` の [hosted CI](https://github.com/kurochan001/kirimasim/actions/runs/40319556353)は、
**2026-10-26 12:52:13 UTC** 時点で **completed / success** でした。**全14ジョブが成功**し、
Linuxのdev/release、GCC ASan/UBSan、Linux TSan、最小コンパイラ、lint、macOSの4構成を含みます。
これは確認時点の記録で、その後の結果はrunのリンクから確認できます。通常CIの成功は、
nightly fuzzの7夜連続成功や、外部比較・速度の再計測の証明にはしません。

## 過去の記録の読み方

以前のノートと測定JSONは、当時のローカル/未統合、未承認、失敗の状態を保持します。
現在の承認要求ではなく、区切りごとの証拠です。tranの古い拒否期待値は `77c9d188`、
automatic eventのmetadata拒否は `4ba6b63d` で解消済みです。この公開以降、
automatic scalar/固定配列の生成と命令境界回収を未有効とする以前の説明は過去の状態です。

[87プローブの比較](../measurements/2026-10-26/instruction-events-comparison.json)は、
両Icarus参照に対して成功する未対応差分が各7件、参照側の不一致が4/7件です。
固定母集団は1,466本中1,456成功・失敗0・未対応10、nativeのsv-testsは344本中244成功です。
これらは同じ実行実装について以前に測った数値で、今回の文書更新で再計測したものではありません。

P1は未完了です。[P1_SUPPORT](../P1_SUPPORT.ja.md)に実装済みの形と残る仕組みを示し、
有限のプローブや合格率だけでIcarus Verilog 13.0の全範囲に対応したとは判定しません。
