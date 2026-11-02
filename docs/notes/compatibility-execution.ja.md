# P1 のフロントエンド互換モードと実行用driver検査

[English](compatibility-execution.md) · 2026-11-03。

承認済み CLI 2.1 の転送を `--compat=default|vcs|all` として接続しました。引数全体を、
コンパイル・通常のparse・SIR出力の共通の固定版slang driverへ渡します。省略時は厳格な既定です。
独自プロファイル・言語の版の切替え・依存変更は加えません。slangは未改変の
`e2d1021661f78b3f17d1edf7ea17f9d6f95fb168` に固定したままです。

## 固定版の各モードが変えること

[機械可読のプロファイル一覧](../measurements/2026-11-03/compatibility-profiles.json)に、その版の
公開設定と診断定義にある全compilation/analysisフラグと診断の基準値の上書きを保存しました。
これはフロントエンド設定の証拠であり、影響する全形式のnative実行の証明ではありません。

- `default`（省略時も同じ）：互換用のcompilation/analysisフラグを足さず、通常のlocal include・
  macro規則と、既定の診断の基準値を使います。
- `vcs`：階層名の定数、整数からenumへの変換、宣言前の使用、stringから整数への変換、
  括弧を省いた再帰呼出し、括弧を省いたmoduleのparameter代入、自己決定のstreaming連結、
  ANSI portの再宣言・統合、配列連結中のassignment pattern、ライブラリ再定義時の最初の定義、
  古い`cross_auto_bin_max`、型付きinput portの変数としての推論を有効にします。
  local変数の複数driverの緩和と、記録した診断の上書きも使います。明示したincludeパスは
  ソースの場所より先に探索します。macroの最初以外の継続行ではbackslash後の末尾空白を許しますが、
  最初の継続はフロントエンドの制限を保ちます。
- `all`：同じ12個のcompilationフラグに、トップのinterface port、無名generateへの参照、
  bind/defparamで上書きしたinterfaceのvirtual-interfaceへの代入を加えます。analysisではさらに、
  特殊な手続きの変数へinitialも書く形を許します。診断の上書きは`default`・`vcs`と異なります。
  `vcs`のincludeパス優先・macro末尾空白の既定は**有効にしません**。単純な上位互換ではありません。

一覧にはerror/ignoredにする診断グループの正確な名前を保っています。`default`と`vcs`は異なる
グループをerrorにし、`vcs`と`all`は記録した9グループを無視します。明示的な`-W`は既存の
フロントエンドの警告方針に従います。モード名は、別の製品と実行結果が同じになるという主張ではありません。
より広い変換の制限は [P1対応表](../P1_SUPPORT.ja.md)に残します。

同じモード引数を繰り返すと、検査した直接/引数ファイルの両経路で固定版enum parserの**最後**の指定を保ちます。
空・未知の名前は入力エラーです。直接指定は文書化した `=<名前>` だけを追加し、既存の引数ファイルの扱いは
フロントエンドに任せます。

## 実行可能なSIRを作る前のソース診断

2026-10-31の互換モード観測に使ったソースはbyte単位で保持しました。`--compat=all`や複数writerの
診断を抑制した設定では、通常parseは成功しますが、従来の実行/SIR出力は検証器に達して内部エラー3でした。
[前後の記録](../measurements/2026-11-03/compatibility-validation.json)に12件ずつの観測と元のsource hashを保ち、
実行側をソースのコンパイルエラー2へ直しました。通常parseの結果は維持しています。

`front::checkExecutableDrivers` が、既存のSIR 3.4/10・scheduling 6.4のwriter制約
（IEEE 1800の6.5・9.2.2.2〜9.2.2.4）に従い、変換前に型付きの意味解析のdriver診断を検査します。
通常設定では既存analysisを再利用し、互換設定で検査を省いた場合は、ローカルなanalysisで2つのwriter緩和を
外します。元のdriverは変えません。特殊な手続きの複数writer、変数の複数連続driver、変数の混在driver、
未解決netのdriverの診断を型で選び、ソース位置とnoteを保ちます。警告を抑えても不正な実行用SIRは作りません。

これはソースの拒否であり、成功やSIR契約の書換えではありません。ほかの適格な設計を変換した後には
SIR検証器を実行し、その本当の不具合は内部エラーのままです。検証器のエラー文字列による分類、
全エラーの一括格下げ、実行時のモード分岐、ソースの書換えは使いません。analysisが所有する引数の生存中に
診断を整形し、変換前にanalysisを解放します。実行時の値・scheduler・force/strengthの意味、C ABI、公開kernelヘッダは不変です。

通常parseは、そのため実行の部分集合より広く受理する場合があります。従来からSIR変換を必要とする
compile-only VPIの経路は同じcompiler検査を使い、通常parseとは同一のAPIではありません。

## 仕組みの検査

生成器は**独自SV 106件とヘッダ2件**を作ります。うちdriver 96件はff/comb/latch、変数の連続・混在driver、
1/129ビット、module/interface/generate、直接/関数経由、重複/別オブジェクト・bitを組み合わせます。
モード側はenum、階層の定数、宣言順、string変換、includeの優先順位、後続のmacro継続、不正/重複モード、
通常の解決net・taskの書込み・宣言初期化の境界を扱います。

- モードのCLI **409項目**：直接/引数ファイル、引数の位置、通常parse・実行・SIR出力。
- driverのCLI **1,728項目**：厳格/警告抑制/all。拒否は位置付きエラー、設計/SIR出力なし、終了2を要求し、
  クラッシュ・ハング・内部エラーは失敗です。別の範囲、解決net、taskの書込み、宣言初期化は適格のままです。
- API単体4件：compile結果の分類、parseとの分離、重ならない広幅bit、独立した呼出しを繰り返す場合の状態を検査します。
- **コンパイルした意味の変異8件を全て検出**：モードの転送、厳格な既定、互換設定の再解析、抑えたraw診断、
  writerの3種類、変換前の停止です。ビルド失敗・時間切れを検出数へ加えず、source hashと正常系9テストを復元しました
  （[記録](../measurements/2026-11-03/compatibility-controls.json)）。全1,149件の変異anchorが適用でき、重点clang-tidyも成功しました。

既存期待値、固定P1母集団・比較は変えていません。Icarus/sv-tests/fuzzの再測定、商用ツールの実行、上流投稿、
新しいパッケージのインストールは行っていません。**P1は進行中です。** dev全体と最終local CIの結果は完了後に追記します。
