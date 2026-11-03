# 下書き：UDP のエッジ網羅検査に指数的な時間がかかる

[English](slang-udp-coverage-issue-draft.md) · 上流への報告用の英語版が正で、日本語版は確認用です。

状態：**未投稿の下書き。人による確認待ち**。2026-10-31 改訂。
保守者の指示で投稿は当面保留（2026-10-31）。再開の明示的な指示を待ちます。

2026-10-24 の下書きを、現在の固定版と upstream master の slang 単体での測定で更新しました。

## 要点

入力ごとにワイルドカードの保持行を 1 行ずつ持つ sequential UDP で、入力数とともにコンパイル時間が急増します。
33 入力・33 行の例は、slang 単体の CLI で 15 秒の外部制限に達しました。
master `82ed79add4bdc3bc730c00b8fe0c04b1a7714fdb` と、固定版
`e2d1021661f78b3f17d1edf7ea17f9d6f95fb168` の両方で再現しました。
この表は、各入力が単独で変化するすべてのエッジを網羅します。再現にシミュレーションは要りません。

## 確認した環境

- macOS arm64、Apple clang `21.0.0 (clang-2100.3.34.2)`。
- 改変していない上流のソースから、CMake の Release で slang 単体をビルド。
  `SLANG_INCLUDE_TOOLS=ON`、`SLANG_INCLUDE_TESTS=OFF`、mimalloc 有効。
  取得済みの依存を `FETCHCONTENT_SOURCE_DIR_*` で再利用しました。
- master の取得・確認日は 2026-10-31。固定版を先に測り、同じ単体ビルド用ディレクトリを master で再ビルドしました。
  どちらもソースに変更がないことを確認しました。ソースのコミット、実行ファイルが表示する版、
  実行ファイルの SHA-256 は測定資料に記録しています。

別の checkout での通常の単体ビルドは次のとおりです。

```sh
cmake -S slang -B slang-build -G Ninja -DCMAKE_BUILD_TYPE=Release \
  -DSLANG_INCLUDE_TOOLS=ON -DSLANG_INCLUDE_TESTS=OFF
cmake --build slang-build --target slang_driver -j 8
```

## 独自に作った再現例

次を `make_hold.py` として保存します。

```python
from pathlib import Path
import sys

n = int(sys.argv[1])
names = [f'i{i}' for i in range(n)]
rows = []
for i in range(n):
    tokens = ['?'] * n
    tokens[i] = '*'
    rows.append('    ' + ' '.join(tokens) + ' : ? : -;')
source = (
    'primitive hold_all(q, ' + ', '.join(names) + ');\n'
    '  output q;\n  reg q;\n  input ' + ', '.join(names) + ';\n'
    '  table\n' + '\n'.join(rows) + '\n  endtable\nendprimitive\n'
    'module top;\n  wire q;\n  reg [' + str(n - 1) + ':0] d;\n'
    '  hold_all u(q, ' + ', '.join(f'd[{i}]' for i in range(n)) + ');\n'
    'endmodule\n'
)
Path(f'hold_{n}.sv').write_text(source)
```

生成してコンパイルします。

```sh
python3 make_hold.py 33
slang-build/bin/slang --top top hold_33.sv
```

表は、各入力のすべてのエッジについて、ほかの入力と現在の出力の値を問わず保持を指定します。
データで出力が変わる UDP のシミュレーションの意味から、網羅検査だけを切り離した例です。

## 測定結果

各値は 1 回の実測で、その間にほかのコンパイルは走らせていません。
時間制限は測定側の上限です。いつまでも終了しないと確認したわけではありません。

- 固定版 `e2d102166`：9 入力 **0.030 秒**、11 入力 **0.284 秒**、13 入力 **3.217 秒**。
  いずれも終了コード 0、警告なし。
- master `82ed79add`：9 入力 **0.034 秒**、11 入力 **0.294 秒**、13 入力 **3.123 秒**。
  いずれも終了コード 0、警告なし。
- 網羅した 33 入力の表は両版とも **15 秒**の上限に達しました。停止するまで出力はありません。
- 同じ 33 入力の表は、両版とも `-Wno-udp-coverage` を付けても、
  別に `--max-udp-coverage-notes 0` を付けても **15 秒**の上限に達しました。
- master で `-Wudp-coverage` を有効にしても、網羅した 13 入力の表に警告はなく、
  網羅した 33 入力の表は **15 秒**の上限に達します。33 入力の表から最初の入力の保持行を除くと、
  期待した `-Wudp-coverage` の警告が速やかに出ます。診断そのものも別に確認しました。
- 元の、データで出力が変わる 33 入力の `dense_udp_coverage` プローブも、両版の単体 CLI で **15 秒**の上限に達します。
  Icarus Verilog 13.0 stable は、この元のプローブを **0.013 秒**でコンパイルし、終了コード 0、実行すると `1` を出しました。
  網羅した 33 入力の保持表は **0.012 秒**でコンパイルします。

## 原因の切り分け

両版で同じ
[`checkPrimitiveEdgeCombinations`](https://github.com/MikePopoloski/slang/blob/82ed79add4bdc3bc730c00b8fe0c04b1a7714fdb/source/ast/symbols/MemberSymbols.cpp#L1212)
が、`PrimitiveSymbol::fromSyntax` で表を構築した後、無条件に呼ばれます。
具体的な行を列挙し、それぞれを `BitTrie::contains` に照会しています。
未網羅の例が一定数集まると打ち切りますが、完全に網羅した表では未網羅の例が見つからないので、この打ち切りが効きません。

N 入力で単独の入力が変化する具体的なエッジ状態は `6 * N * 3**(N-1)` 通りあります。
この関数は、小さなワイルドカードの行ではなく、具体的な状態を辿ります。
指数的な処理量という説明はソースからの推論で、測定の増え方とも整合します。
式は状態空間の大きさを示しており、実際のループ回数を測った値ではありません。
警告の抑制と注記数の制限では、このループを迂回しません。
小さなワイルドカードの表でも扱える検査にし、網羅の診断を保つことを求める報告です。
網羅の規則は IEEE 1800-2023 29.6 にあります。その規則を保つことを求めます。

関連する [#936](https://github.com/MikePopoloski/slang/issues/936) は網羅検査の導入で、完了して閉じられています。
2026-10-31 に `UDP`、`primitive`、`coverage`、`exponential` で Issue を検索したところ、
同じ性能の報告は見つかりませんでした。重複がないと証明したものではありません。

AI の利用：この報告と独自の保持表の再現例は Codex の支援で用意しました。投稿前に人が確認します。

## 手元の証跡

- [ソース、SHA-256、子プロセスの全結果、ビルドの識別情報](../measurements/2026-10-31/slang-udp-coverage-standalone.json)。
- [2026-10-24 の測定](../measurements/2026-10-24/udp-edge-coverage-observations.json)。
- 単体ビルド、コンパイラのログ、測定スクリプトは `build/slang-udp-20261031/`（追跡外）。
  プロジェクトの依存の固定版は変えていません。
