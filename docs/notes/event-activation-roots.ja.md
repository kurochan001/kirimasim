# 型付きeventフィールドと保持されたactivationのroot

[English](event-activation-roots.md)が正。状態：部品を実装・検証済み、2026-10-25。
Interpreterのroot公開とソース生成は残作業です。

**後続の接続（2026-10-26）：** [VMの境界部品](event-vm-boundaries.ja.md)で、通常rootを
既存safepointへ公開します。命令間safepointに必要なSSA全体の公開とソース生成は残作業で、
以下は以前の部品段階の記録です。

**後の承認（2026-10-26）：** 人が別途、[管理eventのsafe point修正](managed-event-safepoints-proposal.ja.md)の
推奨案1を承認し、RUNTIME_ABI 6へ反映しました。以下の未承認という記述は以前の区切りの
状態を記録したものです。完全な公開手順と検証の実装は残作業です。

## 契約と実装

根拠は承認済みの[automatic捕捉契約](automatic-capture-proposal.ja.md)5/6/7、
SIR_SPEC 3.2.1/7/8、RUNTIME_ABI 4/6とIEEE 1800-2023 6.8・6.21・15.5です。
実装とテストは独自で、外部の実装・テストコードは読まず、依存と性能の基準も変えていません。

`1dc06096`で正規の`event`型だけをactivationフィールドに受理します。
`ActivationPlan`は既存カーネルの64bit型付きevent providerへ対応させ、
不正な幅はlayout公開前に拒否します。子が別のフィールドだけを捕捉していても、
保持された字句領域全体を訪問し、未読のスカラーと固定配列eventも含めます。
通常の整数フィールドに同じbit列があってもeventを保持しません。
名前付き待機は、元スコープが終了しても選択した具体的対象をキャンセルまで保持します。

`FrameLayout.eventRoots`は通常のEventの**行番号**を、container/ref rootと別に記録します。
配列の最終offsetは要素列挙より前に決まります。scoped/captured行は、保持された
カーネルactivationのproviderを使います。Interpreterから管理表への通常行の公開は
まだ接続していません。この表現だけでは`event.new`実行やソースのautomatic eventを有効にしません。

別途承認済みの[旧入力移行](event-field-expectation-migration.ja.md)の項目1/2を
`ecbe4abb`で適用しました。正規幅の入力だけを不正幅に変え、拒否検査と他のcaseは保ちました。
項目3のソースメタデータ入力は変更せず、接続待ちです。

## 証拠と限界

独自の`EventActivation`6テストで、正規スカラー・昇順/降順固定配列・2次元配列・
全3join・未読領域全体・複数の子・同一性共有・待機pin・整数の除外・通常配列の最終offsetを
検査します。承認済み移行後、対象51テストが通りました。再進入検査は時刻0で512周し、
物理event行1個を再利用しますが、回収は**明示的に**呼びます。
Interpreter内の待機なしループで確保量が有界になる証拠ではありません。

[コンパイルした5故障](../measurements/2026-10-25/event-activation-controls.json)は
すべてビルド成功後に実際のテストで失敗し、復元した本番実装は対象51件に通りました。
[全local CI](../measurements/2026-10-25/event-activation-validation.json)の21確認が通り、
dev/ASan/UBSanは4,954/4,956、releaseは4,955/4,957、失敗0・各既知スキップ2件です。
既存dump全29件とPerfCountも通り、README件数はdev JUnitから生成しました。

新しい[87プローブ実行](../measurements/2026-10-25/icarus14-event-activation-probes.json)は、
native/Icarus 14の全判定と成功時の全出力byteを維持しました。binaryが示す構成時の版は
実際に`2866a0b62a05`で、その構成時に未コミットだったproviderを後で`1dc06096`にしました。
版文字列をcleanなソース同一性とみなさず、記録には正確なsource hashを残しました。
過去のIcarus 13合格との差分は21件で、言語全体の一覧ではありません。
Icarus 13・母集団・sv-tests・Linux・速度は今回再計測していません。

## 次の接続と判断

型付きslot・通常/scoped/captured/input・一時値を公開し、カーネル経由で型付き操作を
実行して、ソース行列全体が揃ってからfrontを有効にします。
未公開の引数・戻り値などは、callbackとエラーの間も所有する必要があります。
[管理eventのsafepoint修正案](managed-event-safepoints-proposal.ja.md)は**未承認**です。
現在のRUNTIME_ABI 6は回収をslot/region境界に限定しています。提案では、待機なしの
有界な再利用を示すためにeventだけの命令間safepointが必要となる理由を説明しています。
この部品では新しい回収タイミングを有効にしていません。P1とIcarus全範囲の対応は未完了です。
