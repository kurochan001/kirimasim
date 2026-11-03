# 宣言したオブジェクト型に応じたVPIの値形式

[English](vpi-object-value-formats.md) · 2026-11-03。

承認済みの[VPI契約3〜5節](vpi-p1-proposal.ja.md)の範囲で、`vpiObjTypeVal`が宣言の情報から
scalar・integer・real・vectorの返却形式を選ぶようにしました（IEEE 1800-2023 38.22。
releaseの結果も38.34に従い同じ変換を使います）。従来はreal以外を常にvectorにしていました。
通常の読み取り、値変更callback、変数のreleaseが`Vpi::Impl::convert`を共有します。
明示的に指定した形式の変換と寿命は従来どおりです。

slangのcanonicalなscalar型をSIRの静的reflection情報として保持し、1ビット整数の格納先との
整合性を検証してVPI hostへ渡します。1ビットのpacked宣言はvectorのままです。
これらの宣言とpackedのbit handleの`vpiScalar`・`vpiVector`も幅ではなく型情報を使います。
SIRのテキストはreflection行を`scalar`・`integer`・`vector`に区別します。既存の承認済み
型付きreflection契約の実装で、SIR v1の文法の確定ではありません。公開C VPI ABIと依存pinは
変えません。kernel公開ヘッダはC++20で、kernelへslang・SIRの依存も足しません。

hostのintegerオブジェクト型は`vpiIntVal`、real記憶域は`vpiRealVal`、宣言のscalar・packedの
bit handleは`vpiScalarVal`、残る対応するpacked宣言は`vpiVectorVal`を選びます。
X/Z、整数の符号、出力バッファの所有、callbackのスナップショットは共通変換で保ちます。
明示formatの読み取りには追加のmetadata探索を行いません。形式を決めるためにソース式を
再評価しません。

## 独立した検査

[独自の生成器](../../tools/gen_vpi_object_format_matrix.py)は**87プログラム**を作ります。
宣言場所8種・型9種の変数72件と、読み取り専用のnet 15件です。独立したCプラグインが
引数と名前による読み取り、変数force/releaseの返却値、値変更callbackの実際の値を検査します。
netは解決値の読み取りだけで、net書込みモードを追加しません。
[更新前後の記録](../measurements/2026-11-03/vpi-object-format-matrix.json)に全件の入力hashと
結果を保存し、nativeは**50/87から87/87**になりました。

追加C++テスト6件は、scalarと1ビットvectorの区別、符号、scalarの全4値、明示formatの維持、
real・広い値、callback内の入れ子の読み取り、packedの添字、frontのmetadata、不正なreflectionの
拒否を確認します。[コンパイルした故障対照6種](../measurements/2026-11-03/vpi-object-format-controls.json)
はビルド成功後の意味の検査で全て検出し、復元後のソースhashとテストも確認しました。
最初の対照の1つはC++の名前空間指定を誤ってビルドが失敗したため検出に数えず、修正後に
ビルドして意味の検査で検出しました。

Icarus 13.0との完全一致は**17/87**です。64件では整数系callbackに対し
`value format 12 not supported`と診断し、ほかに1ビットvector・integerの形式やpackageの
名前探索の差も観測しています。参照検査の成功に置き換えず、差として保存します。
nativeの期待は規格の宣言したオブジェクト分類と承認済みreflectionモデルに従います。
有限の比較からVPI全体の同等性を認定しません。

## 残る範囲

この区切りは対応するstatic数値宣言全体とpackedのbit handleです。式だけの値、全ての選択・
集約オブジェクト、time専用形式まで含む自動分類は残作業です。net deposit・全体のforce、
追加VPI API、cocotb連携も従来の境界に残します。この制限を理由にP1からテストを除外せず、
`wire real`の例外、固定プローブ、gate母集団は変えません。
以前の[自動形式の観測](../measurements/2026-11-03/vpi-release-auto-format-observation.json)は
調査時の履歴を保ちます。既存の期待値や入力は変更していません。上流への投稿保留も維持します。

## 最終のローカル検証

`d5a308ab`で`python3 tools/local_ci.py`の全21項目が成功しました。
dev・ASan・UBSanは各**11,794/11,794**、releaseは**11,795/11,795**で、skipはありません。
clang-format、clang-tidy（191翻訳単位）、公開ヘッダのC++20ビルドと残る検査も通過しました。
実行中は追跡対象ファイルを固定し、終了後にhashの一致を確認しました。スクリプトの
dirty表示は未追跡のローカル報告資料によるもので、今回の変更には含めず、変更もしていません。

ブランチにupstreamがないため、スクリプト内のコミットごとの期待値検査は実行されません。
今回の2コミットは`check_expected_change.py`で別途検査し、両方通過しました。
[検証記録](../measurements/2026-11-03/vpi-object-format-validation.json)に対象コミット、
構成ごとの件数、所要時間、個別の期待値検査を保存しました。Linux・最小コンパイラ・nightlyは
ここでは再実行していません。この区切りのmain統合とpushは未実施です。
