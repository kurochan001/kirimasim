# VPIのnet全体への書込みの実行

[English](vpi-net-write-execution.md) · 2026-11-04。

**公開mainへの統合（2026-11-04）：** main `91139559`はこの実装を含みます。[全local CIと成功したGitHubワークフロー](main-integration-2026-11-04.ja.md)が、以下の時点の検証に加わりました。日付付きの生の根拠は不変です。

[承認済み契約1〜8節](vpi-net-write-proposal.ja.md)の即時net全体の操作を、front/SIR・第1層・共有kernelへ
接続しました。最初のVPI範囲で測定した通常のnet全体への書込みの不足を解消します。VPI全体やP1全体の
完了を示すものではありません。依存と公開C ABIは不変です。別途承認した[物理origin件数の移行](vpi-net-write-expectation-migration.ja.md)は、
`src/`を含まない`1f33ba51`で適用済みです。

## 対応する操作

対象は、staticな4値整数のscalar/packed net宣言全体です。無駆動net、共有したport alias、対応する
wire/tri/wired/pull/supply種別を含みます。実引数handleと名前handleは同じ検証済みの数値の物理権限を
使います。任意の記憶域へnetフラグを付けただけでは書込みを認めず、不正SIRは実行時の記憶域割当て前に拒否します。

- `vpiNoDelay`のdepositは、物理componentに受動edgeがないpoint netに対応します。driverを変更したり
  永続rootを追加したりせず、公開値をStrong 6、Xは`[-6,6]`、Zは`[0,0]`で一時的に変更します。
- driverの確定寄与が実際に変わると、通常の解決結果が同じでも、**物理宣言全体**のdepositを解除します。
  同じ寄与や取り消した保留更新では解除しません。分布遅延・module pathでは、予約時ではなく実際の確定時に解除します。
- `vpiForceFlag`・`vpiReleaseFlag`は、HDLとビットごとの所有を共有し、登録済み受動ネットの既存規則を
  使います。後のforceが所有者を置き換え、古いHDL forceはRHSが変わっただけでは所有を取り戻しません。
  force中もdriverは更新し、releaseは現在の解決結果を公開します。force中のdepositをrelease後に持ち越しません。
- vector/network全体の値と所有を確定してから通常のcallbackを呼びます。同期的な再入で別のforceを
  入れても、callbackの元の値を保持し、置き換えたforceの所有も保ちます。公開値が同じなら値変更通知を重複させません。
- 対応するreleaseの出力formatで、解除後の現在値を返します。出力ポインタのnullも認めます。不正formatは、
  forceの解除や呼出し元の記憶域変更より前に拒否します。即時3モードは時刻記述子を無視し、読み取り専用領域の
  書込みは既存の実行時エラーを保ちます。

VPI、整数の読み取り、強度の`%v`、VCDのalias、復号したFSTは同じ公開値を観測します。物理ネット内部の
通知中は再帰的な公開を防ぎ、その段階の後で通常の値変更callbackを呼びます。状態はContextが所有し、
即時操作のために隠れたHDL process・timer・RNG所有者を追加しません。公開kernelヘッダはC++20でビルドできます。

## 根拠と結果の読み方

[検証記録](../measurements/2026-11-04/vpi-net-write-execution-validation.json)に、実装の`c21d1e96`、
ソースのハッシュ、完全なdev JUnitを結びます。件数だけの更新は`c61b55df`です。

- 不変の[実装前61入力の一覧](../measurements/2026-11-03/vpi-net-write-inventory.json)は、Icarus 13.0の
  期待値一致54件・参照差4件・期待値なしの観測3件を保ちます。元の記述済み期待値58件はnativeで全件成功しました。
  受動forceの観測2件は、既存の物理ネット契約から別の新しい期待値を追加しました。受動depositは未対応APIのままです。
- 元の参照差4件は、通常の解決結果が同じ寄与変更でのdeposit解除、同じ寄与での保持、置き換えたHDL所有、
  同じ値の重複callbackです。nativeは承認済み契約に従い、参照側の成功には置き換えません。
  先行する外部判定器のコンパイル変異3件と、以下のnative変異は区別します。
- `tools/gen_vpi_net_matrix.py`の**194入力**は、通常実行166件、期待どおりのAPI拒否24件、実行時エラー1件、
  compile段階2件、dump段階1件です。公開VCD/FSTの観測4件を加え、**198項目が成功**しました。
  拒否の検査が成功しても、拒否した機能を対応済みとは扱いません。
- 幅1/32/33/65/129/257、4値、宣言文脈、task/関数/fork、名前探索、package/classのstatic呼出し、
  alias、driver変更、force所有、callback、保留遅延、実行段階、選択・flagの拒否を検査します。
  波形は分布遅延・module pathの保留到着、aliasの同一性、隠れたdriver宣言がないことを確認します。
- 新規単体テスト14件は、物理状態、割当て失敗、Context所有、release/driver通知からの再入、型付き権限を検査します。
  不正SIRの14変種は実行時割当て前に拒否します。共有する観測アサーションは物理originを正確に2/3本、
  出力qのoriginを正確に1本とし、元のソースと公開値の期待を保持します。
- [コンパイルしたnative変異13件](../measurements/2026-11-04/vpi-net-write-native-controls.json)はすべて意味の
  アサーションで検出し、コンパイル失敗や登録表の自己検査は検出件数に含めません。最初の再入変異で検査の不足が
  分かったため、driver公開時のcallbackテストを追加しました。ソースのハッシュを復元した後、重点CTestの**80件**が
  再び成功しました。登録表は**1,191アンカー**を検査し、適用不能は0件です。全アンカーのコンパイルを意味しません。
- devの構成・全ビルド・全テストは、失敗・skipなしの**11,862/11,862**です。新しいソース行列はCLIのCTest 1件です。
  固定P1母集団、以前の外部測定、固定176プローブの比較は変更していません。

## 最終のローカル構成検査

`5dae9e23`で、devの構成・全ビルド・全テスト・件数検査に続き、
`python3 tools/local_ci.py --presets release,asan,ubsan`の全17段階が成功しました。
Releaseは**11,863/11,863**、ASan・UBSanは各**11,862/11,862**で、失敗・skipはありません。
Releaseだけの追加1件は`cli/slot_event_default`です。Release flag、禁止API、SPDX、fixtureの組、
大域状態、clang-format、全**194翻訳単位**のclang-tidyも成功しました。

新しい作業ブランチにupstreamの追跡設定がなかったため、その時点までの**18コミット**は
`origin/main`との差を`check_expected_change.py`で個別に検査し、全件成功しました。
今後の検査用に追跡設定も追加しました。これはローカルの検証で、GitHub CI・Nightlyや固定母集団の再測定ではありません。

## 残る範囲と再現

netのbit/part handle、unpacked net配列のword、ユーザー定義・realのnettype、strength形式のput、
遅延付きnet put、返すevent handle、受動depositは今回の範囲外で、公開された未対応要求はAPIエラーです。
新しいP1除外は設けません。任意のIcarus `wire real`例外は、依存を確認した拡張のケースに限る方針を保ちます。

IEEE 1800-2023本文の直接照合は**未確認**です。公式GETの入口はHTTP 418でした。2017年版の38.34・10.6.2と
既存の物理ネット規則は確認しています。この制限とStrong 6の表示選択を開示して承認を受けましたが、版の照合は
後続作業に残ります。slangへの投稿は保留のままです。

```sh
cmake --preset dev && cmake --build --preset dev
ctest --preset dev -R '^(NetWrites|Vpi|ModulePathObservation|cli/vpi_net)'
python3 tests/cli/run_vpi_net_tests.py --kirimasim build/dev/src/driver/kirimasim \
  --fst-reader build/dev/tests/tools/kirimasim_fst_dependency
python3 tools/mutants.py --check
```
