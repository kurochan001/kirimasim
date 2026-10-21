# 第三者のソフトウェアについての表示

[English](THIRD_PARTY_NOTICES.md) · 英語版が正本で、これはその日本語版です。食い違ったときは英語版に従います。

kirimasim 自体のライセンスは、Apache License 2.0 with LLVM Exceptions です（`LICENSE` を見てください）。このリポジトリには、第三者のソースの写しを含めていません。下の部品は、構成のときに CMake の `FetchContent` で取得し、挙げた版に固定して、kirimasim かそのテストにコンパイルして組み込みます。各部品のライセンスは、取得したソースのライセンスのファイルに書かれたものです。

## 実行ファイル `kirimasim` に組み込むもの

| 部品 | 版 | ライセンス | 取得元 |
| --- | --- | --- | --- |
| slang | v12.0 の後の main（`b6c67a8b06c6b98fbc1eaeaf0d7bf734e1b86dfa`） | MIT | https://github.com/MikePopoloski/slang |
| {fmt}（slang が取得する） | 12.2.0 | MIT | https://github.com/fmtlib/fmt |
| mimalloc（slang が取得する） | v3.5.3 | MIT | https://github.com/microsoft/mimalloc |
| boost::regex（slang が取得する。slang の複製） | boost-1.91.0 | BSL-1.0 | https://github.com/MikePopoloski/regex |
| toml++（slang が取得する） | v3.4.0 | MIT | https://github.com/marzer/tomlplusplus |
| boost::unordered、boost の並行コンテナ（slang の `external/` に同梱） | その slang のコミットに同梱の版 | BSL-1.0 | https://github.com/MikePopoloski/slang |
| expected-lite（slang の `external/expected.hpp` に同梱） | その slang のコミットに同梱の版 | BSL-1.0 | https://github.com/MikePopoloski/slang |
| BS::thread_pool（slang の `external/` に同梱） | その slang のコミットに同梱の版 | MIT | https://github.com/MikePopoloski/slang |

slang が取得する部品の版は、その slang のコミットが自分の `external/CMakeLists.txt` で固定しているものです。`external/` は v12.0 と同じです。

## テストだけで使うもの

| 部品 | 版 | ライセンス | 取得元 |
| --- | --- | --- | --- |
| GoogleTest | v1.18.0（`063de7e9578f82b369302001269680b4b1553359`） | BSD-3-Clause | https://github.com/google/googletest |

## 使わないもの

slang のソースの木には、kirimasim がビルドも配布もしないファイルも含まれています。`external/ieee1800/` の下の IEEE 1800 の VPI と DPI のヘッダや、`docs/` の下の文書のデータなどです。
