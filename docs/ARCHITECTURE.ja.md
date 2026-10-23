# kirimasim の構成

[English](ARCHITECTURE.md) · 英語版が正本で、これはその日本語版です。食い違ったときは英語版に従います。

これは短い概要です。詳細な仕様は [docs/spec/](spec/) にあります。

## 処理の流れ

処理は一方向に流れます。

```mermaid
flowchart TD
    src["SystemVerilog sources"]
    slang["slang<br/>parsing, preprocessing,<br/>type checking, elaboration"]
    front["front<br/>lowers the elaborated design to SIR"]
    sir["SIR<br/>storage slots and statically typed instructions"]
    analysis["analysis<br/>X-reachability, static islands,<br/>levelization, observability"]
    subgraph tiers["Execution tiers"]
        t1["tier 1: VM<br/>interprets SIR (P1)"]
        t2["tier 2: generated C++<br/>built ahead of time (P2)"]
        t3["tier 3: JIT<br/>(later)"]
    end
    kernel["Execution kernel<br/>scheduler with every region, value operations,<br/>file I/O, formatting and scanning, VCD,<br/>typed container heap API<br/>(tier integration, classes, VPI, DPI-C: pending)"]
    src --> slang --> front --> sir
    sir --> t1
    sir --> analysis --> t2
    sir --> t2
    sir --> t3
    t1 -->|"resume function and frame"| kernel
    t2 -->|"resume function and frame"| kernel
    t3 -->|"resume function and frame"| kernel
```

slang は改変せず、1 つのコミットに固定したライブラリとして使います。修正は上流に送ります。

## 3 つの実行の層と、1 つのカーネル

2026-10-23時点では、第1層と共有カーネルが[P1の対応一覧](P1_SUPPORT.ja.md)の
範囲を実装しています。P1全体の機能範囲の目標は未達です。
第 2 層は P2 の予定で、第 3 層はその後です。以下の表と共通の実行の取り決めは、
将来の層を含めた設計全体を示します。

| 層 | 何か | 役割 |
| --- | --- | --- |
| 1：VM | SIR のインタプリタ。今は SIR を直接解釈し、SIR から作るレジスタ型のバイトコードは後で入れる | すべての機能を扱う、すぐに始まる、正しさの基準 |
| 2：C++ の生成 | SIR を前もって C++ にし、共有ライブラリとしてビルドする | RTL（静的な島と、合成できるプロセス）の速さ |
| 3：JIT | テストベンチの頻繁に動く関数を、実行時にコンパイルする | 後で。第 2 層の後に決める |

どの層も、プロセスを同じ形で**実行カーネル**に登録します。再開関数と、プロセスの状態（automatic な変数、再開の位置、待ちをまたいで生きる一時の値、保留している違反とアサーションの報告）を持つフレームです。待ちを含みうるタスクの呼び出しは、それにつながる呼び出しのフレームを足します。それ以外はすべてカーネルが持ちます。IEEE 1800-2023 のすべての領域を持つスケジューラ、値の演算、オブジェクトとガベージコレクション、VPI、DPI-C、波形、アサーション、カバレッジ、制約の解決です。カーネルからは層を見分けられないので、別々の層のプロセスを 1 つのシミュレーションの中で一緒に動かせます。層の間の違いは、同じテストの出力の違いとして現れます。

設計のもとにある考え：

- **名前ではなくスロット。** 信号と変数は平らな格納領域に置き、スロットの番号で指します。名前は、VPI、波形、診断のためだけに持ちます。
- **4 値の語。** 値は 64 ビットの `(a, b)` の語の組の配列で、ビットの意味は VPI の aval と bval と同じです（0 = (0,0)、1 = (1,0)、Z = (0,1)、X = (1,1)）。どの入力にも X と Z が無いときは、演算は 2 値の速い道を通ります。
- **静的な島ごとに 2 つの版。** X が無いことを静的に証明できないところでは、第 2 層は島の 4 値の版と 2 値の版を作り、実行時にどちらかを選びます。
- **意味の定義は 1 つ。** 各演算の意味は、`valsem`の遅い参照実装に定めます。整数演算は1ビットずつ計算し、コンテナの値は独立したセルのモデルを使います。速い版はそれと突き合わせてテストします。
- **大域の状態を持たない。** シミュレーションの状態はすべてコンテキストのオブジェクトに置くので、1 つのプロセスの中で複数のシミュレーションを動かせます。

## 型付きコンテナのヒープ：カーネル実装済み、実行層との接続は未実装

[承認済みの契約](notes/dynamic-containers-proposal.ja.md)は、SIR_SPEC 4.2.1と
RUNTIME_ABI 6に反映済みです。Contextはスロットと文字列に加えて、
[`Heap`](../src/kernel/include/kirimasim/kernel/heap.h)を持ちます。配置の記述子は語と
入れ子のコンテナへの辺を表し、slang・SIRに依存しません。空はトークン0、非空の
コンテナと安定した要素セルは世代付きトークンを使います。キューの移動は残るセルを
保ち、置換・削除で切り離したセルも参照から届く間は保ちます。値のコピーは独立した
要素を作り、入れ子のコンテナも深くコピーします。

確保はトランザクションを使い、失敗時は代入先を変える前に新しいオブジェクトを
巻き戻します。回収時は、先に確保した空き項目の格納域へ添字で戻します。コピーと
トレースは反復で行います。`Heap::collect`は明示したコンテナ・セルのルートをたどり、
到達不能な項目を世代を変えて再利用します。印付けに失敗すれば何も解放しません。
独立したモデル、確保失敗、深い入れ子、古いトークン、Context間の分離をテストします。

カーネルのAPIと型付きSIRの記述子・参照・命令、検証器・テキスト形式は実装済みです
（SIR_SPEC 5.8）。カーネルのコピーは入れ子の各段でコピー先の上限を適用し、
スナップショットは実際の元の型を保ちます。VMは型付きのオペランドをAPIへ写し、空でも
実際の対象型を保つ変数参照と、安定したトークンを持つセル参照を使います。
独自のSIRの14テストが実行・呼び出し・待機・エラーへの写像を検査します。
frontの変換、ヒープに依存する変更通知、セルのNBA、スロット・フレーム・一時値・保留イベントを含む全ルートの
自動公開は未接続です。回収には、DPIの最中でない許された境界で全ルートを渡す
必要があります。実行中のSystemVerilogの設計は、まだこのヒープと回収を使いません。
言語の対応には、これらの接続と実行テストが必要です。

## ライブラリ

依存は、ここに挙げた向きだけです。

| ライブラリ | 中身 | 依存してよいもの | 状態 |
| --- | --- | --- | --- |
| `front` | slang を呼び、その結果を SIR に変換する | slang、`sir` | P1 の範囲の構文を変換する。ほかは未対応として報告する |
| `sir` | SIR の定義、検証器、テキスト形式 | なし | 定義、型の表、検証器、テキスト形式 |
| `analysis` | X 到達解析、静的な島、レベル化、可観測性 | `sir` | 空 |
| `vm` | 第 1 層（バイトコードの生成と実行） | `sir`、`kernel` | SIR を直接解釈する。バイトコードは後で |
| `codegen` | 第 2 層（C++ の生成とビルド） | `sir`、`analysis` | 空 |
| `kernel` | 実行カーネル | libfstapi（後で）。任意の依存は `solver_adapter` と `fiber_backend` を通してだけ、C++ のインターフェースとして見る | すべての領域を持つスケジューラ、スロットの格納、4 値の語の演算、ネットの解決、UDP の表、ファイル入出力、書式と読み取り、文字列、VCDの出力、型付きコンテナのヒープAPI、事象の回数、コンテキスト |
| `solver_adapter` | 差し替えられる制約ソルバ | `sir`、選んだ外部のソルバ | P3 の予定 |
| `fiber_backend` | 時間を使う DPI-C のタスクのためのファイバー | 選んだファイバーのライブラリだけ | P3 の予定 |
| `valsem` | 値の意味の参照実装 | なし | 値の命令、辺、強さのあるネットの解決、UDPの表、整数コンテナのセルのモデル |
| `driver` | `kirimasim` のコマンドライン | すべて | コンパイルと実行、`parse`、`--version`、入力のオプション、`--dump-sir`、`--stats=json`、実行の上限、SIGINT での停止 |

同じ向きを図にしたもの（矢印は「依存する」。表と食い違ったときは表に従います）：

```mermaid
flowchart TD
    driver["driver<br/>the kirimasim command"]
    front["front"]
    vm["vm<br/>tier 1"]
    codegen["codegen<br/>tier 2"]
    analysis["analysis"]
    kernel["kernel"]
    valsem["valsem"]
    sir["sir"]
    slang["slang<br/>(external, pinned)"]
    solver["solver_adapter (P3)"]
    fiber["fiber_backend (P3)"]
    driver --> front
    driver --> vm
    driver --> codegen
    driver --> kernel
    driver --> valsem
    front --> slang
    front --> sir
    vm --> sir
    vm --> kernel
    codegen --> sir
    codegen --> analysis
    analysis --> sir
    solver --> sir
    kernel -.->|"C++ interface only"| solver
    kernel -.->|"C++ interface only"| fiber
```

この配置から従う決まり：

- `kernel` は利用者の設計と一緒にリンクされるので、slang にも LLVM にも依存しません。公開ヘッダは、生成コードが読むので C++20 です。
- `solver_adapter` と `fiber_backend` はまだビルドしません（P3 で作ります）。依存の決まりをそろえるために、ここに載せています。

## 正しさと性能の確かめ方

- 速い値の演算は、乱数の入力で `valsem` と比べます。
- 第 1 層と第 2 層は同じテストを走らせ、出力と、観測できる事象を比べる予定です（`--tier-check`）。
- **事象の回数**（`--stats=json`、`PerfCount` のテスト）で、機械の負荷によらず処理量の変化を検出します。このテストの通過だけでは実行速度は分かりません。実時間と最大 RSS は `tools/bench.py` で別に測ります。[性能の仕様書](spec/PERFORMANCE.ja.md)を参照してください。
