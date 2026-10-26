# クラスのstatic記憶域と事前初期化

[English](class-static-execution.md)。状態：2026-10-27、実装・ローカル検証済み。
実装`9e6a149d`をmainへ取り込みました。
[取り込み記録](../measurements/2026-10-27/class-static-main-integration.json)で検証済みの実装・テストの不変を確認します。
元の検証記録は、測定時点の未commit状態を履歴として保持します。P1とクラスオブジェクト実行は未完です。

## 契約と実装

根拠は承認済みの[クラス契約2節](class-runtime-proposal.ja.md)、SIR_SPEC 7.1、
VALUE_SEMANTICS 1.2/1.4/3.13、IEEE 1800-2023 6.21/8.9/13.5です。
テストと故障注入は独自に作りました。他シミュレータのソース・再現コード、規格の本文・例、
新しい依存・インストールは使いません。slangは固定版を改変せず、その意味モデルのsymbolで
識別とアクセス権を決めます。外部の設計資料を新たに参考にはしていません。

`Lowerer::declareClassStatics`を、非パラメータ化クラスの宣言からの入口にしました。
`declareScope`は、そのクラス自身が宣言したstaticプロパティだけを通常の変数・複合型の
slotと初期化の経路に渡します。instance fieldはオブジェクト生成まで初期化しません。
canonicalな宣言クラスの集合で二重訪問を防ぎます。この集合は所属確認だけに使い、
走査でSIR番号を割り当てません。継承したプロパティは元のsymbol/slotを共有し、同名で
隠蔽した再宣言には別の記憶域を持たせます。別々にエラボレートした字句的な所有者は、
文字列の名前で統合せずslangのcanonicalな識別に従います。packageクラスは利用側の
instanceを越えてpackageのrowを共有します。

`declarePackages`が、未参照のクラスも含め、コンパイル単位・package・匿名programの
記憶域を見つけます。instance化されたmodule・program・interface、generate、入れ子の
クラスも同じ宣言入口を通ります。各宣言の位置で、自身のstaticプロパティを宣言順に
既存の時刻0前のstatic-initへ追加します。通常の既定値による初期化が先です。
修飾名による前方参照にも既存の初期化順の警告を出し、instance fieldや継承した宣言を
重ねて初期化しません。

共通の`isVariable`/`inSlot`にClassPropertyを含め、型付きの読み書き、ref/output/inout
の実引数、automatic寿命の判定、初期化順の警告に使います。staticメソッドは既存の
サブルーチン変換を使い、待つtask・NBA・event待ち・組合せ感度も既存の型付きslotの
経路を通ります。VMのソース形状による分岐、kernel操作、SIR命令/版、ABIの追加は
ありません。可変のシミュレーション状態は各Contextが持ちます。

パラメータ付きstatic記憶域は未完です。直接参照は`newSlot`で位置付きの未対応診断を
出します。`checkGenericClassStatics`は、slangが実際に特殊化した未使用のstatic記憶域も
診断し、初期化の副作用を落として正常終了することを防ぎます。遅延したエラボレーションの
前に特殊化をsnapshotし、固定点まで調べ、宣言ごとに1件の診断をソース位置順に出します。
hashの探索順でSIR番号を決めません。プロパティのないgeneric staticメソッドは利用できます。
これは境界の検査であり、契約1/2節が求める決定的なgeneric特殊化の実行ではありません。

小さいスタック上で宣言探索が停止しても、`compile`の境界で既に記録した診断を返します。
追加した探索が例外を外へ漏らす回帰を既存のdefault-threadテストが検出したため、修正し、
class宣言を含む3か所の追加テストと故障注入でも確認します。

## 検証

[生成器](../../tools/gen_class_static_matrix.py)は**563設計**を追加します。
宣言場所8種類・型14種類・名前の表記4種類の448設計、未使用/継承/別名の探索24設計、
隠蔽8設計、初期化24設計、引数方向20設計、待ちとActive/NBAの24設計、
組合せ感度12設計、canonicalな所有者3設計です。構造体memberの既定値は定数式とし、
副作用は通常のstatic初期化で別に検査します。匿名programの別名宣言は、禁止された匿名の
直接memberとして置かず、合法な利用側の名前付きprogramに置きます。

[契約チェッカー](../../tests/tools/check_class_static_contracts.py)は、位置付きの境界9件、
プロパティのないgenericメソッド、SIR上の記憶域識別2設計、修飾名の前方参照警告、
moduleの副作用より前のfatal/finish停止2件を検査します。生成器と合わせて565件、
小さいスタックの3件を合わせて**568件の新規CTest**です。既存のContextのsequential・
parallel・churnにも、整数・string・queue・eventを持つクラスの設計を追加し、独立した
Contextの結果と、繰り返したコンパイル・実行を比較します。

[修正前後の観測](../measurements/2026-10-27/class-static-before-after.json)では、以前は
診断なく初期化を省略した未使用宣言3例が`calls=2 trace=12`になり、別のmodule宣言1例は
未対応から同じ出力になります。保存した修正前のバイナリと現在のdevバイナリのSHA-256と
実測の版文字列を記録します。有限の独自テストでクラス全体の同等性を認定しません。

[コンパイルした7故障](../measurements/2026-10-27/class-static-controls.json)を検出します。
探索の省略、scalar初期化の省略、instance fieldの先行初期化、クラスの二重初期化、
genericの黙った省略の受理、初期化順警告の省略、宣言探索の例外の漏れです。
故障の置換箇所を照合するだけの検査を検出根拠から除き、実際のsource/contract/Context
の実行で検出します。元のソースをbyte単位で戻し、**571件の選択テスト**が通ります。
新規568件と既存Contextの3モードです。

全local CIと更新した回帰の結果は[検証記録](../measurements/2026-10-27/class-static-validation.json)
に記載します。最終の全local CIは**21/21項目成功**です。dev/ASan/UBSanは各
6,625/6,627、releaseは6,626/6,628、失敗0・既知skip各2件です。
PerfCount・804件の故障置換箇所・format・clang-tidyの115翻訳単位も通りました。
既存の保護されたテスト本体・期待値は基準commitから不変です。測定時はcommit前で
topicにupstreamがなかったため作業ツリーの差分を確認しました。その後、実装commit単位の
期待値保護検査も通過したことを取り込み記録に示しています。

[固定母集団](../measurements/2026-10-27/class-static-population.txt)は
1,456/1,466、失敗0・未対応10です。
nativeの[sv-tests](../measurements/2026-10-27/class-static-svtests.json)は244/344、
未対応62で、以前の合格を失いません。独自87プローブの
[Icarus 13](../measurements/2026-10-27/icarus13-class-static-probes.json)と
[Icarus 14](../measurements/2026-10-27/icarus14-class-static-probes.json)の比較は、
差7件・参照と記述したモデルの不一致4/7件を維持し、nativeの合否・stdoutも不変です。
以前の[クラス調査](class-execution-investigation.ja.md)は日付付きの修正前の記録として残します。
既存の期待値・仕様・性能予算は変更しません。新しい実時間の速度比較、Linux実行、
ホスト側CIは主張しません。

## 制限

既存のslotが扱える型を持つ、非パラメータ化クラスのstaticプロパティに対応します。
packed整数の2/4値、real/shortreal、string、event、固定配列、対応するunpacked構造体、
整数要素のqueue/動的配列です。各操作の既存の制限を引き継ぎます。
クラスscope/propertyの波形選択には対応しません（WAVEFORM 7）。

クラス生成、object handle/layout、instance field、receiver/virtual dispatch、
object RNG・所有・GC、パラメータ付きstatic実行は残作業です。
承認済みのクラス契約全体の実装ではありません。既存87プローブの差と有限の母集団の
件数だけでは、P1の完了を認定できません。
