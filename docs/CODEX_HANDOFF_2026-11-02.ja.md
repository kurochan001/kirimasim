# Codexへの引き継ぎ：P1の現在地と再開手順（2026-11-02）

[English](CODEX_HANDOFF_2026-11-02.md) · この日本語版は英語版に対応します。

保守者の明示的な依頼により、日英の公開引き継ぎ資料として保存する。
2026-11-02 06:07 JST時点のmain・remote・GitHub CIを確認した。
以前のローカル資料`docs/CODEX_HANDOFF_2026-10-31.md`は、未追跡の履歴として保持し、公開に含めない。
その冒頭にある古いmain・件数・未対応一覧ではなく、まず本資料の現状を読む。

## 1. 最初に把握すること

- **文書公開前の実装チェックポイントは、main = origin/main = remote main =
  `866abf4d86d11130a9946b79ea5fd19e2913b19c`。**
  スカラーspecifyの8コミットは、保守者の依頼によりfast-forward・通常push済み。
  未pushの実装や、今回の実装・期待値移行についての承認待ちはない。
  本資料の公開コミットはこの先頭に追加されるため、再開時の先頭はGitで確認する。
  文書公開で実装・期待値・件数は変更しない。
- **slangへの投稿は、保守者の明示的な指示で保留中。** 再開の指示までは投稿しない。
- P1は未完了。有限の176プローブでは、必須範囲の差は`dense_udp_coverage`の1件だが、
  これだけでP1全体の残作業が1件になったわけではない。
- Icarus拡張`wire real`はP1の必須条件から除外済み。依存を確認した`real_net`だけを
  対象外に分類し、標準の`real`機能や`nettype real`まで一括除外しない。
- **公開文書は日英両対応。** 本資料も英語版・日本語版を同時に更新する。

最初に次を読む。

1. 本資料。
2. 手元にある場合は、未追跡の優先順位資料`docs/P1_PRIORITIES_2026-10-31.ja.md`。
   これはローカルの計画資料で、新規cloneには含まれない。現在の公開上の根拠は次項の仕様・対応表。
   95プローブ・未実装class等の記述は過去の測定なので、現在の一覧と混同しない。
   Claude Codeが受け持つ予定だった棚卸し・specify/VPI提案等もCodexが引き継ぐ。
3. [P1_SCOPE](spec/P1_SCOPE.ja.md)、[現在の対応範囲](P1_SUPPORT.ja.md)、
   [今回のspecify実行](notes/specify-execution.ja.md)。
4. 実装を広げる場合は、[承認済みspecify契約](notes/specify-p1-proposal.ja.md)と
   [VPIの初期契約](notes/vpi-p1-proposal.ja.md)。

`AGENTS.md`の依存・禁止事項・期待値保護・仕様承認の規則を守る。
同ファイルの旧プロジェクト名、P0、parseだけという状態説明は古い。
実際のコードの名前空間・パスは`kirimasim`で、現在地はREADMEとP1_SUPPORTで確認する。
`CLAUDE.md`と[AI_POLICY](spec/AI_POLICY.md)も参照する。
ルートの`RTK.md`は存在しなかった。参照された内容は未確認であり、推測で補わない。

## 2. 保守者の指示・承認・保留

- **今回のspecify契約1〜8節は承認済み。** 再承認を求めず、承認範囲の実装を維持する。
  状態依存path等の未記入の意味まで、この承認に含めない。
- **既存`specify_path.sv`の分類移行は別途承認済み。** 実行結果と具体的な移行案を示した後、
  保守者から「承認OK」を受領。実装とは別の`737416ef`へ保存した。詳細は
  [移行記録](notes/specify-expectation-migration.ja.md)。再承認は不要。
- **今回のmain反映・pushも了承済みで完了。** この許可を、将来の変更のpush許可へ拡張しない。
  本資料の日英版の公開も、今回新たに明示依頼された。
  次の別の変更のpushは、その変更についての保守者の依頼・了承を確認する。
- **新しいツール・依存の導入、既存仕様と異なる意味の実装は事前承認が必要。**
  先に具体的でレビューできる提案を完成させ、承認を得てから実装する。
- 現在、提示済みの具体案に対する承認待ちはない。次の追加仕様はまだ別途提案する段階。
- slangへの投稿は保留。草稿が置いてあっても投稿許可とは扱わない。
- ストリーミングの`with`・stringの左辺、E02のevent代入は、保守者の従来の保留判断を維持する。
  必要なP1範囲との新たな食い違いが分かった場合は、勝手に変更せず具体的に示す。
- `wire real`対応を今回のP1必須へ戻さず、互換モードも勝手に追加しない。

## 3. mainに入った直近8コミット

起点は`80859f34`。本体・承認済み期待値・件数・文書の分離を保持したままpushした。

1. `5baf5b9e` — 独自specify参照102入力、判定器の対照実験、詳細契約の準備。
2. `407888e5` — 保守者が承認したspecify契約1〜8節を正式仕様の有効な改定として記録。
3. `fdc88772` — 独立したvalsem参照と共有kernelのスカラーpath処理。
4. `c56e86d5` — typed SIR、VM、slang frontend、ソース行列、VPI/VCD観測の接続。
5. `737416ef` — 承認済みの既存拒否期待値1件だけを別途移行。
6. `4aa68356` — 無関係な未対応宣言で従来の診断の走査を保つ修正。
7. `b62cf990` — 全件成功したdev JUnitから件数だけを更新。
8. `866abf4d` — 公開日英文書、FAQ・用語集、比較・検証記録を更新。

すべてnoreply作者、`Assisted-by: Codex:GPT-6`付き。履歴を書き換えたり、まとめ直したりしない。
人の承認・レビューのトレーラーはAI_POLICYに従い、実際の記録なしに推測して付けない。

## 4. specifyで実装した範囲と境界

対応したのは、module instanceのスカラー4値入力netと、単一の通常の機能driverで駆動する
スカラーwire/tri出力。単純`=>`・完全接続`*>`、複数スカラー入出力、無条件のposedge/negedge、
データ・極性注釈、specparam/parameter、min/typ/max、実数・負値・ゼロ遅延、階層・generateを含む。
内部手続きの状態から1本の連続代入を介して出力する形にも対応する。

意味を決める要点は以下。

- 遅延は`01,10,0z,z1,1z,z0,0x,x1,1x,x0,xz,zx`の12遷移へ正規化する。
  1/2/3/6/12個のリストを扱う。省略したXへの到着はmin、Xからの出発はmax。
  通常のgate用3遅延selectorへ置き換えない。
- 入力の実際の4値変化だけを、callbackより前に記録する。強度だけの変化では時計を進めない。
  資格のある最新入力を選び、同時刻なら最小の遷移遅延を選ぶ。
- 同じセル内では、絶対path期限と分布遅延の機能到着のmaxを使い、加算しない。
  直列の別セルは、前段の公開到着を自分の入力イベントとして扱う。
- 同じ予約値は期限を保つ。確定寄与への戻りは未公開パルスを取り消し、古い世代のtimerは無効。
  完全な値・強度の寄与を所有し、VMの一時spanを借用しない。
- 期限到達・ゼロ・期限超過のpath公開はActive。通常の`drive.delay #0`のInactiveと混同しない。
- 元の物理originを保持し、解決済みreadoutを新しい物理driverとして再投入しない。
  公開出力と非公開の機能結果を分け、内部・外部の読み取り、monitor、`%v`、VPI、VCDを一致させる。
- forceは公開netを即時固定するが、下の寄与・予約更新は継続する。
  releaseは現在の確定寄与を同期して読み、将来の期限を早めず、経過した期限も失わない。
- data/polarity注釈を機能計算として実行しない。注釈の関数やRNGを実行しない。
- SIRの数値binding・所有関係はVMのスロット割当前に検証する。
  foreign net参照はVPIのnet束縛位置だけに渡せ、通常の書込みrefへ転用できない。
- 可変状態はContext所有。kernelのslang/SIR依存、公開C ABI変更、新しい依存を追加していない。
  kernel公開ヘッダはC++20のまま。

**まだ未対応で、位置付きエラーにするもの：** vector・bit select・出力変数・inout端子、
受動接続・複数driverの出力、tri-state/MOS/UDPの機能driver、状態依存`if`/`ifnone`、
明示的pulse control、PATHPULSE$、SDF、timing check。
これらを新しいP1対象外の例外としたわけではない。

### 実装を見る入口

- `src/kernel/include/kirimasim/kernel/module_path.h`、`src/kernel/module_path.cpp`。
- `src/kernel/sim.cpp`、`strength_drivers.cpp`、`strength_publication.cpp`の時計・公開との接続。
- `src/valsem/module_path.cpp`の独立した端点参照。
- `src/sir/include/kirimasim/sir/module_path.h`、`src/sir/module_path.cpp`、`design.h`、`verify.cpp`、`text.cpp`。
- `src/front/lower_module_path.cpp`、`lower.cpp`、`lower_stmt.cpp`と、既存の`lower_strength.cpp`。
- `src/vm/vm.cpp`の数値bindingの受渡し。時間の意味をVM独自の条件分岐へ移さない。
- `tests/unit/module_path_test.cpp`、`module_path_sir_test.cpp`、`module_path_observation_test.cpp`。
- `tools/gen_specify_matrix.py`、`tests/cli/run_specify_tests.py`、`tools/mutants.py`。

## 5. 検証済みの候補とCI

**不変の`866abf4d86d1`に対するlocal CIは全21項目成功。**
実行中はソース・追跡文書を編集せず、終了後も専用worktreeはcleanだった。

- dev：11,447/11,447、200.84秒。
- release：11,448/11,448、133.92秒。
- ASan：11,447/11,447、212.13秒。
- UBSan：11,447/11,447、181.50秒。
- 全構成で失敗・skipなし。releaseの差1件は従来の既定イベント上限テスト。
- 全188翻訳単位のclang-tidy 23.1.4は診断0。書式、禁止API、global-state probe、SPDX、
  release flags、suite files、8コミットそれぞれの期待値分離、件数も成功。
- Apple clang 21.0.0、CMake 4.4.4、clang-format/clang-tidy 23.1.4で実測。

専用worktreeは`../kirimasim-p1-class-runtime`。ログ・補助scriptは同ディレクトリの以下。

- `build/specify-local-ci.log`、`build/local-ci-before-handoff-publication-20261102/`：
  実装チェックポイントの全local CI。
- `build/handoff-publication-local-ci.log`、`build/local-ci/`：日英文書を公開する候補の検証用。
  公開候補を固定して全local CIを実施し、最終結果はpush時の報告で確認する。
- `build/specify-full-dev.xml`、`build/specify-full-dev.log`：件数更新に使った独立dev全件成功。
  このrunは130.23秒で、上記local CIのdev再確認とは別のrun。
- `build/specify-native-controls.json`、`build/specify-native-controls.log`：最終15変異の実行。
- `build/check_specify_native_controls.py`：対象だけをビルド・実行した補助script。
- `build/specify-push-preservation.json`：main反映・push前後の草稿保護の記録。
- `build/local-ci-before-specify-20261102/`：その前のlocal CIログ。

`build/`内はignoredで、この手元にだけある。新規cloneに存在する前提にしない。
追跡済みの根拠は[specify検証metadata](measurements/2026-11-02/specify-execution-validation.json)と
[15変異の記録](measurements/2026-11-02/specify-native-controls.json)にある。
変異は全件コンパイルし、実行の意味または所有権の検査で検出。ビルド失敗・登録簿自身の失敗は数えない。
元ソースのSHAを復元し、nativeの両方の基準検査が再度成功した。

**GitHub CIは、上のlocal CIと区別する。**
[push run 41325154021](https://github.com/kurochan001/kirimasim/actions/runs/41325154021)を
2026-11-02 06:07 JSTにread-only APIで照会し、`completed`、`conclusion: success`を確認した。
対象SHAは`866abf4d86d11130a9946b79ea5fd19e2913b19c`と一致した。
これは実装チェックポイントの結果であり、文書公開のpush workflowは別runになる。
再開時には、その時点の対象SHAに対応するrunを確認する。

## 6. 比較の現在地と、再測定していないもの

- [元の176プローブ](measurements/2026-11-02/icarus13-scalar-specify.json)：
  **P1評価対象175、対象外1、必須差1、raw gap 2、参照不一致25。**
  必須差は`dense_udp_coverage`の15秒frontend時間超過。
  対象外は依存を確認したIcarus `wire real`拡張の`real_net`。
- 176件すべてのソース・期待値・P1分類は以前の記録と同じ。
  nativeの結果が変わったのは`specify_path`だけで、従来の期待どおり早い時点で0、遅い時点で1。
  他175件の診断・結果も保持した。
- [specifyの102入力](measurements/2026-11-02/specify-execution-inventory.json)：
  native期待一致83、未対応16、固定oracleのない実行3。ソース・期待値は全件不変。
  参照側は期待一致87、差6、観測のみ9。観測をnative PASSへ書き換えない。
- native専用行列は**正常実行105＋期待した位置付き拒否16＝121ケース**。
  単体82件を追加。これらのケース数とCTestのテスト件数を混同しない。
- 現在のdev件数は11,447。unit 1,217、suite 9,948、Tools 193、DumpSir 29、Context 21、
  CLI+Perf 39、生成器176。`docs/counts.json`とREADMEの生成範囲が正。
- 固定P1母集団1,456/1,466、sv-tests（344件中kirimasim 244、IcarusはLinux 211・macOS 210）、
  7晩のファジング、Icarus 14.0等は過去の測定のまま。今回の実装で測り直していない。
  `docs/counts.json`の外部測定日や、過去の95プローブの項目を現在の176件へ勝手に置換しない。
- P1の残りの総件数・工数、Icarus 13.0との全面的な同等性は**未確認**。
  最終M8には人によるレビューの記録と、判定対象の版での証跡の確認も要る。

### `wire real`の例外を広げない

対象外なのは拡張への依存を確認したケースだけ。`real`変数・実数演算、標準のreal変数への単一の
連続代入、`nettype real`はこの例外に含めない。今回を理由に`nettype real`を新しい必須機能へ
追加する変更でもない。未対応をPASSに書き換えず、対象数と対象外数を分ける。
クラッシュ・ハング・内部エラーは、対象外でも独立した不具合として失敗に残す。

### IEEEの版の未確認事項

specify契約の一次資料として実際に読めたのはIEEE 1800-2017の30章。
公式2023配布本文は取得できず、**2023本文との節ごとの直接照合は未確認**のまま。
具体的な1〜8節のプロジェクト契約は承認済みだが、版の監査完了とは扱わない。
次の意味を広げる前に[契約9節](notes/specify-p1-proposal.ja.md)と公式配布で確認する。
規格本文・図・例の複写を公開repoに入れない。読んだPDFは専用worktreeのignored `build/tmp/pdfs/`。

## 7. 次の担当者が進める順

1. **現在のGitHub CI結果を確認し、mainと専用worktreeの状態を把握する。**
   旧ブランチや古い資料を、そのまま未実装・未pushと判定しない。
2. **比較の棚卸しを継続する。** 176件の外の必要な標準機構を、自作の形別プローブで調べる。
   元ソース・期待値を固定してbefore/afterを取り、Icarusの参照差とnativeの不足を区別する。
   他プロジェクトのソース・テスト・issueの再現コードを移植しない。
3. **次のspecify範囲の具体案を作る。** 状態依存`if`/`ifnone`、vector、出力変数・複数driver等と、
   必要なtiming checkを棚卸しする。初期実装に含まれると見なさず、条件・X/Z・パルス・
   notifier・領域・port/force・寿命・不正IR・検査手順まで具体化して承認を得る。
   状態依存条件は通常の`if`と同じと仮定しない。既存参照ではX/Z条件の差も記録済み。
   `$setup`等が参照でコンパイルできただけで、timing checkを実行できたとは判定しない。
4. **VPIの初期範囲を超える必要部分、FST、残るclass/container/port/入力の境界を優先度順に。**
   VPI登録・静的数値reflection・変数書込み・対応callbackは実装済み。
   net書込み、未対応型・API/callback、cocotbは残作業。FSTは依存だけでbackendは未実装。
   `--compat`は意味と文書の整合を確かめて提案する。独自の緩和モードを勝手に作らない。
5. **dense UDPは投稿保留を維持。** 固定slangの網羅検査が原因という既存の調査・草稿を保持する。
   この差を除外して合格にせず、slangを改変したり依存の固定版を勝手に進めたりしない。
   新たな再現調査は独立した場所で行い、投稿再開・依存変更は保守者の指示を待つ。
6. **正しさの不具合とM8の証跡を並行して棚卸しする。**
   古いIssue件数・open/closed状態は今回再照会しておらず、現在の値として記さない。

すでにclass実行、計算したalways_ffの辺、名前付きforkのdisable、部分inout/port実効種別、
ソースの`%p`の一部、強度`%v`、警告オプション、初期VPI・スカラーspecifyが入っている。
ただし各機能の全形が完成したという意味ではない。詳しくはP1_SUPPORTの境界を読む。

旧引き継ぎの10-28レビューA/B/D〜G（負・広い`%t`、実数精度上限、範囲外ref、巨大割当、
`-0.0`/monitor、CLIの追加報告）は過去の保留一覧。このセッションでは全項目の再監査をしていない。
現在の仕様・実装・承認履歴と照合せず、新しい承認待ちとして一括で復活させない。

## 8. 作業ツリーと草稿を守る

通常のmain作業ツリーは`kirimasim`。次の既存7ファイルは今回も変更せず保持した。

- 追跡済み・変更あり：`docs/notes/slang-udp-coverage-issue-draft.md`と`.ja.md`。
- 未追跡：`docs/CLAUDE_CODE_HANDOFF_2026-10-30.md`。
- 未追跡：`docs/CODEX_HANDOFF_2026-10-31.md`（過去履歴、追記41節まで）。
- 未追跡：`docs/P1_PRIORITIES_2026-10-31.md`と`.ja.md`。
- 未追跡：`docs/measurements/2026-10-31/slang-udp-coverage-standalone.json`。

本資料の日英2ファイルだけを公開対象にし、上の7ファイルは含めない。
公開後も残る7ファイルの表示を、未コミットの実装と混同しない。
**`git add -A docs`、一括stash、無関係なreset/cleanをしない。** stageはファイルを名指しする。

検証済みworktreeは`../kirimasim-p1-class-runtime`、ブランチ`feat/p1-specify-runtime`、
実装の検証時はmainと同じ`866abf4d`でcleanだった。
文書公開時にはこの検証済みworktreeを文書の候補へfast-forwardする。
前の名前の`class-runtime`でも、中身は現在のspecifyまで含む。
再利用する前に担当・プロセス・dirtyを確認し、他の作業と同時に使わない。

他にも`fix/inside-array-element-extension`、`fix/p1-core-regressions`、配列・pattern・queueの
期待値用ブランチ、古い比較用detached worktree等が残っている。
存在は`git worktree list --porcelain`で確認したが、各担当・稼働状況は未確認。
今回触っていないので、削除・reset・再利用・古いブランチの再mergeを勝手にしない。

## 9. 実装・検証で繰り返さないこと

- local CI・コンパイル変異中は、その作業ツリーのソースを固定する。
  変異中に別profileをビルドしたり、実行バイナリを別の測定で使ったりしない。
- 変異のアンカーが当たるだけでは検出ではない。実際にコンパイルし、意味を検査するテストで
  失敗を確認して、元のSHAと基準実行を復元する。`--only`は部分文字列で、ORではない。
- 新しい期待値は根拠とともに追加できるが、既存の期待値変更・削除・分類移行は理由と規格節を
  示して別途レビューし、本体と分離する。既存assertionを弱めて通さない。
- 件数の総数・生成器数を変えた直後は、`tools/readme_counts`自身が失敗しやすい。
  必要なら件数ファイル・README日英・VALIDATION_COUNTS日英を退避して総数だけ仮同期し、
  全dev実行後に必ず復元する。**完全に成功したJUnitからだけ**最終の通過数を更新する。
  `build/specify_bootstrap_counts.py`は今回使った補助scriptだが、再利用時は内容を確認する。
- JUnit出力は絶対パスにする。presetで相対パスを渡すと、想定と違う`build/dev/`以下へできる。
- 途中で失敗・中断したrunのJUnitから、成功数を作らない。
  今回は診断走査の回帰を見つけて中断・修正し、その後の全件成功XMLだけを採用した。
- clang-tidyはlocal CIの最後。新しいTUは早めに対象を絞って確認すると手戻りを減らせる。
- `main/build`と専用worktreeの`build`を混同しない。CMakeCacheは作業ツリーに結び付く。
  バイナリの`--version`を実際に確認し、別のcheckoutでの結果を現在の証拠として使わない。
- Icarus/Verilatorの実装ソースや生成bytecodeは読まない。slangの依存ソースは読めるが改変しない。
- 公開資料は日英同時更新。非公開資料、個人・機械情報、規格の本文・図・例、商用simulator出力を
  公開repoに入れない。未確認の版・意味・動作を推測で記録しない。

## 10. 再開時のコマンド

まずmain側で読むだけの確認を行う。

```sh
git status --short --branch
git rev-parse HEAD origin/main
git ls-remote --exit-code origin refs/heads/main
git log -8 --oneline
git worktree list --porcelain
```

専用worktreeを使えると確認した場合、対象検証は以下。
同じ不変の候補について全local CI済みなので、引き継ぎを読むだけのために再実行する必要はない。

```sh
cd ../kirimasim-p1-class-runtime
git status --short --branch
build/dev/src/driver/kirimasim --version
ctest --preset dev -R 'ModulePath|cli/specify' --output-on-failure
python3 tests/cli/run_specify_tests.py --kirimasim build/dev/src/driver/kirimasim
python3 tools/mutants.py --check
```

実際にpathの仕組みを変えたときの変異入口は以下。実行中は同じソースで他の作業をしない。

```sh
python3 tools/mutants.py --only 'scalar module path:'
```

変更後に全dev JUnitを取る場合は、出力先を固定し、件数同期の前提を先に確認する。

```sh
ctest --preset dev -j8 --output-junit "$PWD/build/p1-next-dev.xml"
python3 tools/readme_counts.py check --build build/dev --junit build/p1-next-dev.xml
```

次のpush前には、承認された候補を固定してlocal CIを確認する。
この環境で使った既存LLVMの場所は以下で、新たにインストールしたものではない。

```sh
CTEST_PARALLEL_LEVEL=8 CMAKE_BUILD_PARALLEL_LEVEL=4 \
  python3 -u tools/local_ci.py --llvm-bin /opt/homebrew/opt/llvm/bin
```

本資料の公開では、日英2ファイル以外の実装・期待値・件数・元の7ファイルを変更しない。
公開候補を固定してpush前のlocal CIを実施する。次の担当者は、元の未追跡資料をstageせず、
現在の証拠と保守者の保留指示を保って再開すること。
