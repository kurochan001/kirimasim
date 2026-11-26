# SVG図

[English](README.md)

既存のkirimasimの動作と境界を説明する独自の図です。SVGファイル自体を編集用のソースとして
保守します。テキスト・線・図形と、内蔵のスタイル・白い背景を使い、Mermaidの描画器、
外部フォント、スクリプトなしで表示できます。

- [不具合調査の段階](debugging-stages.ja.svg)：[不具合調査ガイド](../DEBUGGING.ja.md)の
  検査と、失敗した段階に応じた診断の分岐。
- [VPIの起動順序](vpi-lifecycle.ja.svg)：[VPI実行ノート](../notes/vpi-execution.ja.md)の
  コンパイル時の登録・検査と、実行モードによる実行時処理の境界。
- [代入値の保持](assignment-capture.ja.svg)：[構成資料](../ARCHITECTURE.ja.md#代入値の保持と待機)の
  フレームによるスナップショットの保持と、独立した保留処理による保持の比較。
- [実行層と共有カーネル](execution-tiers.ja.svg)は、ソース・保存済み SIR の入力、キャッシュ、VM への切り替えを示します。
- [0.2.2 の VM と native の固定25回路比較](simulation-speed-0.2.2-vm-native-daily25.ja.svg)は、既定の VM を1倍としてキャッシュ準備後の実行速度を示します。
- [0.1.3 と 0.2.2 の固定25回路比較](simulation-speed-0.1.3-0.2.2-daily25.ja.svg)は、各測定日の Icarus を1倍として両版の速度倍率を示します。
- [0.1.3 と 0.2.1 の固定25回路比較](simulation-speed-0.1.3-0.2.1-daily25.ja.svg)は、各測定日の Icarus を1倍として両版の速度倍率を示します。
- [0.1.3 と 0.2.0 の固定25回路比較](simulation-speed-0.1.3-0.2.0-daily25.ja.svg)は、各測定日の Icarus を1倍として両版の速度倍率を示します。
- [0.2.0 と Icarus の固定25回路比較](simulation-speed-0.2.0-daily25.ja.svg)は通常設定の4状態 VM の速度倍率を示します。[実測値と測定条件](../BENCHMARKS.ja.md#v020-daily25)
- [25回路でのシミュレーション速度比較](simulation-speed-0.1.3-daily25.ja.svg)は、IcarusのΔをkirimasim 0.1.3のΔで割った値を示します。Δの定義と各回路の測定値・条件は[ベンチマーク表](../BENCHMARKS.ja.md#v013-daily25)にあります。

`.svg`と`.ja.svg`を一緒に保守し、接続・所有関係・範囲を合わせます。ラベルと座標を直接編集し、
ルートの`viewBox`、矢印の線と先端の座標、アクセシビリティ用の`title`・`desc`、埋込み先の代替テキストを
整合させてください。編集後は、日本語の代替フォントを含めて文字と線の表示を確認します。

図は、リンク先の説明と承認済み契約を要約したものです。矢印は処理順や所有関係を示し、
代入の図には時間幅やスケジューラのリージョン順を持たせていません。ソースの形・型の制限と
例外は、本文の説明を保ちます。
