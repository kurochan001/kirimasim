# FST の依存設定

[English](fst-dependency-setup.md) · 英語版が正本です。

Status: **依存設定を実装済み（2026-10-26）**。利用者の依頼によるものです。
FST 実装の準備を記録するもので、新しい波形の意味を承認したものではありません。
波形バックエンドは未実装で、`--wave=<file>.fst` は引き続きコマンドラインの誤りに
なります。別の作業である P1 実装のゴールは一時停止したままです。

## 依存とビルドの境界

独立した [libfst リポジトリ](https://github.com/gtkwave/libfst) を、
commit `17f6a5d27b2f576d65b550114fbde3c035cc74a8`（2026-10-18）に固定して取得します。
上流のプロジェクト情報は版 1.0.0 です。固定するのは commit で、リリースタグがあるとは
主張しません。[その commit の公式ライセンス](https://github.com/gtkwave/libfst/blob/17f6a5d27b2f576d65b550114fbde3c035cc74a8/LICENSE)
には、libfst の MIT、同梱 LZ4 の BSD-2-Clause、同梱 FastLZ の MIT の条件があります。
表示の全文は [THIRD_PARTY_NOTICES](../../THIRD_PARTY_NOTICES.ja.md) に載せました。

`cmake/FstDependency.cmake` が静的ターゲット `kirimasim::fstapi` を定義し、kernel が
私有の依存としてリンクします。公開 kernel ヘッダに FST API は出さず、kernel に
slang や LLVM の依存も加えません。取得したソースの変更やリポジトリへの同梱はしません。
GTKWave アプリや他シミュレータの実行系を実装の参考として取り込むこともありません。

CMake の定義は、上流の [ルートのビルド情報](https://github.com/gtkwave/libfst/blob/17f6a5d27b2f576d65b550114fbde3c035cc74a8/meson.build) と
[ソースのビルド情報](https://github.com/gtkwave/libfst/blob/17f6a5d27b2f576d65b550114fbde3c035cc74a8/src/meson.build) に合わせています。
この設定に使ったのはライセンス、ビルド情報、API 宣言です。実行処理の本体や上流の
テストは設計の参考にせず、smoke テストは kirimasim で独自に書きました。

- システムの zlib 1.2.0 以上、bzip2 1.0.0 以上のヘッダとライブラリを使います。
  Debian/Ubuntu では `zlib1g-dev libbz2-dev`、macOS では Command Line Tools の
  SDK を使います。不足は CMake の構成時に検出します。これらはシステムから検出し、
  固定したソースをダウンロードするものではありません。
- libfst 同梱の LZ4 と FastLZ をビルドします。別の LZ4 パッケージ、Meson の
  インストール、GTK、GLib は不要で、上流のテストもビルドしません。
- `fseeko`、`realpath` を検出し、`_GNU_SOURCE` と `FST_INCLUDE_CONFIG` を設定して、
  ビルドディレクトリに私有の `config.h` を生成します。
- バックグラウンドの書き込みスレッドは無効です（`FST_WRITER_PARALLEL` と
  `HAVE_LIBPTHREAD` を未定義にします）。有効化は今後の別作業です。
- 上流のビルド情報に合わせ、対応するコンパイラでは FastLZ だけに
  `-fno-sanitize=alignment` を付けます。他の検査と kirimasim 自体の sanitizer は
  有効のままです。

CI と nightly は、ビルドの構成前にシステムの圧縮パッケージを用意します。
FetchContent のキャッシュキーは全 `cmake/*.cmake` と `cmake/*.in` を含み、FST の
固定 commit や生成ヘッダのひな型の変更にも追従します。

## 確認したこと

macOS arm64、Apple clang 21.0.0、CMake 4.4.4、Ninja 1.13.2 で確認しました。

- dev の全ビルドと CTest：5,384 本のうち 5,382 本が成功、失敗 0、スキップ 2
  （`suite/cb/input_sample_1step`、`suite/nets/net_port_drivers_part`）。
- dev・release・ASan・UBSan で `tools/fst_dependency` が通ります。
  zlib・FastLZ・LZ4 の各圧縮モードでファイルを書き、libfst の reader で開いて、
  変数の数と時刻の範囲を確認します。C++20、例外と RTTI なしでコンパイルします。
  これは依存の動作確認で、波形の値の読み戻しや他のビューアとの互換性の検査ではありません。
- CMake は SDK の zlib 1.2.12、bzip2 1.0.8 を検出しました。`otool -L` では対応する
  システムの動的ライブラリが見えます。同梱の圧縮処理は静的です。取得した libfst の
  ソースは固定 commit と一致し、変更していません。

dev 全体の再確認は README の通常のコマンドで行えます。依存だけの確認は、`p` を
`dev`、`release`、`asan`、`ubsan` としてそれぞれ次を実行します。

```sh
cmake --preset "$p"
cmake --build --preset "$p" --target kirimasim_fst_dependency
ctest --preset "$p" -R '^tools/fst_dependency$' --output-on-failure
```

この設定では release・ASan・UBSan の全テストと `tools/local_ci.py` は実行していません。
この 3 構成では依存のターゲットだけをビルドして検査しました。Linux とホストされた CI の
実行は **未確認** です。公開後に設定済みワークフローを実行し、パッケージの準備、各
コンパイラ、smoke テストを確認できます。実際の波形の値、ビューアとの互換性、dump タスク
との接続、性能は、今後の FST バックエンドとそのテストで確認します。
既存の VCD の動作と既存の期待値は変更していません。
