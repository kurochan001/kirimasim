# kirimasim ユーザーマニュアル

[English](USER_MANUAL.md) · 英語版と日本語版が食い違う場合は、英語版を正本とします。

kirimasimのビルド、SystemVerilogデザインのシミュレーション、波形の取得、
実行結果の確認方法を説明します。最初のシミュレーション例は2026-10-29に、
コミット`f86b940f9f6f`のバージョン`0.0.0`で確認しました。その後に加わったオプションは、
リポジトリのCLI回帰で検査しています。結果の再現に使うビルドの`--version`出力を
保存してください。

オプション、システムタスクの分類、終了コードから調べる場合は、
[リファレンスマニュアル](REFERENCE_MANUAL.ja.md)を参照してください。
このユーザーマニュアルは、準備と順を追った実行手順を説明します。

既存シミュレータの実行手順を移す場合は[移行ガイド](PORTING.ja.md)を参照してください。
ほかの資料は[ドキュメント案内](README.ja.md)から目的別に探せます。

kirimasimは開発初期段階にあります。第1実行層（Tier 1）を実装するP1は進行中で、
実プロジェクトで使用できる段階ではありません。現在はSIRインタプリタと共通の
シミュレーションカーネルで実行します。C++コード生成や、コンパイル済みモデルの
再利用にはまだ対応していません。

## 目次

- [ビルドとバージョンの確認](#ビルドとバージョンの確認)
- [最初のシミュレーション](#最初のシミュレーション)
- [入力と実行モードの選択](#入力と実行モードの選択)
- [マクロと実行時引数の指定](#マクロと実行時引数の指定)
- [シミュレーション時刻の制御](#シミュレーション時刻の制御)
- [実行とリソースの上限](#実行とリソースの上限)
- [VCD波形の記録](#vcd波形の記録)
- [出力と統計の取得](#出力と統計の取得)
- [システムタスクとデータファイル](#システムタスクとデータファイル)
- [終了コードの読み方](#終了コードの読み方)
- [トラブルシューティング](#トラブルシューティング)
- [関連文書と問題の報告](#関連文書と問題の報告)

## ビルドとバージョンの確認

以下のコマンドは、LinuxまたはmacOSのPOSIXシェルで実行してください。必要なものは
次のとおりです。

- Gitと、初回のソースコード・依存ライブラリの取得に必要なネットワーク接続。
- CMake 3.28以上、Ninja、Python 3.10以上。
- 対応するC++23コンパイラ。GCC 13以上、Clang 18以上とlibc++ 18以上の組、
  Clang 19以上とlibstdc++ 13以上の組、またはApple clang 17以上とXcode 26です。
  Clang 17・18とlibstdc++の組み合わせでは、固定バージョンのslangをC++23として
  ビルドできません。
- zlibとbzip2の開発用ヘッダ・ライブラリ。Debian/Ubuntuのパッケージ名は
  `zlib1g-dev`と`libbz2-dev`です。macOSではCommand Line ToolsのSDKを使います。

```sh
git clone https://github.com/kurochan001/kirimasim.git
cd kirimasim
cmake --preset release
cmake --build --preset release
ctest --preset release --parallel 4
export KIRIMASIM="$PWD/build/release/src/driver/kirimasim"
"$KIRIMASIM" --version
"$KIRIMASIM" --help
```

チェックアウトがすでにある場合は、そのルートディレクトリで
`cmake --preset release`から実行してください。`KIRIMASIM`は、このマニュアルで使う
シェル変数です。kirimasimが設定として読み取る環境変数ではありません。実行ファイルを
絶対パスで保持するので、作業ディレクトリを変えても同じ変数で起動できます。

初回のCMake構成時に、バージョンを固定したslang、GoogleTest、スタンドアロンの
libfstを取得します。libfstのビルド設定は用意されていますが、FST波形出力は未実装です。
ビルドの詳細は[FST依存設定](notes/fst-dependency-setup.ja.md)を参照してください。

通常の実行には`release`、デバッグには`dev`、サニタイザによる検査には`asan`・`ubsan`を
使えます。それぞれのビルド先は`build/<プリセット名>/`、実行ファイルは
`build/<プリセット名>/src/driver/kirimasim`です。別のプリセットを使う場合は、
そのプリセットでCMakeの構成とビルドを行い、`KIRIMASIM`を更新してください。
開発者向けの追加チェックは
[CONTRIBUTING](../CONTRIBUTING.ja.md)で説明しています。

以降の例は、リポジトリのルートを作業ディレクトリとして実行します。

## 最初のシミュレーション

リポジトリには、次のファイルからなる実行例を用意しています。

- [counter.sv](examples/user_manual/counter.sv)：幅をパラメータで指定できるカウンタ。
  同期リセットを持ち、クロックの立ち上がりエッジでノンブロッキング代入を行います。
- [tb.sv](examples/user_manual/tb.sv)：周期10 nsのクロックを生成し、12 nsでリセットを
  デアサートするテストベンチ。サンプリング回数を指定できます。
- [settings.svh](examples/user_manual/include/settings.svh)：幅の既定値。
- [files.f](examples/user_manual/files.f)：インクルード先とソースファイルの一覧。

```sh
"$KIRIMASIM" --top=tb -f docs/examples/user_manual/files.f
```

テストベンチは、標準出力に次の結果を表示します。

```text
width=4 cycles=4
t=20 ns q=1
t=30 ns q=2
t=40 ns q=3
t=50 ns q=4
```

バージョン、乱数シード、終了理由などは標準エラー出力に表示されます。終了コードは0です。
テストベンチは、立ち上がりエッジでのノンブロッキング代入による更新が完了した後、
立ち下がりエッジで値をサンプリングして表示し、最後に`$finish`を呼びます。
明示的に終了しない限り、フリーランニングクロックがイベントをスケジュールし続けます。

サンプリング回数を増やす場合は、次のように実行します。

```sh
"$KIRIMASIM" --top=tb -f docs/examples/user_manual/files.f +cycles=6
```

最後の表示は`t=70 ns q=6`になります。サンプリング回数に0以下を指定すると、
サンプル内の`$fatal`により終了コード1で終了します。

## 入力と実行モードの選択

コマンドの書式は次のとおりです。

```text
kirimasim [options] <file>... [+plusarg]...
kirimasim parse [options] <file>...
kirimasim --version
kirimasim --help
```

`parse`を付けなければ、起動ごとにソースの読み込み、階層のエラボレーション、SIRへの
変換、シミュレーションを行います。現在の実行層には、ビルドと実行を分けるキャッシュや、
保存して再利用できる実行モデルはありません。

### 構文解析とエラボレーションの確認

入力オプションより前に`parse`を置きます。

```sh
"$KIRIMASIM" parse --top=tb -f docs/examples/user_manual/files.f
```

プリプロセス、構文解析、型チェック、エラボレーションを行い、シミュレーションは実行しません。
成功しても、SIRへの変換や実行が可能とは限りません。slangが受理する構文の中には、
kirimasimがまだ実装していないものがあります。

### SIRへの変換と内容の確認

```sh
"$KIRIMASIM" --dump-sir --top=tb -f docs/examples/user_manual/files.f > counter.sir
```

SIRへの変換（ローワリング）と検証も行い、シミュレーションの代わりにSIRを出力します。
SIRのformat 0は開発中の診断用表現であり、安定した交換形式としては使えません。
出力したファイルを実行ファイルに渡して再実行する機能もありません。

### トップ階層の指定

`--top=tb`と`--top tb`は、エラボレーション対象のトップ階層を選択します。
複数のトップを指定する場合は、オプションを繰り返します。指定しなければ、
入力ソースにコンフィギュレーション（config）が
1つだけある場合はそれを選びます。複数のconfigがあれば、明示的な選択が必要です。
configがなければ、ほかのモジュールからインスタンス化されていないモジュールを
slangがトップモジュールに選択します。

configを明示的に選ぶ場合は、`--top=cfg:config`と指定できます。同名のモジュールが
ある場合もこの書式を使います。configの選択やソースライブラリのバインディングには追加の制約が
あります。[CLI仕様](spec/CLI.ja.md)を参照してください。

### ファイルリストとインクルードパス

添付の`files.f`の内容は次のとおりです。

```text
+incdir+docs/examples/user_manual/include
docs/examples/user_manual/counter.sv
docs/examples/user_manual/tb.sv
```

`-f <ファイル>`で、ファイルに書いたフロントエンド向けの引数を読み込みます。
ファイルリスト内の相対パスは、リストファイルの配置先ではなく、プロセスの作業
ディレクトリを基準に解釈されます。kirimasimの実行オプションと実行時のプラス引数は、コマンドラインに
指定してください。

同じ入力は、直接指定することもできます。

```sh
"$KIRIMASIM" --top=tb -Idocs/examples/user_manual/include \
  docs/examples/user_manual/counter.sv docs/examples/user_manual/tb.sv
```

`-I<dir>`・`-I <dir>`でインクルードパスを追加できます。`files.f`内の既存形式
`+incdir+<dir>`も同じ用途に使えます。混在した複数のパスも指定順を保ち、includeを書く
ファイル自身のディレクトリを先に探索します。空白を含むパスは`-I "include dir"`のように
引用符で囲んでください。共通形式の`-I`・`-D`・`-y`は`dd2c0188`から使えます。
古いビルドでは`--version`と`--help`を確認してください。

### ライブラリモジュールの探索

ライブラリモジュールには`-y<dir>`・`-y <dir>`・`--libdir=<dir>`を使います。
slangのライブラリ解決と、設定した`+libext+<拡張子>`で未解決の定義を探索します。
これらはインクルードパスを追加せず、`-I`もライブラリパスは追加しません。

`counter.sv`を直接列挙する代わりに、ライブラリから探索できます。

```sh
"$KIRIMASIM" --top=tb -Idocs/examples/user_manual/include \
  -DCOUNTER_WIDTH=8 -y docs/examples/user_manual \
  docs/examples/user_manual/tb.sv +cycles=6
```

明示するソースは`tb.sv`だけです。未解決の`counter`インスタンスに対して、ライブラリの
`counter.sv`を読み込みます。`width=8 cycles=6`を表示し、最後は`t=70 ns q=6`になります。
ライブラリ指定は`-ydocs/examples/user_manual`または`--libdir=docs/examples/user_manual`と
書くこともできます。これらの入力形式は`parse`でも使えます。

言語規格はIEEE 1800-2023に固定されており、切り替えるオプションはありません。
kirimasimのコマンドラインが受け付けるフロントエンドのオプションは限られており、
単体のslangの全オプションを使えるわけではありません。`-W...`・`--compat`・
エラボレーションの上限指定を直接渡す機能は、現在のdriverにはありません。

## マクロと実行時引数の指定

`-DNAME`・`-DNAME=value`・`-D NAME`・`-D NAME=value`で前処理マクロを定義します。
既存の`+define+NAME`・`+define+NAME=value`形式も使えます。値に空白、引用符、
ワイルドカードがある場合は、シェルに渡す引数全体を引用符で囲んでください。

値の省略時は`1`、`-DNAME=`のように空の値を明示した場合は空に展開します。
値に含む`+`や2個目以降の`=`も保ちます。固定版フロントエンドは、両形式を混ぜた場合も
同名マクロの最初の事前定義を保持します。他のシミュレータと共有するコマンドでは、
`-DNAME=1`のように値を明記してください。値なしの`-DNAME`の既定値はツールで異なります。

```sh
"$KIRIMASIM" --top=tb -DCOUNTER_WIDTH=8 \
  -f docs/examples/user_manual/files.f +cycles=6
```

サンプルの表示の先頭は`width=8 cycles=6`になります。ヘッダは、`COUNTER_WIDTH`が
未定義の場合だけ既定値を定義します。

同じマクロを既存のプラス形式で指定することもできます。

```sh
"$KIRIMASIM" --top=tb '+define+COUNTER_WIDTH=8' \
  -f docs/examples/user_manual/files.f +cycles=6
```

`+`で始まるほかの引数は、フロントエンド用に予約した`+incdir+`、`+define+`、
`+libext+`を除いて実行時のプラス引数（plusargs）になります。設計内で`$test$plusargs`や
`$value$plusargs`を使って読みます。サンプルでは、次のように使っています。

```systemverilog
if (!$value$plusargs("cycles=%d", cycles)) cycles = 4;
```

プラス引数はコマンドラインの順序で渡されます。マクロの定義やモジュールの
パラメータの上書きにはなりません。設計側で読み取り、使う必要があります。

`--seed=<n>`は、`$urandom`、`$urandom_range`、`randcase`で使う既定の乱数シードを
指定します。既定値は0で、引数は符号なし64ビットの10進整数です。設計内で明示するシードや、
従来の`$random`系には別の規則があります。[システムタスク仕様](spec/SYSTEM_TASKS.ja.md)を
参照してください。

```sh
"$KIRIMASIM" --seed=42 --top=tb -f docs/examples/user_manual/files.f
```

このカウンタの例は乱数を使わないので、表示する値は変わりません。

## シミュレーション時刻の制御

設計内で`timeunit`と`timeprecision`、または`` `timescale``を指定します。
`--timescale=<単位>/<精度>`は、独自のtimescaleを持たない設計要素の既定値で、
指定しなければ`1ns/1ns`です。サンプルは単位`1ns`、精度`1ps`を明示しています。
`$timeformat`は時刻の表示を変えるもので、スケジューリングの精度は変えません。

```sh
"$KIRIMASIM" --timescale=1ns/1ps --top=tb -f docs/examples/user_manual/files.f
```

`--mintypmax=min|typ|max`は、遅延式などの`min:typ:max`で使う式を選びます。
既定値は`typ`です。

### シミュレーション時刻で終了する

```sh
"$KIRIMASIM" --max-time=25ns --top=tb -f docs/examples/user_manual/files.f
```

設計は先頭行と20 nsの値を表示し、理由`time_limit`、終了コード4で終了します。
上限によって終了する場合は、境界の時刻までの全イベントを処理し、`final`を実行します。
それより前に設計が終了するか、イベントが尽きれば、上限より早く終了することもあります。

値は、0以上の10進数の直後に`s`、`ms`、`us`、`ns`、`ps`、`fs`のどれかを付けます。
例えば`10us`や`1.5ns`です。設計の時間精度へ最も近い値に丸め、ちょうど半分は0から
遠い側へ丸めます。

このシミュレーション時刻の上限で終了したときに終了コード0を返すには、
`--max-time-ok`を追加します。

```sh
"$KIRIMASIM" --max-time=25ns --max-time-ok --top=tb \
  -f docs/examples/user_manual/files.f
```

### 実時間によるタイムアウト

`--max-wall-time=<秒数>`は、コンパイルを除いたシミュレーション実行の実時間を
制限します。整数秒で指定し、既定値の0は無制限です。到達すると、理由
`wall_time_limit`、終了コード4になります。`--max-time-ok`を付けても変わりません。

シミュレーションのセーフポイントで確認するため、ブロック中のホスト側処理を
強制的に中断するものではありません。`final`プロシージャには実時間の上限を
適用せず、完了まで実行します。

## 実行とリソースの上限

実行の上限によって、長いシミュレーションと時刻の進まないループを切り分けられます。
回数は符号なしの10進整数で指定します。

- `--max-zero-delay=<n>`：1つのタイムスロットで、イベントをActiveまたはReactiveへ
  移動する回数。既定値は**1,000,000**です。
- `--max-slot-events=<n>`：1つのタイムスロットで、全リージョンを通して実行するイベント数。
  既定値は**100,000,000**です。上記のリージョンへの移動を伴わず、プロセス同士が
  互いに起床させ続ける場合も検出します。
- `--max-steps=<n>`：1つのプロセスが、待機せず連続して実行する第1層の命令数。
  既定値は**1,000,000,000**です。遅延やイベント待ちを含まないループを検出します。
- `--max-procs=<n>`：実行時に作った、まだ終了していない子プロセスの数。
  既定値は**1,000,000**です。エラボレーションで生成した静的プロセスと、終了後も
  保持している記録は含めません。
- `--max-call-depth=<n>`：1つのプロセスで入れ子になったタスク・関数呼び出しの深さ。
  既定値は**10,000**です。実行時には、シミュレーション用スレッドのスタック残量も
  検査します。

先頭の3種類は終了コード5、プロセス数・呼び出し深さ・スタックの上限は終了コード6で
終了します。回数の上限は0で無効にできますが、`--max-call-depth`だけは正の値が必要です。
`--max-scc-iter=<n>`は受け付けますが、第2実行層（Tier 2）の静的アイランドを実装するまでは効果がありません。

例えば、開発中に小さめの上限で実行するには、次のように指定します。

```sh
"$KIRIMASIM" --max-steps=1000000 --max-slot-events=100000 \
  --max-procs=10000 --max-call-depth=1000 --top=tb \
  -f docs/examples/user_manual/files.f
```

通常は既定値で十分です。上限に達した場合は、値を増やす前に、表示された上限と回数、
問題を再現する設計を調べてください。`--max-steps`の診断にはプロセス名も含まれます。

### 停止の方針と範囲外アクセスの警告

- `--stop=finish`：`$stop`でシミュレーションを終了し、終了コード0を返します。既定値です。
- `--stop=ignore`：`$stop`から戻り、呼び出したプロセスを続けます。
- `--stop=error`：`$stop`で終了コード1を返します。終了理由は`stop`のままです。
- `--bounds-check=warn`：実行時の不正なpacked選択や固定長配列の添字を、ソース位置ごとに
  1回警告し、終了時にアクセス回数を表示します。既定値は`--bounds-check=off`です。
  警告を有効にしても、アクセス結果の値は変わらず、それだけで実行失敗にもなりません。

```sh
"$KIRIMASIM" --bounds-check=warn --stop=error --top=tb \
  -f docs/examples/user_manual/files.f
```

この範囲検査は、あらゆるコンテナ操作を検査するものではありません。検査する形と例は
[P1の機能例](P1_FEATURES.ja.md)で説明しています。

### 除去されたパルスを報告する

`--report-pulses`は連続代入・ゲート・UDP・遅延付き強度／スイッチモデルで、
慣性遅延が取り消した変化を報告します。取消しごとに、ソース位置と正確な時刻を
標準エラーへ注記します。値・イベントのスケジューリング・標準出力・カウンタは
保ちます。既定はoffで、同じ位置の繰り返し取消しも毎回報告します。同じ保留値、
transport NBA、シミュレーション終了はパルス注記を出しません。
[実行例](P1_FEATURES.ja.md#除去されたパルスの報告)も参照してください。

## VCD波形の記録

```sh
"$KIRIMASIM" --wave=counter.vcd --top=tb -f docs/examples/user_manual/files.f
"$KIRIMASIM" --wave=counter-dut.vcd '--wave-scope=tb.dut' \
  --top=tb -f docs/examples/user_manual/files.f
```

出力したVCDは、VCDを読める波形ビューアで開いてください。出力先のパスは作業
ディレクトリを基準とします。同名のファイルがある場合は、ダンプ開始時に上書きします。

`--wave-scope=<パターン>[,<パターン>...]`で、スコープとその子孫を選びます。
パターンはスコープの階層パス名に照合し、`*`は任意の文字列、`?`は1文字に一致します。
親スコープが一致した場合も、その子孫を選びます。シェルによる展開を避けるため、
パターンを引用符で囲んでください。

```sh
"$KIRIMASIM" --wave=counter-filtered.vcd '--wave-scope=tb.dut*' \
  --top=tb -f docs/examples/user_manual/files.f
```

既定では、各タイムスロットの終了時点の値を記録します。`--wave-glitches`を
付けると、スロット途中の変化も記録します。

コマンドラインの`--wave`の代わりに、次のブロックをサンプルの`tb`モジュールへ追加して
実行することもできます。

```systemverilog
initial begin
  $dumpfile("counter-from-design.vcd");
  $dumpvars(0, tb);
end
```

`$dumpvars(0, tb)`は、指定スコープとその下位階層をすべてダンプ対象にします。制御用の`$dumpoff`、
`$dumpon`、`$dumpall`、`$dumplimit`、`$dumpflush`も使えます。`--wave`を指定すると、
ファイル名と記録対象はコマンドラインが決めるので、設計側の`$dumpfile`と`$dumpvars`は
警告して無視します。ほかの制御タスクは引き続き働きます。`--wave`がなくても、
`--wave-scope`と`--wave-glitches`は、設計側が開始したダンプに適用されます。

現在のVCD出力は、対応する静的なネット・変数・スコープを記録します。
unpacked配列、automatic変数、文字列やキューなどの動的データは記録しません。
program・interfaceのスコープには制限が残ります。FST出力と`$dumpports`は未実装です。
ファイルを開けない場合は警告してシミュレーションを続けるため、ファイルが作られたかも
確認してください。詳細は[波形仕様](spec/WAVEFORM.ja.md)にあります。

## 出力と統計の取得

通常のシミュレーションでは、出力を2つのストリームに分けます。

- **標準出力**：`$display`、`$write`、`$strobe`、`$monitor`など、標準出力を宛先にした
  設計の出力。
- **標準エラー出力**：kirimasimのバージョン・シード・引数の行、診断、`$error`・`$warning`などの報告、
  終了メッセージ。

設計の出力を比較する場合は、分けて保存します。

```sh
"$KIRIMASIM" --top=tb -f docs/examples/user_manual/files.f \
  > design.out 2> diagnostics.log
```

`--autoflush`は、display・write・strobe・monitor系タスクの実行ごとに標準出力を
フラッシュします。標準出力を宛先にしたファイル出力タスクも対象です。パイプの相手や
対話的な読み手が、実行途中の出力をすぐに受け取る必要がある場合に使います。

```sh
"$KIRIMASIM" --autoflush --top=tb -f docs/examples/user_manual/files.f
```

### 回数カウンタの取得

```sh
"$KIRIMASIM" --stats=json --top=tb -f docs/examples/user_manual/files.f \
  > stats.out 2> diagnostics.log
tail -n 1 stats.out
```

`--stats=json`は、標準出力の最後の行にJSONオブジェクトを1つ追加します。
前には設計の出力があるので、ファイル全体がJSON文書になるわけではありません。
主なキーは`sim.end_reason`、`sim.end_time`、`sched.time_slots`、`sched.events.active`、
`nba.scheduled`、`vm.instrs`です。`sim.end_time`の単位は設計全体の時間精度のtickで、
必ずしもナノ秒ではありません。これらは処理の回数で、実時間やメモリ使用量ではありません。

`parse`や`--dump-sir`でもカウンタを出せます。その場合は、そのモードの処理を表し、
シミュレーションの統計ではありません。[性能カウンタ仕様](spec/PERFORMANCE.ja.md)を
参照してください。

## システムタスクとデータファイル

表示・書式化、ファイル入出力、メモリファイルの読み込み、時刻・型の照会、乱数、
実行制御などのタスクを実装しています。対応する形と引数の型は
[P1対応機能](P1_SUPPORT.ja.md)にあります。ソースからの`%p`と、解決した強度の`%v`表示は
未対応です。

メモリファイルを使う簡単な例として、作業ディレクトリに`memory_demo.sv`を作成します。

```systemverilog
module memory_demo;
  logic [7:0] memory [0:3];
  initial begin
    $readmemh("memory.hex", memory);
    $display("first=%h last=%h", memory[0], memory[3]);
  end
endmodule
```

同じディレクトリに`memory.hex`を作ります。

```text
10 20 ab ff
```

```sh
"$KIRIMASIM" --top=memory_demo memory_demo.sv
```

出力は`first=10 last=ff`です。将来のイベントがないので、理由`quiescent`で正常終了します。
クロックが動き続けるテストベンチでは、通常は`$finish`で明示的に終了させます。

`$readmemh`、`$fopen`、`$dumpfile`などに渡す相対ファイル名の基準は、プロセスの作業
ディレクトリです。ファイルタスクでは、記述子や戻り値も確認してください。
固定長配列、動的配列、キューは使える操作が異なります。コードを移す場合は、
[コンテナの例](P1_FEATURES.ja.md#動的配列とキュー)も参照してください。

## 終了コードの読み方

ビルドやテストのスクリプトでは、実行ファイルの終了ステータスを取得してください。

- **0**：通常の`$finish`、イベントが尽きた場合の`quiescent`、既定の`$stop`。
  `--max-time-ok`付きのシミュレーション時刻の上限でも0です。
- **1**：`$fatal`、または`--stop=error`を指定したときの`$stop`。
- **2**：不正なコマンドラインや入力、未対応の構文、実行時の設計の誤り。
- **3**：SIR検証器の失敗など、kirimasimの内部エラー。
- **4**：シミュレーション時刻または実時間の上限。シミュレーション時刻には、上記の
  例外があります。
- **5**：時刻が進まない実行の上限。
- **6**：実行時プロセス数、呼び出し深さ、シミュレーション用スタックのリソース上限。
- **130**：SIGINT。通常はCtrl-Cです。

自動実行を確実に失敗させたい自己検査には、例えば
`if (actual !== expected) $fatal(1, "mismatch");`のように`$fatal`を使います。
現在の実行ファイルでは、`$error`は診断を出しますが、それだけで終了ステータスを
失敗にはしません。`$finish`と`$stop`の数値引数は報告の詳しさであり、OSの終了コードでは
ありません。`$finish(2)`は統計の表示も要求します。

シミュレーション中の最初のCtrl-Cは、安全な検査点での停止と`final`の実行を要求します。
2回目は直ちにプロセスを終了させます。コンパイル中のSIGINTは、OSの通常の動作に従います。

## トラブルシューティング

段階ごとの最小化、SIRの確認、診断の実行例は[不具合調査ガイド](DEBUGGING.ja.md)、
再実行する範囲とテスト結果の読み方は[検証・再現ガイド](VALIDATION.ja.md)を参照してください。

### ヘッダやデータファイルが見つからない

作業ディレクトリと、`-f`の中のパスを確認してください。インクルード先は
`-I<dir>`・`-I <dir>`・`+incdir+<dir>`で追加します。インクルードの検索先を指定しても、実行時のファイルタスクが
データを探す場所は変わりません。

### モジュールが見つからない

ソースを直接列挙するか、`-y <dir>`・`--libdir=<dir>`を使い、モジュール`counter`に対する
`counter.sv`のように名前が一致するファイルを置いてください。別の拡張子が必要なら
`+libext+<拡張子>`を指定します。`-I`のディレクトリはincludeの探索先であり、ライブラリの
ディレクトリはヘッダを探しません。[ライブラリの実行例](#ライブラリモジュールの探索)を参照してください。

### parseは成功するが実行が終了コード2になる

`parse`が動かすのはslangのフロントエンドまでです。`--dump-sir`で変換を確認し、
最初の位置付き診断を調べてください。合法なSystemVerilogの形でも、kirimasimでは
未対応の場合があります。再現例を小さくし、[P1対応機能](P1_SUPPORT.ja.md)で、その構文や
記憶域の型の制限を確認してください。

### 信号がXやZになる

kirimasimは4値を保持します。初期値のない通常の4値整数変数はXで始まります。
駆動のない通常のwireネットは、ほかのドライバやネット種別が値を決めなければZになります。
リセットとドライバを確認し、`%b`や`%h`で値を表示してください。
`--init-values=zero`・`one`・`random`は、初期値のない4値整数変数と新規メモリ要素の
既定値を変えます。明示初期値、ネット、2値変数、不正な読み出しの既定値は保ちます。
`random`は`--seed`を使い、設計の`$random`・`$urandom`の系列を消費しません。
X以外を選ぶと版の行の次に注記を出します。既定へ戻す指定は`--init-values=x`です。
[実行例](P1_FEATURES.ja.md#ソース変数の初期値)も参照してください。

`--cond-x=warn`は、`if`・`?:`の条件と`case`の選択式に含むX/Zを検出します。
ソース位置ごとに1回警告し、最後にX/Zを検出した評価回数を出します。既知の1により真となる
`2'b1x`も対象ですが、値や分岐結果は変えません。既定は`--cond-x=off`です。
`--cond-x=error`は設計の実行時エラーとして終了し、`final`を実行します。
`final`内の診断は既存の終了方針に従い、`final`開始前の終了理由を保ちます。

### 終わらない、または上限に達する

待機のないループ、`#0`の繰り返し、互いを起こすプロセス、終了条件のないクロックを
調べてください。終了コード5では、表示された上限と回数を確認します。
`--max-steps`の診断にはプロセス名も含まれます。
終了コード6では、再帰や実行時のfork生成を確認します。調査中は、上記の時刻・リソースの
オプションで実行を制限できます。

### 波形に信号や変化が出ない

ダンプファイルの警告、スコープのパターン、信号の記憶域・型、現在のVCDの制限を
確認してください。同じタイムスロットの途中の変化は、`--wave-glitches`で記録できます。
シミュレーションの終了コードが0でも、ファイルを開けたとは限りません。

### 仕様書にあるオプションが拒否される

仕様書は予定の動作も含みます。`--log`、`--diag-format`、`--tier`、`--state`などは、現在のdriverには未実装です。
ログの保存には、シェルのリダイレクトを使ってください。`--help`は短い一覧で、この
マニュアルの各節ではフロントエンド向けのプラス形式も含めて、現在使える指定を説明しています。

### ほかのシミュレータと結果が違う

両シミュレータのバージョン、入力ファイル、オプション、乱数シード、標準出力、標準エラー出力、終了コードを
記録します。比較前に、競合、初期化、時間精度、未対応の形を確認してください。
ネット、スイッチなどに残る制限は、[P1対応機能](P1_SUPPORT.ja.md)にあります。
IEEEの全面的な準拠や、Icarusとの完全な機能同等性は確認できていません。

## 関連文書と問題の報告

[リファレンスマニュアル](REFERENCE_MANUAL.ja.md)に、実装済みCLI、既定値、
タスクの索引、実行の終了結果をまとめています。

2026-10-29の追加対応では、既定または明示した`#1step`と`@(cb)`を使う入力専用の
クロッキングブロック、および駆動強度を保持する静的なinput/outputの部分ポートを扱います。
正確な対応範囲は[P1対応機能](P1_SUPPORT.ja.md)を参照してください。他のスキュー、output/inoutの
クロッキング変数、より広い部分ポートの形は未対応です。この追加対応は、本書冒頭の基準コミットより後の変更です。

- [P1対応機能](P1_SUPPORT.ja.md)：実装済みの形と残る制限。
- [P1の機能例](P1_FEATURES.ja.md)：追加の言語機能の実行例。
- [CLI仕様](spec/CLI.ja.md)、[システムタスク仕様](spec/SYSTEM_TASKS.ja.md)、
  [波形仕様](spec/WAVEFORM.ja.md)：詳細な契約。予定の作業を説明する節では、実装状況も
  確認してください。
- [P1完了条件の監査](P1_CRITERIA_GAPS_2026-10-28.md)：完了までの残作業。
- [CONTRIBUTING](../CONTRIBUTING.ja.md)：Issueのフォームと参加の決まり。

問題はリポジトリのIssueフォームから報告してください。自分で書いた最小の再現例、
正確なコマンドと作業ディレクトリの構成、`--version`、期待結果と実際の結果、終了コードを
添えます。時刻や乱数に関する問題では、timescaleと乱数シードも添えてください。
非公開の設計や規格本文のコピーは含めず、自分の言葉で説明し、分かる場合は規格の節番号を
示します。現在、リポジトリはPRではなく、Issueと仕様の提案を受け付けています。
