# kirimasimへの移行ガイド

[English](PORTING.md) · 日英で食い違う場合は英語版を正本とします。

Icarus Verilog・Verilator・Lyraから、ソースを使うシミュレーション手順をkirimasimへ
移すためのガイドです。ソースの基準は`5e8ab597`、確認日は**2026-10-29**です。
P1は未完です。共通のオプション名だけではCLIや言語全体の互換性を示せません。
必要なソースの形ごとに[P1対応機能](P1_SUPPORT.ja.md)を確認してください。

ビルド手順は[ユーザーマニュアル](USER_MANUAL.ja.md)を参照してください。
以下の実行例は開発用ビルドを使い、作業ディレクトリはリポジトリのルートです。
比較相手のツールは必要に応じて使い、結果を使う前に導入済みの版を記録してください。

## 既存の実行手順を洗い出す

トップ名、順序付きのソース一覧、インクルードパス、マクロ値、モジュールライブラリ、
パラメータ、timescale、実行時引数、データファイルを記録します。C++ハーネス、
VPI/DPIモジュール、cocotb、ビルドスクリプト、プロジェクト設定など、
SystemVerilogの外側で行う処理も確認してください。

各ツールの版表示と、コンパイル・実行の正確なコマンドを保存します。
kirimasimでは次から始めます。

```sh
build/dev/src/driver/kirimasim --version
build/dev/src/driver/kirimasim --help
```

フロントエンドの設定と実行時の設定を分けます。`-DCOUNTER_WIDTH=8`は前処理マクロで、
`+cycles=6`は実行中のデザインが読み取ります。どちらも暗黙にモジュールパラメータを
上書きする指定ではありません。

## コンパイルと実行の手順を移す

### Icarus Verilogから

添付したカウンタをIcarusでコンパイル・実行する手順は次のとおりです。

```sh
iverilog -g2012 -s tb \
  -Idocs/examples/user_manual/include -DCOUNTER_WIDTH=8 \
  -o counter.vvp docs/examples/user_manual/counter.sv \
  docs/examples/user_manual/tb.sv
vvp counter.vvp +cycles=6
```

kirimasimでは1回の呼び出しでコンパイルと実行を行います。

```sh
build/dev/src/driver/kirimasim --top=tb \
  -Idocs/examples/user_manual/include -DCOUNTER_WIDTH=8 \
  docs/examples/user_manual/counter.sv docs/examples/user_manual/tb.sv +cycles=6
```

トップ指定の`-s tb`を`--top=tb`へ置き換えます。コンパイル結果を指定する
`-o counter.vvp`と、別の`vvp`呼び出しは取り除きます。kirimasimはVVPファイルを
入力せず、再利用可能なコンパイル済みモデルも生成しません。フロントエンドは
IEEE 1800-2023で、`-g2012`を外しても2012の言語モードを選ぶわけではありません。
元の書式は[Icarusのオプション](https://steveicarus.github.io/iverilog/usage/command_line_flags.html)と
[VVPのオプション](https://steveicarus.github.io/iverilog/usage/vvp_flags.html)に記載されています。

`$stop`の扱いについて、非対話的な`vvp -n`に対応する指定はkirimasimの既定の
`--stop=finish`です。`vvp -N`のように`$stop`で失敗させるには`--stop=error`を使います。
これは`$stop`の方針に関する対応です。kirimasimのSIGINTには別の中断処理と終了コード130が
あります。

### Verilatorから

SystemVerilog側でクロックと刺激を生成するテストベンチでは、Verilatorでの対応する手順は
次のようになります。

```sh
verilator --binary --timing --top-module tb \
  -Idocs/examples/user_manual/include -DCOUNTER_WIDTH=8 \
  docs/examples/user_manual/counter.sv docs/examples/user_manual/tb.sv
./obj_dir/Vtb +cycles=6
```

kirimasimでは前述の呼び出しを使います。`--top-module tb`を`--top=tb`へ変え、
コンパイラ側のビルド指定と生成した実行ファイルの呼び出しを、ソースの直接実行に置き換えます。
上流の[オプション資料](https://verilator.org/guide/latest/exe_verilator.html)に、
バイナリを作る手順と指定が記載されています。

C++ラッパーを使う場合は追加の作業が必要です。kirimasimにはVerilatedモデルの
インターフェース、`eval()`ループ、ホストC++ソース引数の代替はありません。
ラッパーが行うリセット、クロック、刺激、検査を洗い出し、対応できる処理を
SystemVerilogのテストベンチで表現してください。VPI/DPI-Cとcocotbの連携は未対応です。
洗い出す対象のインターフェースは、上流の
[モデル接続の説明](https://verilator.org/guide/latest/connecting.html)を参照してください。

### Lyraから

2026-10-29に直接確認した
[`6069311ba779`の公式README](https://github.com/hankhsu1996/lyra/blob/6069311ba779a8d90a8f970d3b46c6ba13a05b30/README.md)では、
ソースを実行する形式を次のように説明しています。

```text
lyra run --top top design.sv
```

kirimasim側のコマンドの形は次のとおりです。

```text
kirimasim --top=top design.sv
```

これらは書式の説明であり、Lyraで検証した実行例ではありません。READMEは、
フロントエンドの引数をslangへ渡すことを説明しています。受理する形式は導入済みの
`lyra --help`で確認してください。検証環境にはLyraを導入していないため、実行ファイルの版、
このコマンドの実行、フロントエンドの全オプションの対応は**未確認**です。
導入済みの版で、同じ独自の小さな設計を動かして確かめてください。`lyra.toml`の設定は
ソースとオプションへ明示的に展開します。kirimasimにはLyraのプロジェクトを読む機能や、
`run`・`check`・`build`・`emit cpp`・`dump`サブコマンドはありません。
`--dump-sir`はkirimasim独自の診断用表現です。

## 入力指定を明示的に移す

- **インクルード：** `-I<dir>`・`-I <dir>`、または既存の`+incdir+<dir>`を使います。
  kirimasimはincludeを書くファイル自身のディレクトリ、その後でユーザーの指定順に探索します。
  同名ヘッダがある場合は順序を保ってください。
- **マクロ：** 共有するコマンドでは`-DNAME=value`のように値を明記します。
  kirimasimの`-DNAME`は`1`、`-DNAME=`は空です。同名の事前定義は固定版フロントエンドが
  最初の指定を保持します。他ツールの既定値や重複定義の動作が同じとは仮定しません。
- **モジュールライブラリ：** `-y<dir>`・`-y <dir>`・`--libdir=<dir>`を使います。
  `+libext+.custom`は`.v`・`.sv`を残したまま拡張子を追加します。kirimasimでは、
  ライブラリパスとインクルードパスは別の役割です。Verilatorの資料では`-y`はincludeも
  探すため、その動作に依存したコマンドを移す場合は`-I`も明示してください。
- **ファイルリスト：** `-f <file>`にフロントエンド向け引数を書きます。
  kirimasimはプロセスの作業ディレクトリを基準にパスを解決します。driverは`-F`を直接受理
  しません。リスト自身の配置先を基準にしたパスは、選んだ作業ディレクトリに合わせて直します。
  実行用オプションと実行時プラス引数は、リストの外の実際のコマンドラインに置きます。
  シェルコマンドはリストに書きません。
- **トップとパラメータ：** `--top=<name>`を明示します。Icarusの`-P`やVerilatorの`-G`は
  kirimasimのdriverオプションではありません。必要なら、対応するパラメータ上書きを持つ
  ソース内ラッパーを使います。マクロはソース自身が参照している場合だけ効果があります。
- **データファイル：** 相対パスの`$fopen`・`$readmem*`は作業ディレクトリが基準です。
  インクルードやライブラリの探索先ではデータファイルの配置を変えられません。

たとえば、`counter.sv`をモジュールライブラリから読むコマンドは次のとおりです。

```sh
build/dev/src/driver/kirimasim --top=tb \
  -Idocs/examples/user_manual/include -DCOUNTER_WIDTH=8 \
  -y docs/examples/user_manual docs/examples/user_manual/tb.sv +cycles=6
```

受理する書式とkirimasimの探索規則は
[リファレンスマニュアル](REFERENCE_MANUAL.ja.md#入力オプション)を正本とします。
Verilatorの`-f`・`-F`・`-y`は、前述の上流オプション資料を参照してください。
ツール間でファイルリストのパーサ全体が等しいとは主張しません。

## フロントエンド・SIR変換・実行を確認する

問題を切り分ける間は、段階ごとに実行します。

```sh
build/dev/src/driver/kirimasim parse --top=tb \
  -f docs/examples/user_manual/files.f
build/dev/src/driver/kirimasim --dump-sir --top=tb \
  -f docs/examples/user_manual/files.f > migrated.sir
build/dev/src/driver/kirimasim --max-time=100ns --max-wall-time=5 \
  --stats=json --wave=migrated.vcd --top=tb \
  -f docs/examples/user_manual/files.f > migrated.out 2> migrated.err
```

`parse`はフロントエンドの検査で、すべての構文を変換・実行できる保証ではありません。
SIRダンプは入力モデルとして再実行できません。最初の位置付きエラー、出力ファイル、
実際の終了コードを確認してください。このファイルリストの例が成功すると、既定の
`width=4 cycles=4`を出力し、VCDを作り、理由`finish`で終了します。

`--max-time`はシミュレーション時刻、`--max-wall-time`はコンパイルを除く実行中の
経過秒数を安全な地点で制限します。上限は終了の方針であり、テスト成功の証明ではありません。
意図した`time_limit`をスクリプトの成功とする場合にだけ`--max-time-ok`を使ってください。

## デザインの動作を比較する

幅8・6サイクルのカウンタのコマンドでは、Icarus・Verilator・kirimasimで次の
デザイン出力を確認します。

```text
width=8 cycles=6
t=20 ns q=1
t=30 ns q=2
t=40 ns q=3
t=50 ns q=4
t=60 ns q=5
t=70 ns q=6
```

ツールの起動表示や終了報告はこれらのデザイン出力とは別で、出力先と文面が異なる場合が
あります。kirimasimは通常、起動表示と診断をstderrへ出します。`--stats=json`ではstdoutの
最終行がJSON objectになるため、stdout全体を1つのJSON文書として読まないでください。

不一致をシミュレータの不具合と判断する前に、時間単位・精度、リセット・初期化、4値の想定、
実行順序とレースの前提、乱数シード、入力ファイルを合わせます。kirimasimは既定で4値を
保つため、相手の正確な版と指定での方針も確認します。同じ数値のシードだけでは、
ツール間で同じ乱数列は保証できません。波形の階層名、識別子、途中の遷移も、
バイト単位で同じとは限りません。

自己検査の失敗には`$fatal`を使います。kirimasimの`$error`だけでは終了コードを非0に
しません。`$finish`・`$stop`の引数は報告レベルであり、OSの終了コードではありません。
詳細は[終了の契約](REFERENCE_MANUAL.ja.md#終了と終了コード)を参照してください。

## 互換性の根拠の範囲を保つ

添付カウンタの手順は、kirimasimのソース`5e8ab597`、Icarus
`14.0 (devel) (s20260301-411-gca6b8d376)`、Verilator
`5.052 2026-09-05 rev vUNKNOWN-built20260905`で確認しました。この根拠は記載したコマンドの
デザイン出力の範囲で、全般的な互換性やIcarus 13.0との同等性を示すものではありません。
外部資料のリンクは2026-10-29に確認しました。更新されるページは、導入済みの実行ファイルと
別の版を説明している場合があります。

Lyraの実行は前述のとおり未確認です。より広い言語の範囲は[P1対応機能](P1_SUPPORT.ja.md)と
日付付きの[Icarusとの比較](notes/icarus-13-comparison.ja.md)で確認してください。
クラスオブジェクト、より広いclocking、並行アサーション、coverage、VPI/DPI-C、FST出力は
未対応または未完です。共通の入力指定では、これらの機能は有効になりません。
新しい互換性の観測を報告する場合は、版、独自の入力、指定、stdout、stderr、終了コードを
記録してください。
