# automatic引数のVPI宣言メタデータ

[English](vpi-argument-declarations.md) · 2026-11-06。

ローカルの`c0b047e3`で実装し、mainへはまだpushしていません。

`logic signed [-3:61] value`の宣言添字は、正規化した格納オフセットとは異なります。
このautomatic変数全体をCプラグインへ渡したときも、`-3:61`の宣言範囲を保つようにしました。
`vpi_handle_by_index(argument, -3)`で正しいビットを読み、既存のアクセス権限に従って即時に書けます。
[VPI契約3〜6節](vpi-p1-proposal.ja.md)の承認済みの宣言保持・寿命規則を
[VPI_SPEC](../spec/VPI_SPEC.ja.md)に沿って実装する変更で、新しいVPI APIやC ABIは追加しません。

## メタデータと数値のアクセス権限

frontは、対応するautomaticの整数引数全体について、任意のSIR `foreign.argument`宣言を記録します。
範囲、宣言上のscalar/vector、既存の整数オブジェクト分類を、呼出し箇所IDと引数番号へ対応付けます。
既存のsyscallの引数組と数値参照は保ちます。静的reflectionも同じ宣言形状のヘルパーを使います。
SIRは実行時の記憶域をVMが確保する前に、範囲と幅、フラグ、整数型、全体のオフセット、変数binding、
対応の一意性、有効な呼出し・引数を検証します。この任意の記述子がない従来のSIRは既存の動作を保ちます。

VMは検証済みの宣言を`compiletf`の前に共有ホストへ渡します。実行時の値は元の評価と検証済みbindingから
取得し、ソース式を再評価しません。scalarの`logic`とpackedの`logic [7:7]`は、どちらも1bitでも区別します。
選択したビットは符号なしで、承認済みの値形式の自動選択ではscalarを選びます。

選択した引数の親は数値bindingのスナップショットで、次の呼出しへ切り替わる呼出し記述子ではありません。
通常のautomaticフレームにも所有者が検証する世代を持たせ、同じ番号の再利用で期限切れのviewが復活するのを防ぎます。
scoped参照は既存のresolver検査を保ちます。viewは所有権や新しい書込み権限を与えません。
const-refへの書込み、期限切れの読み書きは失敗し、同時呼出しは独立し、一時的な親・ビットを全体の名前検索へ公開しません。

## 対応範囲と確認

[独自ソースの行列](../measurements/2026-11-06/vpi-argument-declaration-matrix.json)は**243件**です。
20種類の宣言×12文脈に、呼出しの再利用、同時呼出し、const-refの対照を加えました。
scalar／1bit packed、int/integer、2/4値、符号あり／なし、幅33/65/129/257、負の添字を含む昇順／降順、
左端・中央・右端へのアクセスを確認します。HDL側でも変更したビットと変わらない隣接ビットを確認します。
文脈はstaticの対照、automatic task/関数、入れ子、refの転送、名前付きブロック、fork、
interface/program/generate/package、classメソッドです。同じ入力が**20/243から243/243**へ改善し、
既存のテスト期待値は変更していません。新しいfront/host単体テスト5件で宣言の記録、入れ子の呼出し箇所との対応、
確保前の不正記述子拒否、従来の転送、直接渡した不正なホスト宣言を確認します。行列全体はCLI CTestの1件です。

参照用の補助検査は、明示指定したIcarusコンパイラ／ランタイムの**13.0 stable（`v13_0`）**で実行します。
240件の宣言情報と0/1の読み書きを確認し、nativeの親・値形式自動選択・X/Z・寿命の強い検査は別に保ちます。
参照側は**90/240**がこの期待値と一致し、そのうち**81件**は変更前のnativeで失敗していたものが通りました。
残る観測も記録しています。refポート／package呼出しは拒否され、1bit vectorのフラグ、intのオブジェクト型、
integer／2値のビットアクセスは異なります。これらをP1対象外や参照側のPASSへ書き換えていません。
他シミュレータのソースやヘッダは読んでいません。

[確認記録](../measurements/2026-11-06/vpi-argument-declaration-validation.json)に、構成別の結果、
実際の実行ファイルの版とハッシュ、コンパイルした8種類の変異を記録します。
すべての変異を動作テストで検出し、適用可否の自己検査を検出根拠には使っていません。
ソースを復元した後も正常なテストが通ります。

dev全件は失敗・skipなしの**12,268/12,268**、release／ASan／UBSanも関連するVPI CTestが
それぞれ**69/69**通ります。1,224件の変異アンカーもすべて適用でき、281ファイルのformat検査と
変更した7翻訳単位のclang-tidyも通ります。最初のdev全件実行では古い件数表の自己検査だけが失敗しました。
整合させた後の全件再実行は成功し、その結果からリポジトリの件数を別コミットで公表します。
外部の測定と測定日は変更していません。

任意の式・part-select、多次元packed／集約型の引数メタデータ、automatic変数の全体の名前検索、
一時的なビットのcallback、新しいnet書込み、VPI全体／cocotb対応は追加しません。
Icarusで必要な残る形はP1の作業です。188プローブの一覧、固定した外部母集団と、唯一の任意項目である
`wire real`の例外は範囲を保ち、この行列をP1の達成率には使いません。
再測定した[188プローブの記録](../measurements/2026-11-06/icarus13-vpi-argument-checkpoint.json)も、
P1評価対象187件、任意の`real_net`、必要範囲の`dense_udp_coverage`のタイムアウト1件と、
別に扱う参照側の不一致25件を保っています。既存ソースと期待値の組は変更していません。
P1は未完了です。上流への投稿は行っていません。

```sh
python3 tests/cli/run_vpi_argument_declaration_tests.py --kirimasim build/dev/src/driver/kirimasim
ctest --preset dev -R 'Vpi|cli/vpi' --output-on-failure
python3 tools/mutants.py --check
```

参照用の検査には明示した`--iverilog`と`--vvp`を渡し、`--report`の別項目`reference_matches`を確認します。
コマンドの成功はnativeが独自の期待値に一致したことを示し、参照側との完全な一致は意味しません。
別途承認されたpushの前には、全体のlocal CIを実行する必要があります。
