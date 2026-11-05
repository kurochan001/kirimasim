# 最初の型付きコンテナの暗黙監視

[English](implicit-container-execution.md) · 2026-11-06。

[契約の1〜8節](implicit-container-observers-proposal.ja.md)は2026-11-06に承認され、
`8873c89a`で実装しました。[既存11件の拒否判定](implicit-container-expectation-migration.ja.md)も
別途承認を受け、`b3d20b8b`で移行しました。変更はローカルにあり、mainへは未pushです。

## 実行できる範囲

暗黙感度は、値が変わったときに本体を再実行するための依存先です。
適格な静的変数にある整数要素の動的配列・キューを、comb/latch、先頭の`always @*`、
直接の連続代入で、型付きの値全体への依存として扱います。上限付きキューと対応する
2値/4値の幅も共通の経路です。サイズ問い合わせ、値渡し引数、comb/latchの通常・入れ子の
関数読み出し、要素を読むwildcardの識別子全体への依存を検査しています。
module・interface・generate、unit/package、非パラメータ化クラスのstatic、適格な
静的サブルーチン変数の修飾名を含みます。

frontが静的に依存先を選び、SIRがslot・型の結び付きを保持・検証し、VMが共有カーネルへ
渡します。カーネルは利用者のコードを評価せず、長さと順序付きの正規化した要素ビットを
X/Zも区別して比較します。要素が変われば、sizeの結果が同じでも本体を再実行します。
同値のコピーでは起床せず、置換後のセルへ監視を付け直します。弱い印と生の値のワードは、
GC rootや言語のコピーを増やしません。走査中の確定したA→B→Aも観測し、複数セルの操作は
完了した値を公開します。待機は1回だけ起床し、既存のActive/Reactive領域で実行します。

comb/latchの感度へ寄与する要素選択、automatic/refの結び付き、入れ子の暗黙制御、
非整数・入れ子の要素、連想配列・classに依存する監視、呼出先本体でコンテナを読む
連続代入は最初の範囲に含めません。違法なソース形式へのslangの厳格な診断も保ちます。
必要な境界はP1の残作業であり、新たな対象外にはしません。明示イベント式の合法性と
結果の監視も変えません。

## 確認と制限

[ソース行列](../measurements/2026-11-06/implicit-container-source-matrix.json)は、
**実行584件と、フロントエンドの拒否を期待する12件**です。596件すべての検査が成功し、
拒否対象は拒否したままです。明示した`bit`/`logic`の幅を正規化してから、値の変化と
起床回数をモデルで決め、シミュレータの出力から期待値を作りません。
以前の[96件の候補の調査](../measurements/2026-11-05/implicit-container-review.json)はそのまま保ち、
`int`とX/Zを使う候補は今回の受入ゲートにはしません。そこに記録した単純な参照通過の
差20件は、今回実行できるようになりました。

[検証記録](../measurements/2026-11-06/implicit-container-validation.json)は、
除外なしのdev **12,262/12,262件**、失敗・skipなしと、関連するrelease・ASan・UBSanの
**各1,003/1,003件**の成功を記録します。既存の上限・ORのテストを拡充した後も、
kernel/SIRの8テストが4プリセットすべてで再び成功し、件数は変わりません。
frontの新規2テストは、静的な閉包、重複除去、私有・書込み先の除外を検査します。
件数は別コミット`08a733f2`で更新しました。

**実際にコンパイルした12故障**をすべて検出しました。トークンだけ・サイズだけの比較、
X/Zの同一視、最終値まで遅らせた比較、変数の公開の見落とし、同値コピー後の古い依存、
登録時の言語コピー、wildcard登録時の本体実行、修飾した記憶域の欠落、記述子の検証漏れ、
誤ったwildcardの呼出し閉包、フック所有の解除漏れです。復元後にkernel/SIR・front・
596ソースの検査が成功し、元のソースのハッシュも一致しました。

format、変更の影響を受けるTUのclang-tidy、SPDX、禁止語・大域状態、releaseフラグ、
期待値分離の検査も成功し、変異の1,216箇所がすべて適用できます。このブランチの
完全な`tools/local_ci.py`とGitHub CIは未実行です。承認されたpushの前に全local CIを実行します。

[188プローブの測定](../measurements/2026-11-06/icarus13-implicit-container-checkpoint.json)は、
既存の入力と期待値をすべて保ちます。P1評価187件・任意の`real_net`対象外1件で、
必須の`dense_udp_coverage`の時間超過と、別分類の参照不一致25件を残します。
新規ソース行列はP1合格率ではありません。Icarusとの全面的な同等性やP1完了の認定ではなく、
標準の`real`の範囲と固定した外部母集団は変えません。

```sh
python3 tests/cli/run_implicit_container_tests.py --self-check
python3 tests/cli/run_implicit_container_tests.py --kirimasim build/dev/src/driver/kirimasim --json build/implicit-container-source-matrix.json
ctest --preset dev -R '^ImplicitContainer|^ImplicitFront|^cli/(dynamic_containers|implicit_containers)$' --output-on-failure
python3 tools/mutants.py --check
```

コンパイルした故障の検査は、正常な関連テストの基準実行と、登録済みの置換を使いました。
変異の適用箇所の検査だけで検出したとは扱いません。全変異の検査や全local CIではありません。
外部への報告は行っていません。
