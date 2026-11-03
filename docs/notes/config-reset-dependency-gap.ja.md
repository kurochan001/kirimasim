# configリセットと固定版エラボレータの不一致

[English](config-reset-dependency-gap.md)

2026-11-03の測定です。依存の診断であり、本番への修正適用ではありません。

## 確認できた境界

外部の第33×27章のクロス4件で、defparam指定もある対象実体を`use #()`で
リセットした結果が異なります。リセットの優先順位の根拠はIEEE 1800-2023 §33.4.3です。
保持した期待値のW/N/MODE/TAGが生成形状を決め、兄弟実体を別の対照としています。

[単独プローブ](../measurements/2026-11-03/config-parameter-probe.cpp)は固定版slangの
ライブラリと公開AST APIだけを使い、kirimasimのfront・SIR・kernel・VMをリンクしません。
元の41入力を問い合わせると、[37件一致・同じ4件不一致](../measurements/2026-11-03/config-parameter-elaboration.json)でした。
入力ハッシュと問合せ対象のシンボルも記録しています。これにより、4件の値の誤りは
kirimasimのloweringより前の、依存によるエラボレーションへ切り分けられました。

固定版`e2d1021661f78b3f17d1edf7ea17f9d6f95fb168`では、空のconfig指定が代入表を
消去し、後からdefparamを抑止するためのconfig由来の項目が残りません。
一方、名前付きの空指定は、置換式を持たない項目で指定元を保持します。
ソースの確認は、プロジェクト方針で許されているMITライセンスの依存だけに限定しています。

その後採用された未改変のd02b7cda pinでも、[単独プローブを再実行](../measurements/2026-11-03/config-parameter-elaboration-d02b7cda.json)し、37件一致・同じ4件の不一致を確認しました。承認済みラベル取消しの実装は、この依存側の問題を変更しません。上記の実験用パッチは本番と分離したままです。

## 分離した診断用パッチ

[実験用パッチ](../measurements/2026-11-03/slang-config-reset-diagnostic.patch)は、
名前付きリセットの既存表現を用いて、上書き可能な全パラメータをconfig由来のリセットとして登録します。
別のオブジェクトと静的ライブラリの複製だけに適用し、本番の依存とシミュレータは変更していません。
同じ問合せは[41件一致・不一致0件](../measurements/2026-11-03/config-parameter-diagnostic-patch.json)となりました。
確認したのはパラメータのエラボレーションだけです。全シミュレーション、上流での採用、
依存のpin更新、本番の回帰PASSの証拠ではありません。

本番への取り込みは未解決です。承認済みpinのslangを未改変で維持し、4件の失敗を残し、
上流へ適用可能な修正または承認を伴う依存方針変更を準備します。
将来pinを更新する場合は、その時点のバイナリでconfig全41件と全local CIを確認します。
観測されたdefparam値へ期待値を置き換えてはいけません。上流へのIssue・PR送信は行っていません。

## 再現

プローブには、入力、明示したconfigトップ、階層パラメータ名を渡します。

```sh
config-parameter-probe input.sv cfg:config \
  cfg_top.target.W cfg_top.target.N cfg_top.target.MODE cfg_top.target.TAG
```

devのコンパイルデータベースのincludeとdefineを使ってビルドし、既存`libsvlang.a`と
driverのNinjaリンク命令にある依存ライブラリをリンクします。分離実験では、
`ParameterBuilder.cpp`自身のコンパイル命令で変更した一つのオブジェクトだけを作り、
複製したarchiveのそのオブジェクトを交換して、プローブを再リンクします。
利用中のビルドの依存archiveは交換しません。記録にはソース・入力・ライブラリ・パッチのハッシュを残します。
