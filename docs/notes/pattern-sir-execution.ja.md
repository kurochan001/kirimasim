# 型付き代入パターン実行の区切り

[English](pattern-sir-execution.md)。2026-10-24。
[共有表示API](pattern-format-foundation.ja.md)へSIRと実行処理を接続しました。
ソースの`%p`は引き続き未対応で、frontと定数・runtime書式引数の接続が残っています。
P1の完了やIcarusとの全面的な同等性を示すものではありません。

## 実装した表現

`Design::patterns`に、不変の表示レイアウトを独立したPatternIdで保存します。
`pattern.format !pN, "named"|"positional", values...`は、既存SYSTEM_TASKS 3.9の
表記で文字列を返します。スカラー、real・string、enum名、record、固定配列、unionの
最初のメンバー、選択済みtag付きpayload、packedなbit範囲、型付き動的配列・queueを
記述します。明示的な表示情報で、実行時にソース型を推測しません。未対応のソース型や
class・eventの識別を有効にする変更でもありません。

検証器は参照、型、名前とescape情報、enum定数、packed範囲、引数の数・型、mode、
循環、個数のoverflowを検査します。グラフと引数の検査は反復処理で、葉が0個の巨大な
繰り返し形状を個数分たどりません。textには参照・mode・名前・offsetを出し、従来の
設計には新しい表を出しません。

VMは作業フレームで借用viewを組み立て、packedのoffset・符号・状態変換、実際の
heap型とruntimeの長さを使います。入れ子・空のコンテナも扱います。一時確保はContextの
heap resourceを使い、失敗はresource limitです。出力規則は共有rendererが決め、kernelを
SIR・slangに依存させません。公開schedulerヘッダはC++20です。

## heapの値を表示するmonitor

独自ソースで`$monitor("%0d", values[0])`の後にqueue要素を0から9へ変更すると、
従来は0だけでしたが、修正後は0・9を表示します。両Icarusは終了コード0でも、この
monitor式を実行時に拒否します。その診断を成功や期待値にはしません。

schedulerはPostponedで実際に表示本体を実行する間だけ、型付きheap読み出しを捕捉します。
余分な式評価は増やしません。slotのbindingと、読んだcontainer・cellの変更で次の表示を
予約します。依存は弱い参照で、表示ごとに置き換え、monitorの差し替え、ファイルclose、
callbackプロセスの解放、VM終了で外します。変更hookは既存のwait monitorと所有数を共有し、
最後の所有者がなくなると元のhookへ戻します。呼び出し内だけの値を除く既存filterも渡します。
登録番号により、callback中の差し替え・再配置へ古い捕捉を付けません。捕捉失敗ではcallbackの
プロセスを記録し、既存の位置付きresource診断に使います。

file monitorも同じAPIへ接続しました。ただし、heapを使うfile monitor、close・差し替え、
確保失敗、callbackの生存期間の組み合わせは、専用のsource・kernel行列を追加する必要が
あります。従来のファイル検査だけでそれらの組み合わせを確認したとは扱いません。
全ソースコンテキストでの対応は未確認です。

## 確認した証拠

独自SIRの11テストで、スカラー・enum・string・real、named・positional、packedの
状態・符号・offset、入れ子のコンテナ、空の値、4千段、text、不正なグラフ・範囲・引数・
mode、葉がない巨大形状、確保失敗を検査します。guardとdisplay monitorでは、ハンドルを
保ったcell変更を、直接とrecord内の計4組で確認しました。PatternSir・Pattern・Format・
Scheduler・VmContainerの選択した122件が成功しています。

混合状態のpackedフィールドの別の独自ソースでは、両参照がX/Zを残す一方、nativeは
宣言された2値メンバーを0へ変換します。7.2.1には、そのメンバーを読む際の4値から2値への
暗黙変換があります。この食い違いを新しい期待値に採用したり、既存87件へ混ぜたりしません。
SIRには明示的に選んだ型を残し、front接続時にもslangの型付けを維持する必要があります。

読み出し捕捉を共有したため、既存wait monitorの誤実装2件のアンカーを該当関数へ限定しました。
登録513件はすべて適用できます。今回、新しい誤実装をコンパイルして検証したとは主張しません。
既存suite本体、期待値、性能回数の期待値は維持しています。

## 再開する場所

まずdisplay monitorの生存期間・ファイル・resourceの専用行列を整え、その後にfrontの型・
フィールド・enum情報とsnapshotを`pattern.format`へ接続します。定数とruntimeの書式を
同じカーネルへ通し、引数順、Postponedの評価、root、書き戻しを守ります。ソース対応を
有効にする前に、古い`%p`の未対応期待値を別のレビューへ分けます。承認待ちのclass・event
契約は、この実装によって承認されたものとは扱いません。

## 区切り時点の全検証

[コードハッシュ・CI・実ソース観測](../measurements/2026-10-24/pattern-sir-validation.json)を
保存しました。ローカルCIの21項目がすべて成功し、dev・ASan・UBSanは各3,342件中
3,340件成功、releaseは3,343件中3,341件成功です。同じ既知2件のみスキップしました。
LLVM 22.1.8のformat・tidy、期待値保護、件数などの検査も成功しています。
source `%p`、新しいheap file-monitor行列、全面的な機能比較は残作業です。
