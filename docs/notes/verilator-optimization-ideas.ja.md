# Verilator 公開資料に基づく kirimasim 高速化アイデアと実験ガイド

[English](verilator-optimization-ideas.md) · 英語版が正本で、これはその日本語版です。食い違ったときは英語版に従います。

**クリーンルーム方針の明記（`docs/spec/AI_POLICY.md` 準拠）:**
本文書は Verilator のソースコードを一切閲覧・引用せず、公式マニュアル・変更履歴・公開技術ブログ・学術論文に記載された**設計上の考え方のみ**を整理したメモです。実装時はすべて `kirimasim` 自身の SIR・解析テーブル・`CODEGEN_SPEC`・`RUNTIME_ABI`・`--tier-check` に基づいて独立に設計・検証します。

---

以下の優先順位は 11 月 23 日の開発時点の実測に基づきます。0.3.0 は Ibex の両方の目標を達成しました。[P3 完了時の実測](../BENCHMARKS.ja.md#p3-performance-targets)

## 1. 現状のボトルネックと優先順位の考え方

[段階 A の測定](p4-performance-profile.ja.md)では、Ibex は島の評価・NBA 反映、
CVA6 はネイティブに入れない本体の VM 実行の比重が高かった。まず狭い非巡回領域の
評価と観測の境界、次に NBA の連続発行区間を扱う。CVA6 の生成適格性と、
日次 28 件中 14 件で実際の生成本体の呼び出しがない問題は別に測る。
待機付き本体の生成再開と VM バイトコード試作を並行の計測項目に置く。

下表は過去の実験候補を含む。A3 は融合と観測契約の確立後の別提案とし、
A1 のコンパイラ設定、Hot/Cold 分離、A2 の PGO は構造上の処理を減らした後、
生成側・本体側のフラグを分けて測る。速度見積もり欄はこの環境の測定値ではない。
[設計と順序](p4-performance-proposal.ja.md)

## 2. 優先度別アイデア一覧（実験バックログ）

| ID | アイデア | 対象フェーズ | 主な狙い | 期待効果の領域 | 難易度 |
| :--- | :--- | :--- | :--- | :--- | :--- |
| **A1** | 生成 C++ の Hot/Cold 分離と `-Os` 最適化（I-cache 対策） | P3 | 大規模 RTL の命令キャッシュミス削減とビルド短縮 | 実行時間＋ビルド時間 | 低 |
| **A2** | コンパイラ PGO（`-fprofile-generate` / `-fprofile-use`）の実験導入 | P3 / P7 | 分岐予測・コード配置の自動最適化 | 実行時間（5〜15% 目安） | 低 |
| **A3** | 非観測中間信号のローカル変数化（Localization） | P3〜P4 | モデル状態メモリへの読み書き削減・レジスタ昇格 | 2値/4値 RTL 実行時間 | 中 |
| **A4** | 組み合わせ論理の DFG 最適化・重複排除・ピープホール最適化 | P3 | 冗長演算・複製連結・ワード抽出の削減 | 2値/4値 RTL 実行時間 | 中〜高 |
| **A5** | Packed/Unpacked 集約型の分解（Scalarization / Decomposition） | P3〜P4 | 部分ビット操作や構造体フィールド単位の独立化 | static island 適用率＋速度 | 中 |
| **A6** | 定数プール化・ワイド変数のゼロ初期化／一括埋めの高速化 | P3 | 大きな配列・ワイド定数・リセット処理の軽量化 | 実行時間＋コードサイズ | 低〜中 |
| **B1** | 動的スケジューリングの局所適用（静的 RTL と待機経路の厳密分離） | P4 | テストベンチ待機機能を導入しても RTL 速度を落とさない | P4 以降の退行防止 | 中 |
| **B2** | 待機フレーム専用アロケータ（Custom Frame Allocator） | P4〜P5 | `automatic` タスク中断時のヒープ確保コスト削減 | テストベンチ・UVM 速度 | 中 |
| **B3** | 生成 C++ 出力の整形オプション化と 128 KB 出力バッファリング | P3〜P4 | コード生成フェーズ（Time to start running）の短縮 | 起動・ビルド時間 | 低 |
| **B4** | 値オブジェクト・定数ノードのコンパクト化とステージ統合 | P4〜P7 | 大規模設計時のピーク RSS 削減・キャッシュ効率改善 | メモリ使用量＋生成時間 | 中 |
| **C1** | 未使用クラスメソッド削除と非オーバーライド仮想関数の脱仮想化 | P5 / P7 | UVM クラス階層の呼び出しオーバーヘッド削減 | UVM 実行時間 | 中 |
| **C2** | 階層別コンパイル（Hierarchical Compilation）と増分キャッシュ | P6〜P7 | 大規模 SoC（CVA6 / OpenTitan）のビルド・メモリ削減 | ビルド時間・総所要時間 | 高 |
| **C3** | 実測コストに基づく静的タスク分割（粗粒度化）と Thread PGO | P6〜P7 | 同期オーバーヘッドを抑えたマルチスレッド化 | 並列実行スケーリング | 高 |

---

## 3. 各アイデアの詳細と kirimasim での実験手順

### Tier A：P3 ですぐ試せる施策（Ibex 1%・CVA6 0.2% に直結）

#### A1. Hot/Cold コード分離と `-Os` 最適化（I-cache 対策）
- **公開資料の考え方:** Verilator 公式ガイド（[Simulating — Benchmarking & Optimization](https://verilator.org/guide/latest/simulating.html)、[FAQ](https://veripool.org/guide/latest/faq.html)）では、大規模 RTL モデルは CPU の命令キャッシュ（I-cache）容量がボトルネックになりやすく、毎サイクル実行される Hot パスを **`-Os`**（コードサイズ優先最適化）でコンパイルし、初期化・終了など稀にしか呼ばれない Cold パスを別翻訳単位（`__Slow.cpp` 相当）に分けて **`-O0`** にすることで、実行速度とビルド時間を両立しています。MIT CSAIL の論文（[Accelerating RTL Simulation with Hardware-Software Co-Design, MICRO 2023](https://people.csail.mit.edu/sanchez/papers/2023.ash.micro.pdf)）でも、RTL シミュレーションでは命令フットプリントの大きさが主要制約であることが指摘されています。
- **kirimasim での適用ポイント:** `src/codegen/` で生成する C++ を「サイクルごとに走る island / 評価関数（Hot）」と「初期化・メタデータ・VPI/シンボル登録（Cold）」に分割し、生成コードのコンパイルフラグで `-O2` と `-Os`（＋`-march=native` の比較）を検証します。なお `CODEGEN_SPEC.md` の浮動小数点規約に従い、`-ffp-contract=off` は維持します。
- **試す手順:**
  1. 既存の生成 C++ ビルドフラグで `-O2` vs `-Os` vs `-O1 -fstrict-aliasing` を Ibex（2値/4値）と CVA6 で 5 回中央値比較する。
  2. Cold 関数（初期化・テーブル登録）を別 `.cpp` に分離して `-O0` ビルドし、ビルド時間（Time to start running）と実行時間の変化を記録する。

#### A2. コンパイラ PGO（Profile-Guided Optimization）の実験
- **公開資料の考え方:** Verilator 公式ガイド（[Compiler Profile-Guided Optimization](https://verilator.org/guide/latest/simulating.html)）では、GCC / Clang の 2 パス PGO（`-fprofile-generate` で代表実行してプロファイルを採取し、`-fprofile-use` で再コンパイル）により、シングルスレッド・マルチスレッド双方で **5〜15%** の改善が得られると報告されています。
- **kirimasim での適用ポイント:** 生成されたモデル共有ライブラリ／実行バイナリに対して、キャッシュ済みビルドのオプションとして PGO パイプラインを試します。
- **試す手順:**
  1. Ibex CoreMark の短い反復（例：1 イテレーション）で `-fprofile-generate` ビルドを実行し、`llvm-profdata merge` でプロファイルを生成する。
  2. `-fprofile-use` で再コンパイルし、10 イテレーションの本計測（5 回中央値）で通常ビルドと比較する。
  3. 結果の CRC・最終サイクル数および `--tier-check` の一致を確認する。

#### A3. 非観測中間信号のローカル変数化（Localization）
- **公開資料の考え方:** Verilator の引数リファレンス（[verilator Arguments](https://veripool.org/guide/latest/exe_verilator.html) の `--localize-max-size` 等）にあるように、モデル全体の状態構造体（メモリ上のスロット）に保持しなくてもよい一時信号・中間信号を、評価関数内の C++ ローカル変数へ降格させることで、メモリ読み書きを減らしコンパイラのレジスタ割り当てに乗せます。
- **kirimasim での適用ポイント:** `src/analysis/islands.cpp` の static island 内、または P4 の組み合わせ回路まとめ評価区間において、「island 外から参照されない」「VPI / 波形ダンプ / イベント観測者 / `force` 対象になっていない」スロットを抽出し、生成 C++ では `Context` スロットではなくスタック上のローカル変数として計算します。
- **4値・観測性の注意点:** P4 で定義する観測対象テーブル（静的観測者・VPI・将来の SVA・`force`/`release`）に該当する信号、および同値性を静的に証明できない信号は必ず従来どおり `Context` スロットに書き戻すルールを厳守します。
- **試す手順:**
  1. `bench/comb_chain` と Ibex で、island 内部専用スロットの個数と書き戻し回数を集計する。
  2. まず一定幅以下（例：64 bit 以下）の非観測スロットをローカル変数化し、`--tier-check` と全スイート（island 有無一致）を通す。
  3. Ibex 2値/4値（5 回中央値）でメモリトラフィック削減効果を測定する。

#### A4. 組み合わせ論理の DFG 最適化・重複排除・ピープホール最適化
- **公開資料の考え方:** Verilator の公式ドキュメント（[verilator Arguments](https://veripool.org/guide/latest/exe_verilator.html) の DFG 最適化・重複組み合わせ論理削除・ピープホール最適化、および [Revision History](https://verilator.org/guide/latest/changes.html) の「aligned word extract」「concatenation of replications in DFG」「identical compound operands」）では、組み合わせ回路をデータフローグラフ（DFG）として表現し、以下を一括適用しています：
  - 同一値を計算する重複組み合わせ論理の統合（CSE / Deduplication）
  - 連結と複製（`{N{x}}`）の簡約、ワード境界に揃ったビット抽出の直接ワード参照化
  - 定数畳み込み・同一オペランド簡約などのピープホール最適化
- **kirimasim での適用ポイント:** `src/analysis/` と `src/codegen/` の間に、static island（および適格な組み合わせ区間）専用の軽量 DFG 簡約パスを追加します。特に 2値モード（`--state=2`）では X/Z 伝播のマスク演算が不要になるため、定数伝播・ビット抽出・重複式の統合が大きく効きます。
- **4値・観測性の注意点:** 4値モード（`--state=4`）では `0 & X == 0`、`1 | X == 1`、一方で `X ^ X == X`（`0` に簡約してはいけない）など IEEE 1800-2023 の 4値真理値表を厳密に守る必要があります。すべてのピープホール規則は `valsem` とのクロスチェックを必須ゲートにします。
- **試す手順:**
  1. 2値モード限定で「重複組み合わせ式の統合」と「ワード境界に揃ったビットスライス抽出」を実装し、`valsem` および `--tier-check` で検証する。
  2. 4値モード向けに `valsem` で真理値検証済みの安全規則のみを有効化する。
  3. 日次 25 本（特に `comb_chain`, `wide_arith`, `casez_struct`）と Ibex で効果を測定する。

#### A5. 集約型（Packed/Unpacked 配列・構造体）の分解
- **公開資料の考え方:** Verilator 公式マニュアル（[verilator Arguments](https://veripool.org/guide/latest/exe_verilator.html) の packed / unpacked 配列・構造体分解オプション）では、大きな packed 構造体や小規模な unpacked 配列をフィールド・要素単位の個別変数へ分解することで、部分ビット読み書きによる偽の依存関係（False Dependency）を解消し、不要な再評価やビットシフトを削減しています。
- **kirimasim での適用ポイント:** Ibex や CVA6 では制御バスやパイプラインレジスタが packed 構造体で束ねられているため、構造体全体が 1 スロット扱いだと「1 フィールド更新で無関係な下流ロジックまで再評価」「island 候補から脱落」が起きやすくなります。`src/analysis/` の依存解析または codegen 前段階で、動的添字や外部観測のない小規模集約型を要素単位に分解します。
- **試す手順:**
  1. Ibex / CVA6 の SIR で、集約型全体への依存により island 非適格または過剰再評価になっている箇所を `--report-tier` 等で洗い出す。
  2. 静的添字のみの packed 構造体・小規模配列を対象に分解パスを試し、`--tier-check` と `PerfCount`（評価回数減少）を確認する。

#### A6. 定数集約値の定数プール化・ワイド変数のゼロリセット／一括初期化
- **公開資料の考え方:** Verilator 5.054 変更履歴（[Revision History](https://verilator.org/guide/latest/changes.html)）には、定数集約値の定数プール配置、整数 unpacked 配列の一括埋め（bulk fills）、ワイド変数のゼロリセット最適化、VPI シンボル登録のテーブル化が挙げられています。
- **kirimasim での適用ポイント:** ワイドな 4値/2値ゼロクリアやリセット時のループ展開を `memset` / ワード単位ゼロ埋めへ置換し、定数テーブルを `static constexpr` 定数プールへ逃がすことで、生成コードサイズ（I-cache）と初期化コストの両方を削減します。
- **試す手順:** `wide_arith`・`mem_rw`・Ibex で生成 C++ の行数・バイナリサイズ・実行時間を比較する。

---

### Tier B：P3 後半〜P4 で試せる施策（混在実行基盤・起動時間・メモリ）

#### B1. 動的スケジューリングの局所適用（静的 RTL と待機経路の分離）
- **公開資料の考え方:** Antmicro の技術ブログ（[Using coroutines for low-overhead dynamic scheduling in Verilator](https://antmicro.com/blog/2021/12/coroutines-for-dynamic-scheduling-in-verilator)）では、遅延・イベント待ちを伴うプロセスをコルーチン化しつつ、**「設計のどの部分が本当に動的スケジューリングを必要とするか」を静的に判別し、合成可能 RTL 部分は静的スケジュール（高速経路）のまま残す**ことで、SweRV EH1 コア上で動的スケジューラ未使用時とほぼ同等の性能を維持したことが報告されています。
- **kirimasim での適用ポイント:** 新 P4 の「待機をまたぐ VM・native 間呼び出し」および「日次 25 本・Ibex の退行なし」と完全に合致する方針です。待機（`#delay` / `@event`）を含まない RTL プロセスにはフレーム確保や中断チェックのオーバーヘッドを一切持ち込まず、待機を含むタスク境界でのみ 2 階層フレームチェーンへ切り替える設計を徹底します。
- **試す手順:** P4 の受入テスト 1a〜1c を実装したうえで、待機を使わない Ibex / 日次 25 本の `PerfCount` と 5 回中央値に退行がないことを確認する。

#### B2. 待機フレーム専用アロケータ
- **公開資料の考え方:** 同 Antmicro ブログ（[Using coroutines for low-overhead dynamic scheduling in Verilator](https://antmicro.com/blog/2021/12/coroutines-for-dynamic-scheduling-in-verilator)）では、待機状態（コルーチンフレーム）のヒープ確保コストがボトルネックになり得るため、専用アロケータで軽減する手法が述べられています。
- **kirimasim での適用ポイント:** `RUNTIME_ABI.md` の 2 階層フレームのうち、待機付き `automatic` タスクのコールフレームを固定サイズプール／フリーリストから割り当てることで、P4 受入テスト 1b（`fork` 多重待機）や P5 の UVM タスク大量生成時の `malloc`/`free` コストを抑えます。
- **試す手順:** `bench/` の `fork`/`join` ベンチマークおよび P4 受入テスト 1b/1c で、標準ヒープ確保とプールアロケータの実行時間・ASan リークなしを比較する。

#### B3. コード生成の整形スキップと 128 KB 出力バッファリング
- **公開資料の考え方:** Antmicro の大規模設計向け最適化記事（[Scaling Verilator for very large designs](https://antmicro.com/blog/2022/11/scaling-verilator-for-very-large-designs)）では、生成 C++ の人向け整形（フォーマット）が生成時間の最大 **6%** を占めていたため省略可能にし、さらに出力バッファを **128 KB** に最適化することで `write()` システムコール回数を減らし約 **1.5%** 短縮した事例が紹介されています。
- **kirimasim での適用ポイント:** `PERFORMANCE.md` の「Time to start running（実行開始までの時間）」を縮める施策として、通常実行時の `src/codegen/` 出力では最小インデント・128 KB バッファ書き込みを既定とし、デバッグ時のみ整形出力するオプションを設けます。
- **試す手順:** Ibex および CVA6 で「ファイル読み込み開始〜時刻 0 初回評価まで」の時間を比較する。

#### B4. 値オブジェクト・定数ノードのコンパクト化とパス統合
- **公開資料の考え方:** 同 Antmicro 記事（[Scaling Verilator for very large designs](https://antmicro.com/blog/2022/11/scaling-verilator-for-very-large-designs)）では、排他的なメンバを `union` 化し推論可能なメタデータを削ることで値ノードを 96B→56B に縮小して大規模設計で 3 GB のメモリ削減を達成し、さらに「ノードを増やすパスと直後に減らすパス」の部分統合でピーク RSS を下げています。
- **kirimasim での適用ポイント:** `src/sir/`・`src/analysis/` の定数／値ノードのサイズを `sizeof` とピーク RSS（`/proc/<pid>/status` の `VmHWM` または macOS `ru_maxrss`）で計測し、CVA6 規模でのメモリ圧迫を先回りで抑えます。

---

### Tier C：P5〜P7 で試せる施策（UVM・大規模 SoC ビルド・並列実行）

#### C1. 未使用クラスメソッド削除と仮想メソッドの脱仮想化（Devirtualization）
- **公開資料の考え方:** Verilator 5.054（[verilator Arguments](https://veripool.org/guide/latest/exe_verilator.html) の `-fno-dead-members` および [Revision History](https://verilator.org/guide/latest/changes.html)）では、呼ばれないクラスメソッドを削除し、オーバーライドされていない仮想メソッドを非仮想（直接呼び出し）へ変換しています。
- **kirimasim での適用ポイント:** P5 で UVM が動き始めた後、P7 の最適化でクラス継承階層全体を解析し、単一実装の仮想メソッドを直接呼び出し化・インライン化候補にすることで、UVM トランザクション当たりの時間を削減します。

#### C2. 階層別コンパイル（Hierarchical Compilation）
- **公開資料の考え方:** Antmicro の記事（[Improving Verilator's hierarchical mode for better performance and scalability](https://antmicro.com/blog/2025/05/improving-verilator-hierarchical-mode)）では、大規模設計を階層ブロック単位で独立に C++ 生成・コンパイルして結合することで、変更時の再ビルド時間とピークメモリを大幅に削減しています。
- **kirimasim での適用ポイント:** P6〜P7 で OpenTitan や CVA6 のような大規模設計を扱う際、モジュール／サブシステム単位で SIR→C++ 生成結果をキャッシュ・再利用する仕組みに応用できます。

#### C3. 粗粒度タスク分割と Thread PGO（実測コストに基づく並列スケジューリング）
- **公開資料の考え方:** Verilator 4.0 発表資料（[ORConf 2018](https://veripool.org/papers/Verilator_v4_Multithreaded_OrConf2018.pdf)）、公式ガイド（[Thread Profile-Guided Optimization](https://verilator.org/guide/latest/simulating.html)）、および MIT CSAIL 論文（[MICRO 2023](https://people.csail.mit.edu/sanchez/papers/2023.ash.micro.pdf)）の知見は一致しています：
  - RTL の並列性は細かいタスクに分散しているため、細かすぎるとスレッド間同期コストが支配的になる（同論文の計測では 32 コア環境でも約 203 タスクへ粗粒度化した時点で約 3.5x が頭打ち）。
  - 静的コスト見積もりだけでは負荷が偏るため、実測実行時間をフィードバックする **Thread PGO** と、スレッドごとの進行ワードによる軽量同期（Verilator 5.054 変更履歴の「per-thread progress words」「AArch64 spin waits」「little work 時のワーカー起床回避」）が有効。
- **kirimasim での適用ポイント:** P6（並列実行）の設計初期から「粗粒度マージ（同期コストとのトレードオフ）」「軽量同期」「P7 での Thread PGO 導入」を前提にタスクグラフを設計します。

---

## 4. kirimasim での共通実験プロトコル

各アイデアを実装・評価する際は、P3〜P7 ロードマップの計測規約に合わせて以下の手順で 1 施策ずつ独立に検証します。

1. **正当性ゲート（最優先）**
   - `python3 tools/local_ci.py`（全ユニット・スイート・ASan/UBSan・静的検査）が通過すること。
   - Tier 1 / Tier 2 一致、static islands 有無の一致、および `--tier-check` で不一致が 0 件であること。
   - 4値最適化を追加した場合は `valsem` とのクロスチェックを通過すること。
2. **カウント回帰確認（`PerfCount`）**
   - `--stats=json` で評価回数・NBA 回数・island 呼び出し回数の変化を確認する（意図した削減が起きているか）。
3. **実時間ベンチマーク（5 回以上の中央値）**
   - **日次 25 本:** 既存 RTL への退行がないか確認。
   - **Ibex / CoreMark（4値・2値）および CVA6（2値）:** 同一ホスト・同一条件で 5 つの CRC と最終サイクル数の一致を先に確認し、コンパイル・キャッシュ準備後に各 5 回以上の中央値で比較。ツール版・コミット・コンパイラ・実効オプションを記録する。
4. **設計メモへの出典記録（`AI_POLICY.md` 準拠）**
   - 採用した施策は、該当する `docs/spec/` 節または `docs/notes/` 設計メモに本資料の出典 URL を明記し、コードの引用を行っていないことを保つ。

---

## 5. 参照した公開資料一覧（すべてソースコード非含有の公開文書）

- Verilator 5.054 Official Guide — Simulating (Benchmarking & Optimization, PGO, Profiling): https://verilator.org/guide/latest/simulating.html
- Verilator 5.054 Official Guide — Arguments Reference: https://veripool.org/guide/latest/exe_verilator.html
- Verilator 5.054 Official Guide — FAQ: https://veripool.org/guide/latest/faq.html
- Verilator 5.054 Official Guide — Revision History and Change Log: https://verilator.org/guide/latest/changes.html
- Antmicro Blog — Using coroutines for low-overhead dynamic scheduling in Verilator (2021-12-30): https://antmicro.com/blog/2021/12/coroutines-for-dynamic-scheduling-in-verilator
- Antmicro Blog — Scaling Verilator for very large designs (2022-11-22): https://antmicro.com/blog/2022/11/scaling-verilator-for-very-large-designs
- Antmicro Blog — Improving Verilator's hierarchical mode for better performance and scalability (2025-05-26): https://antmicro.com/blog/2025/05/improving-verilator-hierarchical-mode
- Wilson Snyder — Verilator 4.0: Open Simulation Goes Multithreaded (ORConf 2018): https://veripool.org/papers/Verilator_v4_Multithreaded_OrConf2018.pdf
- MIT CSAIL — Accelerating RTL Simulation with Hardware-Software Co-Design (MICRO 2023): https://people.csail.mit.edu/sanchez/papers/2023.ash.micro.pdf
