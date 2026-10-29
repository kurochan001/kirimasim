# P1 完了条件の未達・未確認事項（2026-10-28）

対象は main `4c97244784cc107770924c6597e34cbd0186bd24`。
測定日・nightly の日付は、特記しない限り UTC。
この文書は承認済みの条件を点検するもので、仕様や合格条件を変更するものではない。

**2026-10-29追記：** 以下は10月28日の測定記録です。その後`--cond-x`と
`--init-values`を実装しました。現在の対応範囲と回帰は
[P1対応機能](P1_SUPPORT.ja.md)と[初期値の実行例](P1_FEATURES.ja.md#ソース変数の初期値)を
参照してください。これらの完了だけでP1全体の完了とは判定しません。

**2026-10-30追記：** `--report-pulses`も実装しました。慣性遅延が取り消した更新を
ソース位置と時刻付きで報告します。[実行例](P1_FEATURES.ja.md#除去されたパルスの報告)を
参照してください。以下の10月28日の測定結果は再測定していません。

## 結論

**P1 は未完了。現在、未達と確認できる主要な完了条件は、Icarus Verilog 13.0 以上の標準機能の実行範囲である。**

- 現行版で95プローブを再測定したところ、Icarus 13.0 が期待値どおり動かすのに kirimasim が成功しないものは6件。そのうち標準側は5プローブ、Icarus 拡張側は1プローブである。標準側には4件の未対応診断と1件の時間制限到達がある。
- その95プローブには VPI などの未実装機構が入っていない。したがって、上の5件だけを直せば P1 完了、とは判定できない。必要な機構と組合せを Icarus 13.0 に対応付ける棚卸しも残る。
- 既存の P1 仕様に明記された **FST出力、source `%p`、resolved-strength `%v`、`--cond-x` / `--init-values` などの動作も実装が残る**。比較対象の棚卸しが未完了であることを理由に、既に指定された P1 の機能を対象外へ戻さない。
- 固定 P1 母集団の90%条件は、現行版でも **1456 / 1466件、99.3%** で達成している。unsupported 10件は合格ではないが、90%条件の未達を意味しない。
- 「100倍ファジングを7晩連続で差分ゼロ」は、今回調べた **2026-10-21〜27の7回の nightly に成功の証跡がある**。README / `docs/counts.json` の「5晩、10月21日まで」は古い記録であり、その値を根拠に未達と扱わない。
- 通常 CI は全14ジョブ成功、ローカル CI も全項目合格している。ただし、それだけでは機能範囲の完成や M8 の最終判定を代替しない。

## 1. 正式なクライテリアと判定

正本は [TEST_SUITE](spec/TEST_SUITE.md) と [P1_SCOPE](spec/P1_SCOPE.md)。
[README の P1 exit criteria](../README.md#p1-exit-criteria) はその要約である。

「未達」は不足を実行結果・実装で確認できたもの、「未確認」は対象範囲や測定の証跡が揃っていないものとして区別する。

| 条件 | 判定 | 根拠と残る確認 |
| --- | --- | --- |
| 固定 sv-tests の simulation 合格数が、Linux / macOS のそれぞれで Icarus 以上 | 保存記録では達成。現行版の両 OS 比較は未再測定 | Linux は244対211、macOS は244対210。`docs/counts.json` の測定日はそれぞれ10月26日・27日。現行版の通常 CI には、この外部比較を再実行するステップはない |
| 固定 P1 母集団1466件の90%以上（1320件以上）が合格 | **現行版で達成** | 今回の再測定で1456合格、通常失敗0、unsupported 10。期待値は kirimasim-tests `41085fe3432b`、リストは変更していない |
| 必須テスト一覧が合格。ただし明示された除外を除く | 達成の証跡あり | 現行版の通常 CI とローカル4構成が合格。Linux TSan も合格。10月27日 nightly では4つの sanitizer ジョブの「一プロセス・shuffle・3回反復」も成功。測定版・範囲は節7に分けて記す |
| `valsem` / 参照 scheduler と100倍の試行を日付 seed で比較し、7連続 nightly で差分ゼロ | **7晩分の達成証跡あり。集計記録は未更新** | 10月21〜27日の正式 fuzz ステップのコマンド・成功結果を確認。根拠は節6。M8では判定時点の直近7回を照合する |
| Icarus Verilog 13.0 以上の標準機能の実行範囲 | **未達。完全な対応関係も未確認** | 現行版の有限比較で標準側5プローブが未成功。クラス・specify・部分ポート接続などの具体的不足と、VPIを含む未実装・未整理の範囲が残る |

判定上の注意：

- sv-tests の数値ゲートの参照は、13.0 stable ではなく、固定された development commit `04cc493506bd4d75c861cfc9961e2a56517d7cc6`。機能範囲を比較する13.0との基準を混同しない。
- 固定 sv-tests は commit `15490e95fd978ada1c1dc59b7c4b53a7edb23b78` の手書き simulation 344件。独自95プローブとも固定 P1 母集団1466件とも別である。
- 必須リストには `context_sequential`、`context_parallel`、`context_churn`、nightly の unit shuffle も含まれる。`front_parallel` は P1 の必須条件ではない。
- `cb/input_sample_1step` は必須リストの現時点の除外対象。もう1件の skip `nets/net_port_drivers_part` は接続の実装制限である。「2件 skip」をそのまま「必須テスト条件の未達」と数えない。
- 現在の通常 CI に通ったことと、外部の両 OS 比較や nightly の結果は別の証跡である。

出典：[TEST_SUITE の必須テスト・測定方法](spec/TEST_SUITE.md)、[固定母集団](../tests/gates/p1_population.txt)、[許可された skip](../tests/suite/skips.txt)、[保存集計](counts.json)。

## 2. 現行版で不成功を確認した標準側5プローブ

今回、既存の [独自比較ツール](../tools/icarus_feature_probe.py) の95件を、現行 release バイナリで実行した。
新しい参照ツールのインストールや、他シミュレータのソース閲覧は行っていない。

| プローブ | Icarus 13.0 / 14.0 | kirimasim | 残っていること・完了を確認する方法 |
| --- | --- | --- | --- |
| `class_objects` | 両方とも期待値一致 | exit 2、class型 / `NewClass` 未対応 | オブジェクト生成、インスタンス状態、メソッド実行を接続し、このプローブと独立した寿命・継承の検査を通す |
| `class_constructor` | 両方とも期待値一致 | exit 2、class型 / `NewClass` 未対応 | コンストラクタの引数、初期化順、複数インスタンスの独立性を実行できるようにする。非 parameterized static property の対応では代替できない |
| `specify_path` | `-gspecify` で両方とも期待値一致 | exit 2、path delays / timing checks を含む specify 未対応 | module path の遅延と scheduler への接続を実装・検査する。specparam や通常のゲート遅延だけの対応では不足 |
| `mixed_strength_port_bit` | 両方とも期待値一致 | exit 2、ポートを通って会うドライバが未対応 | 部分ポート接続のネット同一性と複数ドライバの強度解決を接続し、単一・複数インスタンスの組合せを検査する |
| `dense_udp_coverage` | 両方とも期待値一致 | 15秒のサブプロセス制限に到達 | dense UDP のソース処理を切り分け、正常に完了することを確認する。今回の時間制限到達だけでは、停止原因・無限ループ・規格違反の有無は未確認 |

この5件は「5つの未実装機能」という意味ではない。クラスを複数プローブで検査している一方、網羅していない機構もある。
`dense_udp_coverage` の15秒はこの比較ツールの制限であり、P1に新しい性能数値ゲートを追加したものではない。

実装側の根拠：[class の型・生成、specify の拒否](../src/front/lower.cpp)、[ポートを通って会うドライバの拒否](../src/front/lower.cpp#L1025)、[UDPの既存対応範囲](P1_SUPPORT.md)。

### 標準側の未達に混ぜないもの

- **`real_net`**：6件目の gap だが、比較ツールの分類は `icarus_extension`（`wire real`）。標準機能の不足として数えず、[P1_SCOPE](spec/P1_SCOPE.md) に従って拡張・互換性方針を別に整理する。
- **参照結果の不一致**：13.0では4件、14.0では7件。これは Icarus がプローブの規格由来の期待値と一致しない分類であり、kirimasim の不具合件数ではない。
- 13.0側の4件は `nested_packed_select_bounds`、`class_virtual_dispatch`、`dynamic_array_queries`、`blocking_repeated_event_one_bit_unknown`。このうち class dispatch は kirimasim も未対応。他の3件は kirimasim が期待値に一致した。
- 14.0側に追加される3件は `dynamic_minimum_duplicate`、`queue_maximum_duplicate`、`dynamic_minimum_with_key`。いずれも kirimasim は期待値に一致した。参照の出力に合わせて既存期待値を変更する対象ではない。

## 3. 95プローブの外に残る機構と、まだ確定していない範囲

承認済み [P1_SCOPE](spec/P1_SCOPE.md) は、必要な部分を旧 P4/P5 から P1 に前倒ししている。
ただし、以下の大分類の全構文を無条件に P1 必須とする変更は承認されていない。
**Icarus 13.0 がどの形を動かすか、標準機能か拡張か、kirimasim のどの境界が不足するかを形ごとに対応付ける作業が未完了である。**
この未確認区分は、既存仕様が P1 / M7 と明記した機能を対象外へ戻す意味ではない。

| 領域 | 現在残っている実装・仕様の境界 | P1での扱い |
| --- | --- | --- |
| VPI / 外部接続 | VPI runtime、オブジェクトと callback、値・時刻の橋渡し。`VPI_SPEC` も詳細未記述 | 必要な VPI の部分は明示的に P1 対象。比較範囲と詳細意味を確定し、必要なら仕様提案を承認してから実装する。cocotb全体の対応を別の合格条件として追加しない |
| クラス | object生成、instance property、dispatch、継承・寿命・回収、parameterized static storage | 少なくとも節2の基本生成・constructorは不足を実測済み。その他の必要範囲は比較を拡張する |
| 動的コンテナ・配列メソッド | 連想配列、queue-slice代入、追加のlocator / shuffle、混在・部分省略のdynamic foreach、nested / runtime次元照会、aggregate / class / event要素、暗黙感度、効果を持つordering key | 必要な部分を比較で特定する。基本的なdynamic array / queueの生成・コピー・ref・待機・reduction・sort・min/maxまで「未実装」と戻して数えない |
| event・プロセス制御 | scalar event return / conditional、automatic structureのevent field、rebinding、eventを含むコンテナ、event-NBAのref / managed-value境界、event-controlled `->>`、named-fork disable、`wait_order` | 個々の形の Icarus 13.0 対応と合法性が未確認。既に動く automatic capture、input event、基本的event-NBAと区別する |
| ネット・スイッチ | 残る部分ポート / alias の同一性、registered strength netのforce/release、完全な強度の書式化、assignmentのないdelayed net宣言、charge storage | 部分ポートの不足は節2で実測済み。残りは形ごとの比較が必要。6種のMOS・6種のpassive switchが全て未実装、とは扱わない |
| specify / timing | module path、timing checks、その相互作用 | module pathの不足は実測済み。必要なtimingの種類・合法性・相互作用の比較と実装が残る |
| aggregate / selection / streaming | structure ref、array-of-structures引数・automatic配列、copyback slice、位置が動くpacked parent、streaming `with` / string target / nested target | 既存の固定aggregateや広幅streaming対応を保ち、不足する形と Icarus 13.0 対応の対応付けを行う |
| UDP | 未接続ポート、dense-tableのソース処理境界 | dense-tableは節2の時間制限到達。未接続ポート等の必要範囲は別に比較する。wide evaluatorそのものは実装済み |
| FST出力 | 依存のビルドは準備済みだがbackend未実装 | **既存仕様で明示されたP1の残作業**。`WAVEFORM`はP1、`CLI`はM7と指定している。Icarus比較の未確認項目へ戻さない |
| 書式・入力・追加のdump scope | source `%p`、resolved-strength `%v`、program / interface scopeのVCD、残る `$fread` / scan / packed-formatの形 | `%p` / `%v`等の既存P1契約の実装不足と、scope・追加の形の対象範囲の未確認を分ける。基盤実装とfrontから実行できることも区別する |
| 指定済みCLI動作 | `--cond-x`、`--init-values`等 | **既存CLI仕様のM7動作が未実装**。新しい数値ゲートではないが、指定された動作の実装不足である |

根拠：[対応機能と制限](P1_SUPPORT.md)、[READMEの残るP1機能](../README.md#remaining-p1-functionality-2026-10-26)、[VPIの未記述箇所](spec/VPI_SPEC.md)、[既存P1の波形仕様](spec/WAVEFORM.md)、[CLIのM7指定](spec/CLI.md)、[書式・入出力仕様](spec/SYSTEM_TASKS.md)、[dynamic foreachの拒否境界](../src/front/lower_stmt.cpp)、[FSTの拒否](../src/driver/main.cpp)、[dump scope](../src/front/lower_dump.cpp)。

## 4. 固定 P1 母集団でまだ動かない10件

今回、**現行 main と、承認済みの期待値版 `41085fe3432b`** を使って再測定した。
1466件のリスト・期待値は変更していない。結果は1456合格、通常失敗0、unsupported 10である。

| テスト名 | 停止する形 |
| --- | --- |
| `ch16/test_comb_stable_multiinstance` | `$stable` |
| `ch16/test_sampled_function` | `$sampled` |
| `ch16/test_system_sampled` | `$sampled` |
| `ch18/test_randsequence` | `randsequence` |
| `ch25/test_comb_interface_modport` | virtual interface |
| `ch25/test_comb_vif_array` | parameterized virtual interface（最初のscalar宣言で拒否） |
| `ch25/test_interface_modport_corner` | virtual interface / modportの形 |
| `ch7/test_assoc_foreach_struct_key` | 構造体をキーに持つ連想配列 |
| `ch7/test_multi_dim_assign` | 固定配列を要素に持つ動的配列 `int dyn_of_fixed[][0:3]` |
| `ch7/test_struct_array_sort` | 固定構造体配列の `sort with` |

判定は **1456 / 1466**。unsupported を分母から取り除いた「1456 / 1456」にはしない。
この10件が残っていても90%の数値条件には合格する一方、機能範囲が全て実装された証拠にはならない。
また、この表の全項目を比較なしに Icarus 13.0 必須機能と断定しない。

ソース：[固定版の外部スイート](https://github.com/kurochan001/kirimasim-tests/tree/41085fe3432bb093bf52c4f33bc4b44fff1a9401/tests)。集計の規則：[p1_population.py](../tools/p1_population.py)。

## 5. Bug台帳と完了条件を混同しないための補足

率のゲートに通ることは、IEEEの値・スケジューリング意味を守る要件を免除しない。
一方、外部スイート全体の失敗数を、固定1466件の未合格数や P1 未達条件の数へ足してはいけない。

今回の変更を含む状態は次のとおり。

- [#66](https://github.com/kurochan001/kirimasim/issues/66)（queue method indexのinteger変換）は修正し、報告元のテストも成功した。[#70](https://github.com/kurochan001/kirimasim/issues/70)（`$timeformat` の不正引数診断）は修正し、追加した7件の回帰で検証した。#70の外部の報告元テスト自体は、この作業では実行していない。
- [#61](https://github.com/kurochan001/kirimasim/issues/61) のshortreal条件式、[#65](https://github.com/kurochan001/kirimasim/issues/65) の全添字省略foreachはランタイムを修正した。ただし、slangがエラボレーション中に決定する `localparam real` / 定数関数の値には別経路の問題が残る。Issue全体を完全解決として扱わない。
- [#69](https://github.com/kurochan001/kirimasim/issues/69) は、生成VCDには named fork scope が存在し、外部テストの読取処理が行を飛ばすことを確認した。[独立した回帰](../tests/suite/proc/dump_named_fork_scopes.sv)も成功している。VCD実装の不足として残件へ数えない。
- [10月28日の非slang台帳](kirimasim_nonslang_triage_2026-10-28.md) の「25本すべてconfirmed」は、この後続調査と修正を含んでいない。現在の確定残件数として再利用しない。
- [#58](https://github.com/kurochan001/kirimasim/issues/58) のconfig選択案は、複数configをエラーにする現行 [CLI仕様](spec/CLI.md) と食い違う。テストの期待に合わせて黙って変えず、仕様修正案の承認が必要である。
- その他の公開Bugを全件再トリアージした監査ではない。未調査のIssueを完了扱いにはしていない。

## 6. 7晩連続ファジング：未達ではなく記録更新の残作業

正式条件は [TEST_SUITE](spec/TEST_SUITE.md) の「Cross-check with valsem」：
通常CIの固定seed比較に加え、日付seed・100倍の試行で、判定前の7連続nightlyに差分がないこと。

以下の7回は全て `event=schedule` の Nightly。
`Fuzz against valsem and the reference scheduler` のコマンドと終了結果を実ログで確認した。
全て `KIRIMA_FUZZ_SCALE=100`、`KIRIMA_FUZZ_SEED=$(date -u +%Y%m%d)` を指定している。

| nightly日付 | commit | 正式fuzzの結果 | 証跡 |
| --- | --- | --- | --- |
| 2026-10-21 | `b27968d1f3c1` | 46 tests passed | [p1-gates](https://github.com/kurochan001/kirimasim/actions/runs/39681273172/job/119504065777) |
| 2026-10-22 | `6ec9964c14b4` | 47 tests passed | [p1-gates](https://github.com/kurochan001/kirimasim/actions/runs/39858890595/job/120104127806) |
| 2026-10-23 | `aae29deaed98` | 104 tests passed | [p1-gates](https://github.com/kurochan001/kirimasim/actions/runs/40026756027/job/120664754604) |
| 2026-10-24 | `c507631d2529` | 104 tests passed | [p1-gates](https://github.com/kurochan001/kirimasim/actions/runs/40131731554/job/120984671470) |
| 2026-10-25 | `a4f653c1d0fd` | 108 tests passed | [p1-gates](https://github.com/kurochan001/kirimasim/actions/runs/40232848500/job/121288067792) |
| 2026-10-26 | `1650064d21c7` | 108 tests passed | [p1-gates](https://github.com/kurochan001/kirimasim/actions/runs/40414439435/job/121893084547) |
| 2026-10-27 | `ae5f9dcd1d2a` | 110 tests passed | [p1-gates](https://github.com/kurochan001/kirimasim/actions/runs/40593559897/job/122503629425) |

10月21〜22日のfilterは `W4Ops.*:Valsem.*:Scheduler.*`、23〜27日はそれに `Heap.*` が加わる。
正式条件が対象とする比較は全7回に含まれている。`Wide.*` は別ステップであり、この7回の件数へ混ぜていない。

10月23〜26日はworkflow / p1-gatesジョブ全体が失敗しているが、**p1-gates内**で失敗したステップは `README counts`。
対象のファジングステップは成功しているので、「workflowが赤」というだけでファジング失敗や連続数のリセットと扱わない。
10月21〜27日の手動Nightly 6回についても、対象ファジングステップは全て成功していた。
たとえば [10月22日の手動実行](https://github.com/kurochan001/kirimasim/actions/runs/39736534314/job/119683430177)、[10月27日の手動実行](https://github.com/kurochan001/kirimasim/actions/runs/40523863226/job/122257954481)も、p1-gates内の失敗は `README counts` である。手動実行を別の晩として7晩へ加算してはいない。

これは少なくとも7晩の証跡を確認したもので、最長連続晩数を集計したものではない。
また、仕様は7晩全て同一commitであることを要求していない。
現行版そのもののnightlyが未実施という理由で、さらに同一HEADで7晩待つ条件を追加しない。
M8では、その判定時点の直近7回と、途中に参照差分が発生していないことを照合する。

**記録の残作業**：`docs/counts.json` の `fuzz.nights=5` / `through=2026-10-21` と、そこから生成される両READMEの表示を更新する必要がある。
この監査ではそれらの既存ファイルは変更していない。

## 7. 測定の版と限界

| 対象 | 今回確認した版・範囲 |
| --- | --- |
| kirimasim | `0.0.0 (commit 4c97244784cc)`、release、AppleClang `21.0.0.21000334` |
| 機能比較のstable参照 | compiler / runtimeとも `Icarus Verilog 13.0 (stable) (v13_0)`。過去記録の `v13_0-dirty` と区別する |
| 機能比較のdevel参照 | compiler / runtimeとも `14.0 (devel) (s20260301-411-gca6b8d376)` |
| 機能比較の母集団 | 既存の独自95プローブを両参照と再実行。gapsは両方6件、参照結果不一致はstable 4件 / devel 7件 |
| 外部 P1 母集団 | 期待値commit `41085fe3432bb093bf52c4f33bc4b44fff1a9401` を隔離して実行。リストは `tests/gates/p1_population.txt` の1466件のまま |
| 現行通常 CI | [run 40695675823](https://github.com/kurochan001/kirimasim/actions/runs/40695675823)。全14ジョブ成功。通常テスト、sanitizer、最低サポートcompiler、lint、Linux TSanを確認 |
| ローカル CI | 同じ現行commitでdev / release / ASan / UBSan、PerfCount、format / tidy、期待値保護など全項目合格。既存skip 2件 |
| nightlyのshuffle / TSan | [10月27日 Nightly](https://github.com/kurochan001/kirimasim/actions/runs/40593559897)、commit `ae5f9dcd1d2a`。Linux / macOSのASan・UBSanでshuffleステップ成功、両OSのTSanジョブも成功 |
| sv-tests両OS比較 | 保存集計とnightlyの記録。**現行 `4c972447` の両OS比較を、この監査で再測定したものではない** |

現行版の独自プローブ・固定母集団を測り直したことと、異なる版で走った nightly の証跡は分けて扱う。
この文書は M8 の最終判定報告ではない。

再測定の出力はローカルの `build/p1-criteria-audit-2026-10-28/` にある。
追跡対象の過去比較は [2026-10-27のstable記録](measurements/2026-10-27/slang-e2d1021-new-icarus13.json)、[devel記録](measurements/2026-10-27/slang-e2d1021-new-icarus14.json) を参照できるが、それらのkirimasim版は `13fe2b65c817` であり、今回の再測定とは別である。

## 8. P1完了までの確認リスト

### 実装と機能範囲

- [ ] 節2の標準側5プローブを成功させ、独立した回帰・相互作用・寿命の検査を追加する。dense UDP はまず時間制限到達の原因を切り分ける。
- [ ] 必要な VPI の対象機能・callback領域・外部境界を具体化し、未記述の意味は仕様提案を経て実装する。
- [ ] FST backend、source `%p`、resolved-strength `%v`、指定済みの `--cond-x` / `--init-values` など、既存P1仕様に対する実装不足を埋める。未記述の詳細があれば仕様提案を先に行う。
- [ ] 節3の残る形を Icarus 13.0 と比較し、標準の必須範囲 / 拡張 / 対象外 / 参照不具合 / 未確認を形ごとに記録する。必要なものを旧P4/P5という理由だけで先送りしない。
- [ ] 値・領域順序・静的評価などの未解決Bugを整理し、必要な標準意味の不足を修正する。#61 / #65 の定数評価の残りをランタイム修正と混同しない。
- [ ] 拡張と外部インターフェースを別に棚卸しする。厳格な受理と衝突する拡張や、既存仕様と食い違う変更は、承認済みの互換性方針・仕様修正案を先に用意する。

### 判定と記録

- [ ] M8で判定する版について、sv-testsをLinux / macOSそれぞれの固定参照と比較し、固定1466件・必須一覧の結果と一緒に記録する。母集団や期待値を都合よく置換しない。
- [ ] 判定前の直近7回の正式nightly fuzzを照合する。今回の7晩の証跡は利用できるが、通常CI・単発ローカルfuzz・別のlibFuzzer実行を代用しない。
- [ ] 古いfuzz晩数や、#69等の後続調査を未反映の台帳を更新する。これは記録の整合性の作業であり、ファジングの追加未達条件ではない。
- [ ] 版、参照版、母集団、未対応・参照差分、seedと連続成功の証跡、機能範囲の対応表を揃えて M8 の判定報告を作る。
- [ ] 人による全件・抽出レビューと生成器ルール確認の記録を担当者が照合する。公開情報だけでは実施状況は未確認。標準本文や非公開レビュー資料は公開せず、件数・抽出seed・訂正結果を要約する。

最後の人によるレビューは [TEST_SUITE](spec/TEST_SUITE.md) が M8 報告に求める作業であり、同文書の合格率ゲートとは別である。
SIR v1凍結（P2）、C++生成 / Tier 2（P3）、Icarus比較範囲を超えるUVMや制約乱数などを、この文書で新たに P1 必須へ変更していない。
性能後退のIssueも重要だが、正式P1条件にない独立した性能数値ゲートを追加してはいない。

## 9. 再測定の方法

既存のmatching compiler / runtimeを用意し、まず `--version` / `-V` で版を確認する。
下の変数はローカルで検証済みの実行ファイル・固定版スイートを指すものとする。
新しい依存やツールの導入は、リポジトリの承認規則に従う。

```sh
build/release/src/driver/kirimasim --version
"$IVERILOG_13" -V
"$VVP_13" -V
python3 tools/icarus_feature_probe.py \
  --kirimasim build/release/src/driver/kirimasim \
  --iverilog "$IVERILOG_13" --vvp "$VVP_13" --extended \
  --output build/p1-criteria-audit-2026-10-28/icarus13-current.json \
  --require-parity
```

現状はgapが残るので、`--require-parity` を付けたコマンドは終了コード1になる。
結果JSONはその前に保存される。14.0もmatching binariesを指定して別ファイルへ実行する。
この終了コードは `real_net` のような拡張も含む全95件のgapで決まる。終了コード自体を、標準側の P1 完了を判定する独立したゲートにはしない。

```sh
python3 tools/p1_population.py run \
  --tests-repo "$P1_TESTS_REPO" \
  --kirimasim build/release/src/driver/kirimasim --jobs 12
```

`P1_TESTS_REPO` は `41085fe3432bb093bf52c4f33bc4b44fff1a9401` の内容を指す。
`select` で新しい分母を作って置き換える操作は、この再測定には含めない。
通常CIは `python3 tools/local_ci.py`、外部比較・nightlyの正式手順は [nightly.yml](../.github/workflows/nightly.yml) と [TEST_SUITE](spec/TEST_SUITE.md) を参照する。
