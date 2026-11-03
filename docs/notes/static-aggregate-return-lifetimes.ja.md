# 集約型の関数戻り値変数の寿命

[English](static-aggregate-return-lifetimes.md) · 2026-11-03。

P1の残課題だった、static関数のunpacked配列・構造体の戻り値が保持されない不具合を修正しました。
IEEE 1800-2023の6.21・13.4.1に従い、関数名を持つ戻り値変数も関数と同じ寿命を持ちます。
SIR_SPEC 3.3/8と、承認済みのscalar戻り値の移行にも反映済みの規則です。
staticの戻り値変数は代入した値を呼び出し間で保持し、automaticは呼び出しごとに初期状態になります。

## 実装

`Lowerer::subroutineOf`で、集約型の名前付き戻り値変数にも既存の`Storage`記述子を使い、
staticは通常の宣言slot、automaticはframe変数へ配置します。`lowerReturn`は集約代入・コピーの
共通経路を使います。隠しoutputの`$return`は、永続する宣言の値とは別に、呼び出し元へ値を転送します。
構造体メンバーの初期値も、staticは既存の初期化経路で一度だけ、automaticは呼び出しごとに適用します。
新しいSIR命令、runtime API、依存は追加しません。

staticの戻り値メンバーに初期値があるpackage／unitでは、instanceの初期化より前に
static-initプロセスを確保します（SIR_SPEC 7.1）。これにより、静的な宣言初期化から
関数を呼ぶ場合にも初期値が間に合います。関数本体は従来どおり使用時に変換します。

固定配列、入れ子の構造体、構造体配列に適用し、幅の広い4値、2値、real、stringのleafも扱います。
返却した値は独立したコピーです。module instanceごとに記憶域を分け、packageとcompilation unitは
既存の宣言所有者に従います。eventを含む構造体の戻り値は、従来と同じ位置付き未対応エラーを保ちます。

## 検証と再現

[ソースhash付き記録](../measurements/2026-11-03/static-aggregate-return-lifetimes.json)には、
保存した修正前バイナリで**staticの960検査が失敗、automaticの480検査が成功**した結果も残します。
修正後は**独自360ソース・1,440検査がすべて成功**しました。宣言scope 6種、集約形5種、
寿命宣言3種、戻り方2種、呼び出しの入口2種、初期値方針4種を組み合わせます。
initial手続きと静的な宣言初期化から呼び、部分書き込み、返却値のコピー、メンバー初期値、
再帰、独立したmodule instanceを検査します。以前の寿命プローブ16件も成功しました。

フレーム配置へ戻す変異、staticメンバーの初期値を毎回適用する変異、packageの初期化を
呼び出し元より後へ遅らせる変異の3件を実際にコンパイルし、意味のアサーションで検出しました。
カタログ自身の検査は含めません。ソースhashを復元すると全行列が再び成功します。
dev全件も**11,829/11,829成功**で、
既存SIR出力とPerfCount期待値を含みます。既存の入力・期待値は変更していません。

```sh
python3 tests/cli/run_aggregate_return_lifetime_tests.py \
  --kirimasim build/dev/src/driver/kirimasim
```

記録済みの寿命の不具合を解消したもので、P1の機能範囲全体の完了を認定するものではありません。
外部母集団と有限のIcarusプローブは再測定していません。
固定版slangは未改変で、上流への投稿保留も継続します。
