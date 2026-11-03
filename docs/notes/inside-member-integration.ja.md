# 既存inside集合要素修正の取り込み

[English](inside-member-integration.md)

2026-11-03、ローカルmain `26a3e5a3`に未コミットの変更を加えた記録です。
upstream mainは`f3ea0379`です。先行するローカルの文書2コミットと、無関係な
ハンドオフファイルは保持しています。この作業ではコミット・pushを行っていません。

## 規則と実装

修正は既存ブランチの`b16163c7`にあり、現在のmainには入っていませんでした。
そのソース変更、生成器、新しいmatrix入力、mutation定義を作業ツリーへ取り込みました。
ツール検査の登録だけは現在のCMakeファイルに合わせました。既存期待値と
既存のsuite入力は変更していません。

VALUE_SEMANTICS 2・3.10とIEEE 1800-2023 §11.4.13・11.8.2・12.5.4に基づき、
inside集合の整数配列要素は比較の共通型の拡張規則を使います。要素自身の符号で
拡張すると、誤った上位bitや余計なワイルドカードbitが生じます。
`Lowerer::memberMatch`がスカラ・列挙要素と読出した配列要素の比較を集約し、
`Lowerer::propagate`が共通型による拡張を適用します。通常のinside、case inside、
サブルーチンや連続代入からの入口も同じ経路を使います。kernelの値の意味と
参照実装は変更していません。

## 検証

既存の`tools/gen_inside_member_matrix.py`からinitial/task/always/forkの
新規suite入力4件を生成します。各120出力行で型、語境界、不定bit、配列次元、
保存場所と配列・列挙・case insideの観測を照合します。絞ったCTestは12件すべてPASSです。
[修正前後のharness記録](../measurements/2026-11-03/inside-member-baseline.json)では、4件とも元のバイナリで失敗し、修正版で合格します。
[全local CIの記録](../measurements/2026-11-03/inside-member-validation.json)も21項目すべてPASSで、dev・release・ASan・UBSan、releaseフラグ、
静的規約、format、tidy、期待値保護、件数を確認しました。
dev・ASan・UBSanは各11,794件、releaseは11,795件がPASSし、除外はありません。

外部の2,785件の選択は入力を変えず、insideの2件だけがfailからPASSへ変わり、
2,247 pass・529 unsupported・残り9 failでした。元のsnapshotと全出力の再実行は
kirimasim-testsに保持しています。この時点ではラベル取消しの5件が
[意味規則の修正案](deferred-label-cancellation-proposal.ja.md)の承認待ちでした。
承認後の[実装と検証](deferred-label-cancellation.ja.md)で、この5件も修正しました。
configの4件は[固定版エラボレータ](config-reset-dependency-gap.ja.md)へ切り分けました。
依存の分離診断パッチは、本番にも今回の全local CIにも使っていません。

正式Nightly、7連続Nightly fuzz、固定P1母集団、最小コンパイラCI、
外部の固定oracleスイート全件の新規実行を確認したという意味ではありません。
ゲートの分母とseedの方針は変更していません。
