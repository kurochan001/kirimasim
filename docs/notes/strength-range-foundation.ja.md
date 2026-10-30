# 共通の駆動強度区間の解決

[English](strength-range-foundation.md)。実装`30a6ebc`、2026-10-24。
既存の承認済み[VALUE_SEMANTICS 5.2–5.4](../spec/VALUE_SEMANTICS.ja.md)の
区間の結果を共有カーネルで扱う変更です。MOS・スイッチのソース実行と`%v`書式は未対応です。

本記録のソース対応の制限は、上記の基盤実装時点のものです。その後の
[有向MOS](source-mos-execution.ja.md)と[受動スイッチの実行](source-passive-execution.ja.md)は
この基盤を使います。現在の制限は[P1の対応一覧](../P1_SUPPORT.ja.md)に記載しています。

## 表現とAPI

従来のカーネルは強度の区間を内部で計算していましたが、4値の結果だけを返していました。
`[3,6]`と`[6,6]`はどちらも1と読めても、端点を失うと強度を正確に伝播できません。
新しいAPIは情報を捨てる前の承認済みの結果を公開します。

[`kernel/strength.h`](../../src/kernel/include/kirimasim/kernel/strength.h)に以下を追加しました。

- `StrengthRange`：符号付きの2端点で、入力の条件は`-7 <= lo <= hi <= 7`です。
  負の点は0、正の点は1、0は高インピーダンスです。
- `resolveStrengths(kind, signals, count)`：片側だけの曖昧な強度も含む任意の有効な区間を
  解決します。空の入力でも、そのネットの暗黙のpull・supplyは適用します。
- `resolveStrengthBit(kind, drivers, count, bit)`：マスク付き4値ドライバの完全な結果を
  保ちます。強度は0–7、bitは0–63で、片側マスクのL/Hも区別します。
- `strengthValue(signal)`：必要な時に4値へ射影し、L/Hは従来どおりXと読みます。
  既存の`resolveBit`・`resolve4`も共通の区間計算を使い、値のインターフェースを保ちます。

高速版は既存の有限な点状態の集合と、共通の区間・wiredの同強度判定を使います。
メモリ確保・大域状態・依存の追加はありません。公開ヘッダはC++20を保ち、
`-std=c++20 -fno-exceptions -fno-rtti`の独立した呼び出しも、以下の入力に対して
`-7 -6 0 0`（区間の端点、W4のa/b）を返しました。

```cpp
#include "kirimasim/kernel/strength.h"

namespace k = kirimasim::kernel;
const k::StrengthRange inputs[] = {{-7, -6}, {3, 5}};
const auto signal = k::resolveStrengths(k::NetKind::Wire, inputs, 2);
const auto bit = k::strengthValue(signal);
```

独立した[`valsem::resolveSignals`](../../src/valsem/strength.cpp)は点の選択を直接列挙します。
既存のドライバ版の参照実装も、値と有効な各側の強度を区間へ写してから同じ列挙処理を使います。
カーネルはこの参照実装に依存しません。

## 検証範囲

追加した[ユニットテスト](../../tests/unit/strength_test.cpp)5本で、以下を検査します。

- 有効な120区間と、その4値への射影。
- 区間の全14,400組×11種類のネット、計158,400比較と、空・単独の入力。
- 入力4値、各側の強度8種類、片側マスク4組、ネット11種類、計11,264ケース。
- 固定シードの3区間512組を6種類の全順列で検査（既定のfuzz倍率）。
- 固定シードのマスク付き64ビットワード256組、計16,384ビット（既定倍率）。全ビット位置と
  0–3個のドライバを含みます。

復元後の強度の14テストはすべて通過しました。意図的な4種類の誤りもビルドに成功し、
実際の区間のアサーションで検出しました。同じ値の区間の左右の端点をそれぞれ潰す誤り、
入力を上端の点だけにする誤り、後続の入力を無視する誤りです。従来の値だけを見る9テストは
4種類とも通過しました。特に同じ値の端点を潰す2種類は4値の結果を変えないため、
新しい検査で失われた情報を検出できました。アンカーや判定ツール自身の失敗は根拠にしません。

`30a6ebc`のローカルCIはdev・release・ASan・UBSanと全静的検査を通過しました。
clang-format・clang-tidy・期待値保護も含みます。dev・ASan・UBSanは各2,831件中2,829件成功、
既知の2件がスキップです。releaseは2,832件中2,830件成功、同じ2件がスキップです。
言語の差分と測定したコミットは[比較・検証記録](p1-icarus-progress.ja.md)を参照してください。

```sh
ctest --preset dev -R '^Strength\.' --output-on-failure
python3 tools/mutants.py --only 'strength ranges:'
```

## 残る接続作業

現在のVMのresolve経路は引き続き4値を消費します。解決したネット区間の格納・転送、強度だけの
変更の伝播、MOSの減衰・制御、双方向ネットワークの解決、強度を使う書式は残作業です。
SIR・実行時の契約に未確定な細部があればレビューが必要です（2026-10-24の時点。MOS・スイッチと
遅延sideの契約は2026-10-25に承認されました）。このAPIでソースの各形式を
有効にしたり、Icarusとの実行差分を解消したりしたとは扱いません。P1全体の到達点と
既存の承認待ちの意味の修正案は維持します。
