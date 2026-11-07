# P1 の参照との差の根拠

[English](p1-reference-mismatch-grounds.md)。状態：**2026-11-07 に承認**。英語版が正本です。

[閉じた完了一覧](../spec/P1_SCOPE.ja.md#閉じた完了一覧2026-11-07-に承認)の条件 4 は、Icarus の出力が期待値と違うプローブごとに根拠を明記するよう求めています。2026-11-07 にコミット `60c26d7c` で測ると、Icarus Verilog 13.0 との差は 25 件で、kirimasim はすべて期待値どおりです。表の ` / ` は出力の改行です。

根拠は自分の言葉でまとめたものです。4 件は標準の規則に基づきます。残りの 21 件は、標準が選択を許しているか、プロジェクトの取り決めが形を定めているものなので、**差があっても Icarus 13.0 が誤りだとは言えません**。

## 標準の規則（4 件）

| プローブ | 期待値 | Icarus 13.0 | 根拠 |
| --- | --- | --- | --- |
| `nested_packed_select_bounds` | `5580` | `5780` | 7.4.5、11.5.1、[VALUE_SEMANTICS 3.11](../spec/VALUE_SEMANTICS.ja.md#311-ビットと部分の選択)：要素 `a[0]` から一部はみ出す範囲は、要素の内側の bit だけを書く。`a[1]` は `8'h55` のまま |
| `class_virtual_dispatch` | `9` | `3` | 8.20：一度 virtual になったメソッドは派生クラスでも virtual のまま。基底のハンドル経由でも、派生オブジェクトのメソッドが動く |
| `dynamic_array_queries` | `3 0 2 -1` | `3 0 2 2` | 20.7：動的配列の次元では `$increment` は -1 を返す |
| `blocking_repeated_event_one_bit_unknown` | `7 9` | `0 0` | 9.4.5：繰り返し回数が不定か高インピーダンスなら repeat がないものとして扱う。代入は待たない |

## 統合したネットのポート（11 件）

SIR_SPEC 3.5 は、input と output の接続の統合は任意であり、統合しない実装では値が変わりうると記録しています。

| プローブ | 期待値 | Icarus 13.0 | 根拠 |
| --- | --- | --- | --- |
| `partial_port_wand` | `0 0 / 1 1 / z z / 0 0` | `0 0 / 1 z / z z / 0 1` | 23.3.3.7、28.12.4、[SIR_SPEC 3.5](../spec/SIR_SPEC.ja.md#35-ポートの畳み込み)、[MOS/ポートの取り決め 4](mos-switch-proposal.ja.md#4-部分ネットポートと観測)：標準はネットの統合を許し、プロジェクトは統合する |
| `partial_port_triand` | `0 0 / 1 1 / z z / 0 0` | `0 0 / 1 z / z z / 0 1` | 23.3.3.7、28.12.4、[SIR_SPEC 3.5](../spec/SIR_SPEC.ja.md#35-ポートの畳み込み)、[MOS/ポートの取り決め 4](mos-switch-proposal.ja.md#4-部分ネットポートと観測)：標準はネットの統合を許し、プロジェクトは統合する |
| `partial_port_wor` | `1 1 / 1 1 / z z / 1 1` | `1 0 / 1 z / z z / 1 1` | 23.3.3.7、28.12.4、[SIR_SPEC 3.5](../spec/SIR_SPEC.ja.md#35-ポートの畳み込み)、[MOS/ポートの取り決め 4](mos-switch-proposal.ja.md#4-部分ネットポートと観測)：標準はネットの統合を許し、プロジェクトは統合する |
| `partial_port_trior` | `1 1 / 1 1 / z z / 1 1` | `1 0 / 1 z / z z / 1 1` | 23.3.3.7、28.12.4、[SIR_SPEC 3.5](../spec/SIR_SPEC.ja.md#35-ポートの畳み込み)、[MOS/ポートの取り決め 4](mos-switch-proposal.ja.md#4-部分ネットポートと観測)：標準はネットの統合を許し、プロジェクトは統合する |
| `partial_port_tri0_inner` | `0 0 / 1 1 / 0 0` | `0 0 / 1 0 / 0 0` | 23.3.3.7、28.15、[SIR_SPEC 3.5](../spec/SIR_SPEC.ja.md#35-ポートの畳み込み)、[MOS/ポートの取り決め 4](mos-switch-proposal.ja.md#4-部分ネットポートと観測)：標準はネットの統合を許し、プロジェクトは統合する |
| `partial_port_tri0_outer` | `0 0 / 1 1 / 0 0` | `0 z / 1 z / 0 z` | 23.3.3.7、28.15、[SIR_SPEC 3.5](../spec/SIR_SPEC.ja.md#35-ポートの畳み込み)、[MOS/ポートの取り決め 4](mos-switch-proposal.ja.md#4-部分ネットポートと観測)：標準はネットの統合を許し、プロジェクトは統合する |
| `partial_port_tri1_inner` | `1 1 / 0 0 / 1 1` | `1 1 / 0 1 / 1 1` | 23.3.3.7、28.15、[SIR_SPEC 3.5](../spec/SIR_SPEC.ja.md#35-ポートの畳み込み)、[MOS/ポートの取り決め 4](mos-switch-proposal.ja.md#4-部分ネットポートと観測)：標準はネットの統合を許し、プロジェクトは統合する |
| `partial_port_tri1_outer` | `1 1 / 0 0 / 1 1` | `1 z / 0 z / 1 z` | 23.3.3.7、28.15、[SIR_SPEC 3.5](../spec/SIR_SPEC.ja.md#35-ポートの畳み込み)、[MOS/ポートの取り決め 4](mos-switch-proposal.ja.md#4-部分ネットポートと観測)：標準はネットの統合を許し、プロジェクトは統合する |
| `partial_port_supply0_outer` | `0 0 / 0 0 / 0 0` | `0 z / 0 z / 0 z` | 23.3.3.7、28.15、[SIR_SPEC 3.5](../spec/SIR_SPEC.ja.md#35-ポートの畳み込み)、[MOS/ポートの取り決め 4](mos-switch-proposal.ja.md#4-部分ネットポートと観測)：標準はネットの統合を許し、プロジェクトは統合する |
| `partial_port_supply1_outer` | `1 1 / 1 1 / 1 1` | `1 z / 1 z / 1 z` | 23.3.3.7、28.15、[SIR_SPEC 3.5](../spec/SIR_SPEC.ja.md#35-ポートの畳み込み)、[MOS/ポートの取り決め 4](mos-switch-proposal.ja.md#4-部分ネットポートと観測)：標準はネットの統合を許し、プロジェクトは統合する |
| `partial_port_array_rank2` | `x x z / 0 0 z / z z z` | `x 0 z / 0 0 z / z z z` | 23.3.3.7、7.4、[SIR_SPEC 3.5](../spec/SIR_SPEC.ja.md#35-ポートの畳み込み)、[MOS/ポートの取り決め 4](mos-switch-proposal.ja.md#4-部分ネットポートと観測)：標準はネットの統合を許し、プロジェクトは統合する |

## 変換を伴うポート（6 件）

| プローブ | 期待値 | Icarus 13.0 | 根拠 |
| --- | --- | --- | --- |
| `converting_port_input_4_2` | `11 0011` | `xx 00xx` | 4.9.6、23.3.3.7、[変換のノート](converting-port-execution.ja.md)：変換を伴うポートは、強い値を持つ向きのある暗黙の代入のまま |
| `converting_port_input_2_4` | `1111 11` | `11xx xx` | 4.9.6、23.3.3.7、[変換のノート](converting-port-execution.ja.md)：変換を伴うポートは、強い値を持つ向きのある暗黙の代入のまま |
| `converting_port_input_2_4_signed` | `1111 11` | `11xx xx` | 4.9.6、23.3.3.7、[変換のノート](converting-port-execution.ja.md)：変換を伴うポートは、強い値を持つ向きのある暗黙の代入のまま |
| `converting_port_input_3_3_signed_actual_unsigned` | `111 111` | `xxx xxx` | 4.9.6、23.3.3.7、[変換のノート](converting-port-execution.ja.md)：変換を伴うポートは、強い値を持つ向きのある暗黙の代入のまま |
| `converting_port_output_3_3_signed_actual_unsigned` | `000 000` | `xxx xxx` | 4.9.6、23.3.3.7、[変換のノート](converting-port-execution.ja.md)：変換を伴うポートは、強い値を持つ向きのある暗黙の代入のまま |
| `converting_port_input_3_3_cast` | `111 111` | `111 xxx` | 4.9.6、23.3.3.7、[変換のノート](converting-port-execution.ja.md)：変換を伴うポートは、強い値を持つ向きのある暗黙の代入のまま |

## 残した部分 inout ポート（4 件）

| プローブ | 期待値 | Icarus 13.0 | 根拠 |
| --- | --- | --- | --- |
| `partial_inout_port_wand` | `0z 0 / 1z 1 / zz z` | `xz x / 1z 1 / zz z` | 23.3.3.7、28.12、[部分 inout のノート](partial-inout-port-execution.ja.md)：残した net 間の inout は、減衰のない恒等接続 |
| `partial_inout_port_triand` | `0z 0 / 1z 1 / zz z` | `xz x / 1z 1 / zz z` | 23.3.3.7、28.12、[部分 inout のノート](partial-inout-port-execution.ja.md)：残した net 間の inout は、減衰のない恒等接続 |
| `partial_inout_port_wor` | `1z 1 / 1z 1 / zz z` | `xz x / 1z 1 / zz z` | 23.3.3.7、28.12、[部分 inout のノート](partial-inout-port-execution.ja.md)：残した net 間の inout は、減衰のない恒等接続 |
| `partial_inout_port_trior` | `1z 1 / 1z 1 / zz z` | `xz x / 1z 1 / zz z` | 23.3.3.7、28.12、[部分 inout のノート](partial-inout-port-execution.ja.md)：残した net 間の inout は、減衰のない恒等接続 |

## 未確認

- 条件 4 を満たすかどうか。この表はその記録で、判定は M8 の報告で行います。
- 別のプラットフォームでも Icarus の出力が同じかどうか。これは macOS arm64 での結果です。
