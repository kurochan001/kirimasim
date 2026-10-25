# 共有カーネルの受動強度網と world

[English](passive-network-execution.md) が正。Status: カーネルの実装確認点、2026-10-26。
受動スイッチのソース変換、SIR 接続、P1 全体は残作業。

## 根拠と範囲

`a2aa7573` で、既存の Context 所有の origin 表と component 公開処理へ、
不変の受動グラフ、不確定な edge の正確な world 解決、スカラー導通タイマーを追加した。
根拠は承認済み [MOS/switch 契約 1/3/6/7](mos-switch-proposal.ja.md)、
[規範的な対応](p1-execution-contracts-2026-10-25.ja.md)、VALUE_SEMANTICS 5.3–5.6、
SCHEDULER_SPEC 9/12、IEEE 1800-2023 4.9.5/28.8/28.13/28.14。
実装とテストは独自に作り、他のシミュレータの実装・テストは読まず、規格の本文・例も
写していない。依存やツールは導入していない。外部 C ABI、formatVersion、GC の時機、
性能予算、slang の厳密な診断は変えていない。

独立したグラフ計算だけではなく、実際の scheduler・Store の実行経路へ接続した。
ただし SystemVerilog の front は **まだ受動 edge を生成しない**。SIR の edge・
制御・遅延の宣言、verifier/VM の受け渡し、ソースのプリミティブ正規化、部分・非結合
ポートの同一性、登録 net の force/release が必要である。完全区間の `%v` と、ほかの
P1 差分も残る。ソースでの Icarus 差分縮小や、スイッチへの全面対応とは扱わない。

## 静的所有と正確な解決

`configureStrengthDrivers(nodes, origins, edges)` は任意の edge 表もコピーする。
各 edge は正規化したスカラー node ID 二つ、恒等・非抵抗・抵抗の写像、初期の
off/on/uncertain を持つ。安定した ID は入力表の順番。両端は同じ公開 component に
属する必要があるが、packed vector の公開のように、一つの component に互いに
つながっていない node があってもよい。条件付きデバイスの初期 off、無条件の初期 on
は承認契約どおり、将来の front/SIR 生成側の責任となる。

登録は、実際の表を変える前に ID・写像・状態を検査する。`uwire` は初期 off の edge を
含む潜在的な接続先で、異なる物理 origin を一度ずつ数える。同じ origin の複数経路は
複数ドライバにしない。暗黙 root の規則は同じで、呼び出し側が pull/supply を一度だけ
実体化し、core のネット種別を渡す。表と、隣接・world 選択・経路・解決の作業領域を
Context の memory resource で所有する。失敗時は空の未登録状態で再試行でき、
観測可能な書き込みや部分的なグラフを残さない。

`Scheduler::solvePassiveStrengthComponent` は、導通を固定した world ごとに解決してから
最終的な hull を取る。

1. 不確定 edge ごとのオン・オフを独立に選ぶ。carry vector を使い、機械語のビット数に
   よるデバイス数制限や近似の打ち切りを入れない。
2. 物理 origin ごとに、8 要素の強度写像を固定グラフで伝播する。大きさを制限できる
   queue で、各入力強度の最強経路を保持する。単調な写像は増幅しないため、循環しても
   改善回数は有限。恒等 edge は supply 強度を落とさない。
3. 各到達先で、その origin の正確な写像後の点集合を、既存の点選択の解決状態へ畳み込む。
   同じ origin は複数経路でも一度だけ数える。先に区間・経路の hull を取って穴を埋めない。
4. 全物理 root に対して、到達先のネット種別の tie 規則を適用する。その後で、異なる
   不確定 edge の world の hull をまとめる。

途中の解決済み readout は root に戻さない。例えば、同じ強さの逆の root を持つ
wire/wand/wire は承認済みのプロジェクト解釈で X/0/X となり、真ん中を読んで追加の0
ドライバにはしない。逆の root が共通の不確定 edge を一緒に通ると、到達先は既知1の
区間 `[1,6]` を保つ場合がある。root ごとに先に hull を取ると、不必要な X が生じる。
最後の非ゼロ root を取り除けば、閉じた受動環でも Z になる。全 root がゼロの省略計算は
すべての world で正しく、古い公開値を電荷として残すものではない。

既存の component 公開処理だけが、完成した区間・値の全体を callback 前に commit する。
callback の更新は次の別の Active 解決を予約し、world・経路の計算中に利用者コードを
実行しない。受動 edge のない component は従来の独立 node 解決を使うので、通常の
ソース設計や従来の方向付き MOS 経路にグラフを強制しない。

## 導通の更新と resource の境界

`setStrengthEdge` と `setStrengthEdgeDelayed` は、数値 ID の所有する三状態を更新する。
root と edge が直接更新・慣性更新の共通 helper を使う。同じ pending 値は締め切りを保ち、
異なる値は64ビットの世代でキャンセルし、committed へ戻れば代わりを予約しない。
予約失敗・時刻あふれで以前の pending を置き換えない。新しい on/off はそれぞれの遅延、
uncertain は最小値を使う。明示的ゼロは Inactive に commit し、Reactive 発でも解決は
Active に予約する。

edge が遅らせるのは導通であり、運ばれるデータではない。off が pending の間も、
新しい root は現在オンの edge を直ちに通る。遅延付き on が commit したときには、
予約時の値でなく、その時点の root を運ぶ。タイマーは ID・世代を持ち、発行した
process/frame を所有せず、隠れた fork の子や RNG 状態も作らない。
各スカラー edge は pending と締め切りを独立に持つ。

準備する作業領域は node・edge・root に対して線形であり、world ごとに直積の結果を
動的確保しない。ただし正確な world の列挙時間は不確定デバイス数に対して指数的に
増え得るので、規模に対する高速性の主張ではない。処理内部で既存の割り込み・実時間
予算を確認し、完了前の停止では部分的な蓄積を公開しない。私有の poll wrapper は
時間制限の note 生成時の確保失敗を、例外を境界の外へ出さず既存の resource limit に
変換する。公開処理も資源失敗時の commit 有無を原子的に扱う。
追加予算・許容誤差・隠し CLI 切り替えは設けていない。

## 独立した証拠

`valsem::resolvePassive` は別の遅い参照実装で、edge world ごとに root の点の組と
単純経路を列挙し、参照のローカル resolver を使う。カーネルの隣接 queue、強度表の
伝播、圧縮した点状態の畳み込みは共有しない。`uwire` の潜在到達性も別に調べる。
カーネルにはリンクしない。

新規19テストは次を含む。

- 全120区間×15対向点×core 到達先6種の10,800状態。恒等・非抵抗・抵抗の経路と
  不確定な再合流を含む。さらに edge 3個の写像・制御・負荷の3,645状態。
  合計 **46,980個の到達先区間** が独立参照と一致する。
- 共通 world の相関、既知の正の区間と強さだけの通知、到達先ごとの混在種別、
  callback が次の snapshot を変える root なしの環、65ビット要素境界をまたぐ130 node の
  双方向の抵抗経路、`uwire` の origin 同一性と非接続部分の所有。
- 再試行できる不正グラフ6種、入力表の所有、失敗・成功を含む memory resource の
  48予算と公開失敗時の原子性、導通9遷移、pending 同値・キャンセル・戻し、
  導通待ちの間の現在データ、独立したスカラー締め切り、明示的ゼロ・Reactive の順序、
  以前の pending を保つ時刻あふれ。
- 根がない64個超の不確定 edge と、非ゼロ root がある実時間予算付きの例。
  正確に完了するか、古い snapshot 全体を保って停止することを検査し、遅い指数的
  アルゴリズムを必須にするテストではない。

[新規グラフ故障15件と既存 origin 故障13件](../measurements/2026-10-26/passive-network-controls.json)は、
全件コンパイル成功後に実際のテストが失敗した。既存故障で共通化した時間処理も検査する。
コンパイル失敗を検出とは数えていない。復元後は **300件** が通り、従来の方向付き MOS
154ソース、ソース境界6件、既存29ダンプ、PerfCount も含む。強さだけの変化では
`slot.changes` が増えないことと、component 全体の公開では対象ごとの `slot.writes` が
増えることを別に検査する。書き込み試行は値変化の通知ではない。
既存の期待値・アサーションは変更も緩和もしていない。

## 検証と残作業

[全 local CI 記録](../measurements/2026-10-26/passive-network-validation.json)の21項目は、
**17成功・4構成のテスト項目が `cli/options` だけで失敗**。dev/ASan/UBSanは
5,380/5,383、releaseは5,381/5,384が通る。各失敗1、既知スキップは
`cb/input_sample_1step` と `nets/net_port_drivers_part` の2件。新規グラフ、従来の
ソースMOS、既存dump・PerfCount、Werrorビルド、方針検査、clang-format、全110ソースの
clang-tidyは通る。公開ヘッダを例外・RTTIなしのC++20でコンパイルする独立検査も通った。
手動の `a2aa7573^..a2aa7573` 期待値検査は通る。自動検査は検証時点でupstreamがない。

別の [nmos期待値移行](mos-expectation-migration.ja.md)は未承認・未適用であり、
その古い拒否期待が各構成で唯一失敗するCTestである。このカーネル作業で、その移行や
別のイベント命令単位safepointを承認したとは扱わない。

[通常経路を交互5組測った記録](../measurements/2026-10-26/passive-network-speed.json)は、
CI終了後にreleaseの `e7e2ec59` と `a2aa7573` を20万回、小さい実行の起動時間を差し引いて
比較する。frame accessは0.260870→0.263807秒（+1.1%）、event bounceは
0.047777→0.048948秒（+2.4%）。全カウンタは一致し、RSS中央値はそれぞれ
9.265625→9.1875 MiB、9.1875→9.1875 MiB。負荷のある機械での少数回の観測なので、
安定した悪化・高速化の証明や7夜の基準値ではない。両設計は受動edgeを使わず、
グラフ・worldの負荷は未測定。閾値や予算は変えていない。

利用者の依頼により、コミット・pushして停止するための確認点とする。検証した作業ブランチは
`codex/p1-event-sir`、基点は `62148877`。slangのconfigure表示は
`12.0.561+3b351a52b`、固定commitは `3b351a52b15755b896d3b0144ae0432547d86618`。
fetch時点の `origin/main` は `6432683f` で、別のslang 12.1統合を含む。
これは今回検証したブランチには含まれず、統合と再検証は次の別作業として残す。

次は静的 edge・制御・遅延の所有を、検証した SIR と機械的な VM で受け渡す。
続いて全6種の双方向ソースと構造的ポートを、物理 origin の同一性を失わず正規化する。
登録 net の force と完全な強さの書式も、同じ承認契約の残作業である。
大域 allocator の枯渇と64ビット世代の枯渇を網羅的に実行したわけではない。
今回は局所 memory resource と実際の時刻あふれを検査する。将来、範囲を限定した確保の
差し替え口と世代の注入で、大域可変状態を増やさず確認を強められる。
外部 sv-tests・母集団、新しい Icarus プローブ、Linux・最小コンパイラ、5シミュレータの
完全な速度表の再測定も今回のカーネル確認点には含めない。
