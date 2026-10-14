# haremasim

[English](README.md)

haremasim は、開発の初期にあるオープンソースの SystemVerilog シミュレータです。次のどれも手放さないシミュレータを目指しています。

- **4 値**（0、1、X、Z）を既定とし、4 値の意味を変える最適化をしない
- **IEEE 1800-2023 どおりのスケジューリング**。時刻スロットのすべての領域と、規格の参照アルゴリズムに従う
- **実用的な速さ**。X や Z が無いと証明できるとき、または実行時に確かめられるときは、2 値のコードで実行する

> **状態：まだ使えません。** 今のリポジトリにあるのは P0 の土台だけです。ビルドの仕組み、ライブラリの骨組み、[slang](https://github.com/MikePopoloski/slang) で読むだけの `haremasim parse`、最初のテスト、CI です。`docs/spec/` の詳細仕様書は見出しだけで、P0 の合格条件はまだ満たしていません。シミュレーションの機能はまだありません。

**コードの大半は AI（Claude Code）が生成し、人が仕様・テスト・レビューに責任を持ちます。** 詳しくは [docs/spec/AI_POLICY.md](docs/spec/AI_POLICY.md) と [CONTRIBUTING.md](CONTRIBUTING.md) を見てください。

## ビルド

必要なもの：CMake 3.28 以上、Ninja、Python 3、C++23 のコンパイラ（macOS arm64 の Apple clang 21 で確認。CI では Ubuntu 24.04 でもビルドします）。最初の configure で slang と GoogleTest を取得します。

```sh
cmake --preset dev            # ほかに release、asan、ubsan
cmake --build --preset dev
ctest --preset dev
```

| プリセット | フラグ |
| --- | --- |
| `dev` | `-O1 -g` |
| `release` | `-O3` |
| `asan` | `-O1 -g -fsanitize=address -fno-omit-frame-pointer` |
| `ubsan` | `-O1 -g -fsanitize=undefined -fno-sanitize-recover=all` |

ビルドの結果は `build/<プリセット名>/` に出ます。

## 使い方（今できること）

```sh
build/dev/src/driver/haremasim --version
build/dev/src/driver/haremasim parse file.sv ...        # 読んでエラボレーションする
build/dev/src/driver/haremasim --stats=json parse file.sv
```

`parse` は、slang がエラーを 1 件でも報告すると終了コード 2 で終わります。

## 文書

- [docs/ARCHITECTURE.md](docs/ARCHITECTURE.md)：ライブラリの分割と実行の仕組み（英語）
- [CONTRIBUTING.md](CONTRIBUTING.md)：貢献の方法（英語）
- [CHANGELOG.md](CHANGELOG.md)

## ライセンス

haremasim のライセンスは Apache License 2.0 with LLVM Exceptions（`Apache-2.0 WITH LLVM-exception`）です。[LICENSE](LICENSE) と [THIRD_PARTY_NOTICES.md](THIRD_PARTY_NOTICES.md) を見てください。
