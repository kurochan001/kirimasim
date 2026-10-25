# ソースから実行時indexで固定配列eventを選ぶ

[English](indexed-event-execution.md)が正。状態：実装・検証済み、2026-10-26。
旧CLI期待値の移行は別途`c53ccfe7`で承認を記録し、`28f5c972`で適用しました。
[移行後の検証](../measurements/2026-10-26/indexed-event-migration-validation.json)はlocal CI全21確認成功です。
dev/ASan/UBSanは5,139/5,141、releaseは5,140/5,142で、各失敗0・既知スキップ2件です。
下の移行前の失敗は過去の証拠として保持します。命令間safepoint案は引き続き未承認です。

## 契約と接続

根拠は承認済みautomatic捕捉契約5/7、SIR_SPEC 3.2.1/7/8、VALUE_SEMANTICS 3.11、
SCHEDULER_SPEC 9.4/9.5、IEEE 1800-2023 7.4.5・9.4.2・15.5です。
ソース・生成器・境界検査・故障は独自で、他のシミュレータの実装・テストは読んでいません。
依存・C ABI・IRの版・性能予算・回収時点は変えていません。

`2108dd72`で、`Lowerer::eventOperand`を実行時固定配列選択の`placeOf`/`loadPlace`へ
接続しました。既存の記憶域変換が宣言範囲・次元の順序・広い/X/Z index・ソース評価順を処理します。
妥当な定数static選択には従来の短いslot表現を保ち、既存SIR dumpは変えていません。
`.triggered`はslotならloadし、load済みのevent値なら直接使います。
VMに別の配列の意味は実装していません。

名前付き待機の対象は登録時に1回決め、後のindex変更で対象を変えたりtriggerとしたりしません。
`iff`/信号との混合待機も同じカーネル登録を使います。非ブロッキングの対象は任意の遅延より先に
1回評価し、遅延式内の変更で保留triggerを別対象へ向けません。
明示的な`.triggered`のmonitorは実際に読んだ対象を捕捉し、index変更で依存を付け替えます。
実行時の不正選択はnullとなり、即時/非ブロッキングtriggerは、overflowする遅延を指定しても
何も予約しません。問い合わせはfalse、待機は承認済みの位置付き実行時エラー・終了2です。

slangの厳格な診断は保っています。特に定数literalの範囲外indexは、従来のエラーへ昇格した
`-Windex-oob`診断で、変換より前に拒否します。実行時null経路を試すために緩めてはいません。
名前付きeventの暗黙感度も、入れ子/再帰関数から読む場合を含め、従来の位置付き診断で拒否します。

これはload済み/捕捉event全体に必要なfront接続で、新しい管理eventは作りません。
ソースのautomatic既定生成、event値の代入/比較/表示、命令間回収は有効にしていません。
別途提案したevent safepoint修正案は未承認のままです。

## 独自の仕組みの行列

`gen_indexed_event_matrix.py`は160設計を生成します。通常の64設計は8実行場所、
ローカル名/修飾名、昇順/降順/2次元配列、名前付き待機・混合`iff`・遅延対象の保持・
実際の`.triggered`読み取りを覆います。automatic task/forkでは、捕捉したautomatic indexを
使います。選択と遅延の関数で回数/順序を数え、引数とref書き込みは既存の共有scalar/ref経路を使います。

残り96設計は、4実行場所×全3配列形状×4種類の不正な実行時index（下限未満・X・Z・129bit値）×
trigger/問い合わせとnull待機です。新しい方針検査は暗黙感度の呼出し連鎖12件とliteral境界4件を
確認します。全160設計と全16境界が通りました。コンパイルした4故障は、実行時選択の拒否、
getter対象の誤り、選択式の二重評価、遅延内変更後の対象再評価を検出します。
復元した本番実装は、既存dump全29件と既存関数感度検査を含む対象192 CTestに通りました。

[故障検出の証拠](../measurements/2026-10-26/indexed-event-controls.json)には、4件すべてが
ビルド成功後に実際のテストで失敗し、復元後の対象192件が通ったことを記録しました。
[全local CI](../measurements/2026-10-26/indexed-event-validation.json)は17確認成功・CTest4段階失敗です。
初回devでは生成したsuite/generator件数も古いと判定されました。再生成後のdev全件再実行は
5,128/5,131成功で、失敗は`cli/options`だけ、既知スキップ2件です。
ASan/UBSanも同じ件数、releaseは5,129/5,132で、同じ1失敗と2スキップでした。
新規160設計・既存dump・PerfCountは通り、既存期待値は変更していません。
READMEの生成件数は、以前の全成功の区切りを残すのではなく、実際のdevの1失敗を示します。

[参照観測](../measurements/2026-10-26/indexed-event-reference-observations.json)には、正確なsource hash、
実際のコンパイラ/ランタイム版、成功したnative出力を保持しました。
[固定87項目の再実行](../measurements/2026-10-26/icarus14-indexed-event-probes.json)は以前の結果を維持しました。
母集団・sv-tests・Linux・速度比較は再計測していません。

## 比較と残る承認

導入済みIcarus 14開発版は、160設計すべてをコンパイルできませんでした。
別途再コンパイルした代表2本は、event配列未対応と明示的に診断しました。
その他の生の診断は保持していません。失敗した参照実行をnative期待値の裏付けには使いません。
期待値は仕様から独自に作り、`spec_check: ai`としています。Icarus 13は新たに実行していません。
既存87項目のnative/Icarus 14全判定と成功出力byteは変わらず、過去の13合格との差分は21件です。
有限の観測で言語全体の同等性やP1完了とは扱いません。

変更していない旧CLI入力`event_var_index.sv`は、無出力で成功し、時刻0で静止します。
未対応とする旧診断6件が、上に記録した移行前の`cli/options`失敗の原因でした。
[別途承認された移行](indexed-event-expectation-migration.ja.md)を`28f5c972`で適用し、
元ソースと他の全診断を保ったまま、受理項目1個を追加し、該当診断6行だけを除きました。
全CLIと全local CIは現在通ります。automatic生成と新しいGC時点は別の残作業で、
今回の承認には含みません。
