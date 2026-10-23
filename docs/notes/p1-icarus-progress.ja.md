# Icarusの機能範囲を目指すP1の拡張

[English](p1-icarus-progress.md)。状態：進行中、2026-10-23。

到達点は、Icarus Verilog 13.0以上の機能をP1で支えることです。
全面的な同等性は**未確認**です。従来の合格率や、下の小さなプローブだけでは
判定できません。P3・P4や対象外の機能を前倒しする
[範囲の修正](p1-icarus-scope-proposal.ja.md)は2026-10-23に承認され、
[P1_SCOPE](../spec/P1_SCOPE.ja.md)へ反映しました。

実装と検査を2回進め、次を追加しました。

- 共通カーネルによる無書式の `%u`・`%z` 出力。任意の幅、ゼロ埋め、X/Z、
  ホストの語のバイト順に対応します。生成するCLI行列は132プログラム・14,256変換で、
  ファイル・遅延・packed・文字列への出力を含みます。3本のユニットテストで、
  生の語、幅の上に不要なビットがある値、実行時の書式を確認します。文字列型では
  NULを除き、packedとファイルには残します。
- 参照引数のメモリへのreadmem/writemem。const refでのファイル出力、転送、待機、
  自動変数、選択した行に対応します。生成した63本は7種類の形と9種類の格納場所で、
  両基数、範囲、多次元、列挙型、隣の行の保持を確認します。この行列で見つかった
  部分的な `ref.make` は、`Lowerer::refOf` で残りの座標を補って修正しました。
  既存の期待値は変えていません。

変更前の `499198369a07` では、バイナリと参照メモリの新しいテストが、それぞれ
`%u` と ref の `$readmemh` の未対応エラーになります。今回の実装では通過します。
P1_FEATURESの日英の新しい実行例も実行して確認しました。

拡張した作業ツリーのローカルCIは、dev・release・ASan・UBSanの構成・ビルド・
テスト、releaseフラグ、禁止機能、SPDX、スイート、グローバル状態、README件数、
clang-format、clang-tidyが通過しました。devは1,572件中1,570件成功、
従来許可された2件のスキップです。既存テストの変更はアサーションと
CMake登録の追加だけです。新しいPythonのSPDXと、文書のリンクも確認しました。
公開前の検査では、未pushの各コミットについて既存の期待値の変更も検査します。

## 比較の再実行

比較用のコンパイラとランタイムの表示は
Icarus `14.0 (devel) (s20260301-411-gca6b8d376)` です。この回では正確な13.0版の
実測はしていません。自作の入力と実行結果だけを使い、Icarusのソース・テストは
読んだり取り込んだりしていません。

```sh
python3 tools/icarus_feature_probe.py \
  --kirimasim build/dev/src/driver/kirimasim \
  --output build/p1-icarus-probes.json --require-parity
python3 tests/cli/run_binary_format_tests.py \
  --kirimasim build/dev/src/driver/kirimasim \
  --iverilog iverilog --vvp vvp --container initial \
  --api write --api display --api fwrite --api fdisplay
```

現在の作業ツリーで最初のコマンドは終了コード1になります。16本の自作プローブ中、
Icarusで通りkirimasimで通らないものが8本あります。標準の仕組みの7本は、
動的配列、キュー、クラスのオブジェクト、program、MOS、双方向スイッチ、
specifyパスです。もう1本の `wire real` は
[Icarus独自拡張](https://steveicarus.github.io/iverilog/usage/icarus_verilog_extensions.html)で、
厳密なslangは正しく拒否します。対応には明示した互換方針が必要です。

2番目はIcarusで4プログラム・432変換のバイト単位の一致を確認します。
他の呼び出し場所と文字列・packed出力先の根拠は、kirimasim用の大きい行列です。
Icarusはバイナリの文字列結果のNULで警告し、結果を途中で切りました。
kirimasimは承認済みの文字列の値の規則に従います。この差は比較で隠していません。

プローブは一覧の出発点です。VPI・FST・SDF・タイミングチェック・他の独自拡張・
組み合わせの独自の入力も必要です。両方で通らない連想配列の例やIcarusで通らない
refメモリの例から、その仕組み全体の対応は判断できません。検査を足しても、
この小さな一覧の成功だけで全面的な同等性を宣言しません。

公式の[13.0リリースノート](https://steveicarus.github.io/iverilog/releases/v13-0-release-note.html)は
背景資料であり、完全な機能仕様ではありません。

## バイナリ入力の準備

次の継続では、IEEE 1800-2023 21.3.4.3・Table 21-7と現在の仕様を照合しました。
一般的な空白処理と、転送幅・途中のフィールドの選択には明記した修正が必要なので、
[バイナリ入力の修正](binary-scan-proposal.ja.md)を作成しました。2026-10-23に
段階の範囲変更とともに承認され、SYSTEM_TASKS 6.3・SIR_SPEC 8へ反映しました。
入力実装は次節に記録し、既存の実行期待値は変更していません。

自作の[観測ツール](../../tools/binary_scan_probe.py)は448ケースを持ち、実行の失敗と
観測結果を区別します。幅0を除く部分は374件の観測を完了し、承認済みの本仕様の規則と
一致しました。幅0の動作には差があり、まとめた実行では既存のIcarus開発版2種類の
ネイティブな異常終了もありました。制限は修正案に記録しています。レビューの材料であり、
規格適合や全面的な同等性を主張するものではありません。

## バイナリ入力の実装

3回目では承認済み規則を共通カーネルの `readBinary` へ実装し、SIRの出力先幅を
渡します。定数・実行時の書式、ファイル・packed・文字列入力、10種類の呼び出し場所が
同じ意味を使います。CLI行列は40プログラム・17,120変換、継続の2プログラム、拒否する
40組を確認します。新規ユニットテスト4本は生バイト、不足する各バイト、幅、マスク、
抑止、型を確認します。既存スイートの本体と実行期待値は保持しています。

```sh
python3 tools/binary_scan_probe.py \
  --kirimasim build/dev/src/driver/kirimasim --engine kirimasim \
  --require-kirimasim --output build/binary-scan-all-implemented.json
```

本仕様の448件すべてが一致しました。幅0を除く比較も両ツールで374件ずつ全て一致します。
初期プローブの差は8件になりました。標準の7件は動的配列・キュー・クラス・program・
MOS・双方向スイッチ・specifyパスで、他に `wire real` 独自拡張が残ります。
全面的な同等性は未確認です。次の[program実行の修正案](program-execution-proposal.ja.md)は
`$exit` の説明の食い違いと実行メタデータの不足を解決します。

## 仕組みの比較範囲を拡張

`tools/icarus_feature_probe.py --extended` は初期16件を保ち、自作の22件を加えた
計38件を実行します。`--only <名前>` で個別に選べます。測定した比較用の開発版は
Icarus `14.0 (devel) (s20260301-411-gca6b8d376)` で、正確な13.0版ではありません。

```sh
python3 tools/icarus_feature_probe.py \
  --kirimasim build/dev/src/driver/kirimasim --extended \
  --output build/p1-icarus-extended.json --require-parity
```

Icarusで通りkirimasimで通らないプローブは26件です。従来の差の細分化に加え、
イベント付きNBA、実行時の配列添字を持つ遅延NBA、入れ子のpacked選択への書き込み、
ビット選択のポート接続を通る強さなどを含みます。これは言語機能数でなくプローブ数で、
検査を増やすと実装の回帰がなくても増えます。

遅延を持つネット宣言代入は両方で通過します。別途調べた代入のないネット宣言の遅延と、
配列スライス代入は、今回の測定では両方で拒否されました。クラスの仮想メソッドの例は
Icarusで終了コード0ですが、独自テストの派生側の期待値9でなく基底側の3を出しました
（8.20）。レポートは正常終了での結果不一致とネイティブの実行失敗を区別し、期待する
バイトと実測バイトを併記します。どの結果も、その仕組み全体の対応・非対応を証明しません。
他シミュレータのソースは読んでいません。

バイナリ入力のdev回帰は1,575件成功、既知の2件スキップです。空白を飛ばす・出力先の
幅を無視する・不足するフィールドを代入する、という3種類の誤実装を一時的に入れると、
新しいバイナリのユニットテスト4本がそれぞれ検出し、元へ戻すと通過することも確認しました。

バイナリ入力の最終ローカルCIはdev・release・ASan・UBSan、releaseフラグ、禁止機能、
SPDX、スイート、グローバル状態、未pushコミットの期待値保護、README件数、clang-format、
clang-tidyが全て通過しました。programの実装は詳細修正案の承認待ちです。
