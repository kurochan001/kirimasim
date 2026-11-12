# 第三者のソフトウェアについての表示

[English](THIRD_PARTY_NOTICES.md) · 英語版が正本で、これはその日本語版です。食い違ったときは英語版に従います。

kirimasim 自体のライセンスは、Apache License 2.0 with LLVM Exceptions です（[正式な LICENSE](LICENSE) と[日本語参照訳](LICENSE.ja.md)を参照）。このリポジトリには、第三者のソースの写しを含めていません。下の部品は、構成のときに CMake の `FetchContent` で取得し、挙げた版に固定して、kirimasim かそのテストにコンパイルして組み込みます。ライセンスと著作権の表示は、2026-10-21 に、取得したソースのライセンスのファイルとファイルの先頭の注記から読みました。ライセンスの全文は、このファイルの終わりにあります。FST設定とシステム圧縮ライブラリの表示は、別に2026-10-26に確認しました。slangとmimallocの版・著作権表示・MIT本文は、選択した版の配布物で2026-11-03に再確認しました。

ビルドした kirimasim は、まだ公開していません。ビルドした実行ファイル `kirimasim` を配布する人は、このファイルを一緒に配ります。MIT License は、ソフトウェアのすべての複製と重要な部分に、著作権の表示と許諾の表示を含めることを求めているからです。Boost Software License 1.0 は、機械が実行するオブジェクトコードだけの複製には、これを求めていません。

## 実行ファイル `kirimasim` に組み込むもの

| 部品 | 版 | ライセンス | 著作権 | 取得元 |
| --- | --- | --- | --- | --- |
| slang | 12.1後のmaster（`62affeb02b95df96a69c36ee5a7c3d6dc8878554`） | MIT | Copyright (c) 2015-2026 Michael Popoloski | https://github.com/MikePopoloski/slang |
| {fmt}（slang が取得する） | 12.2.0（`1be298e1bd68957e4cd352e1f676f00e07dcfb57`） | MIT | Copyright (c) 2012 - present, Victor Zverovich and {fmt} contributors | https://github.com/fmtlib/fmt |
| mimalloc（slang が取得する） | v3.6.1（`56dc5eb80145f9a3ba7ca23d47c6a248b9d8586c`） | MIT | Copyright (c) 2018-2025 Microsoft Corporation, Daan Leijen | https://github.com/microsoft/mimalloc |
| toml++（slang が取得する） | v3.4.0（`30172438cee64926dc41fdd9c11fb3ba5b2ba9de`） | MIT | Copyright (c) Mark Gillard <mark.gillard@outlook.com.au> | https://github.com/marzer/tomlplusplus |
| BS::thread_pool（slang の `external/BS_thread_pool.hpp`。slang が手を加えている） | 5.1.0 | MIT | Copyright (c) 2021-2026 Barak Shoshany | https://github.com/bshoshany/thread-pool |
| Boost.Regex（slang が取得する。slang の複製） | boost-1.91.0（`2b3ac0834f31086c6e3c0e0ceb8516e427d5c39d`） | BSL-1.0 | John Maddock ほか。各ファイルの先頭に書かれている（注 1） | https://github.com/MikePopoloski/regex |
| Boost.Unordered と Boost の並行コンテナ（slang の `external/boost_unordered.hpp` と `external/boost_concurrent.hpp`） | Boost 1.91.0（slang のコミット `31d0107c8`） | BSL-1.0 | Peter Dimov、Joaquin M Lopez Munoz、Christian Mazakas、Braden Ganetsky、Daniel James ほか。ファイルの先頭に書かれている | https://github.com/MikePopoloski/slang |
| expected-lite（slang の `external/expected.hpp`） | 0.9.0 | BSL-1.0 | Copyright (C) 2016-2025 Martin Moene | https://github.com/nonstd-lite/expected-lite |

1. 取得した Boost.Regex の木には、それ自身のライセンスのファイルがありません。各ファイルの先頭に、Boost Software License 1.0 で配布すると書かれています。

slang が取得する部品の版は、その slang のコミットが自分の `external/CMakeLists.txt` で固定しているものです。選択した更新ではmimallocがv3.6.1になります。slang は静的ライブラリとしてビルドされ、実行ファイルにリンクされます。2026-10-21 に、`kirimasim` の release ビルドを `nm` で調べ、上の部品それぞれのシンボルを見つけました。expected-lite だけはシンボルが見えませんが、slang のヘッダが取り込むので挙げています。BS::thread_pool の作者は、使うソフトウェアにそのリポジトリへのリンクを求めています。取得元の列がそのリンクです。

kirimasim のビルドは、C と C++ の標準ライブラリを静的にリンクするオプションを付けていません。macOS では、`otool -L` で、実行ファイルがリンクするのはシステムの `libc++` と `libSystem` だけだと確かめました（2026-10-21）。Linux では未確認です（`ldd` で確かめられます）。これらのシステムのライブラリは kirimasim のビルドに含まれないので、挙げていません。

### slang のコメントが、よそから来たと書いているコード

slang の `REUSE.toml` は、これらのファイルを、slang の著作権と MIT ライセンスの下に置いています。出所が分かるように挙げます。それぞれに別の表示が要るかどうかは未確認です。

| slang の中の場所 | コメントに書かれていること | そのプロジェクトのライセンス |
| --- | --- | --- |
| `include/slang/util/Hash.h` | ハッシュは unordered_dense から取ったもので、wyhash を削って小さくしたもの | unordered_dense：MIT、Copyright (c) 2022 Martin Leitner-Ankerl。wyhash：パブリックドメインとして公開。どちらも 2026-10-21 に各プロジェクトの既定の枝で読んだ |
| `include/slang/text/CharInfo.h` | Christopher Wellons による、パブリックドメインの分岐のない UTF-8 デコーダ | パブリックドメインとして公開（その `UNLICENSE`。2026-10-21 に読んだ） |
| `include/slang/util/Function.h`、`include/slang/util/IntervalMap.h`、`source/numeric/SVIntHelpers.h` | LLVM プロジェクトのコードに基づく、またはそれに着想を得たもの | 今の LLVM は Apache-2.0 WITH LLVM-exception。slang がどの版から取ったかは未確認。ライセンスを変える前の LLVM のコードは、University of Illinois/NCSA のライセンスだった |

## FST対応に向けて準備したもの

2026-10-26の依存設定の依頼に沿い、独立したlibfstの静的ターゲットをkernelから私有の依存として
リンクする。APIのsmokeテストでは使うが、kirimasimの波形backendはまだ呼ばない。
以下の完全なcommitで取得し、GTKWave・GLib・上流のテスト・他シミュレータのruntimeは取り込まない。
GitHubの複合ライセンス表示から推測せず、固定commitの公式 `LICENSE` を確認した。
上流の表示全文は末尾に載せる。

| 部品 | 版 | ライセンス | 著作権 | 取得元 |
| --- | --- | --- | --- | --- |
| libfst | project version 1.0.0、commit `17f6a5d27b2f576d65b550114fbde3c035cc74a8`（release tagは主張しない） | MIT | Copyright (c) 2009-2025 Tony Bybell | https://github.com/gtkwave/libfst |
| 同梱LZ4 | 同じlibfst commit。個別の版番号は主張しない | BSD-2-Clause | Copyright (c) 2011-2020, Yann Collet | libfstの `LICENSE` |
| 同梱FastLZ | 同じlibfst commit。個別の版番号は主張しない | MIT | Copyright (C) 2005-2007 Ariya Hidayat | libfstの `LICENSE` |

システムの開発用パッケージでzlib（最小1.2.0、Zlibライセンス）とbzip2（最小1.0.0、bzip2ライセンス）を
用意する。手元のmacOS SDKではCMakeがそれぞれ1.2.12・1.0.8を検出した。他環境はその環境の版を使い、
本プロジェクトではダウンロード・同梱しない。2026-10-26の `otool -L` では、準備後の実行ファイルに `/usr/lib/libz.1.dylib` と `/usr/lib/libbz2.1.0.dylib` も見える。zlibの公式v1.2.12 READMEと、sourceware.orgの公式bzip2 1.0.8
配布物から取得した表示を末尾に再録する。異なる版や静的リンクしたシステムライブラリも配布する場合には、
その版の表示を保持する。

## テストだけで使うもの

単体テスト（`kirimasim_unit_tests`）にだけリンクし、`kirimasim` にはリンクしません。

| 部品 | 版 | ライセンス | 著作権 | 取得元 |
| --- | --- | --- | --- | --- |
| GoogleTest | v1.18.0（`063de7e9578f82b369302001269680b4b1553359`） | BSD-3-Clause | Copyright 2008, Google Inc. | https://github.com/google/googletest |

## CI だけが取得したり実行したりするもの

どれも、このリポジトリにも実行ファイルにも含まれません。nightly の実行（`.github/workflows/nightly.yml`）が、比べるために取得します。

| 部品 | 版 | ライセンス | 使い方 |
| --- | --- | --- | --- |
| sv-tests | コミット `15490e95fd978ada1c1dc59b7c4b53a7edb23b78` | ISC、Copyright (C) 2020 The Symbiflow Authors（その `LICENSE`） | そのテストを実行する。そのファイルは、このリポジトリに写していない |
| UVM（sv-tests の submodule `third_party/tests/uvm`） | https://github.com/SymbiFlow/uvm のコミット `656f20d087370a7c742e00188d20bbf30fa95339` | Apache-2.0（その `LICENSE.txt`） | sv-tests と一緒に取得し、その UVM のテストで使う |
| Icarus Verilog | コミット `04cc493506bd4d75c861cfc9961e2a56517d7cc6` | GPL（GitHub は `COPYING` を GPL-2.0 と判定している。正確な条件は未確認） | `tools/build_icarus.sh` でビルドし、結果を比べるために外部のコマンドとして実行する。ソースは読まない（`CLAUDE.md`） |

CI のランナーに入れるツール（コンパイラ、clang-tidy、bison など）と、ワークフローが使う GitHub Actions は挙げていません。

## 使わないもの

slang のソースの木には、kirimasim がビルドも配布もしないファイルも含まれています。`external/ieee1800/` の下の IEEE 1800 の VPI と DPI のヘッダや、`docs/` の下の文書のデータなどです（slang の `REUSE.toml` は、`docs/cppreference-doxygen-web.tag.xml` を CC-BY-SA-3.0 としています）。slang は Catch2 を自分のテストのためだけに取得し、kirimasim はそのテストを切っています（`SLANG_INCLUDE_TESTS`）。

## ライセンスの全文

正式な英語本文をそのまま保ち、それぞれに全文の日本語参照訳を添えます。
参照訳は原文の許諾・表示要件を置き換えません。

### MIT License

slang、{fmt}、mimalloc、toml++、BS::thread_pool のものです。それぞれのライセンスのファイルは、自分の著作権の行（上の表）に続けて、この文をそのまま載せています。ファイルどうしの違いは改行の位置だけで、mimalloc と toml++ のファイルは「MIT License」という題で始まります。

```text
Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in
all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
THE SOFTWARE.
```

**日本語参照訳（非公式。正式な表示は上の原文）：**

このソフトウェアと付属文書（本ソフトウェア）の写しを取得するすべての人に、無償で、制限なく扱う権利を許諾します。
使用、複製、変更、結合、公開、頒布、再許諾、販売、および提供先に同じことを許す権利を含みますが、それらに限りません。
条件として、上の著作権表示とこの許諾表示を、本ソフトウェアのすべての写しまたは重要な部分に含めます。

本ソフトウェアは現状のまま、明示・黙示のいかなる保証もなく提供します。商品性、特定目的への適合性、非侵害の保証を
含みますが、それらに限りません。著作者・著作権者は、本ソフトウェア、その使用、その他の取扱いに起因・関連する
契約、不法行為その他による請求、損害、その他の責任を一切負いません。

### Boost Software License 1.0

Boost.Regex、Boost.Unordered と Boost の並行コンテナ、expected-lite のものです。slang が `LICENSES/BSL-1.0.txt` に載せている文です。

```text
Boost Software License - Version 1.0 - August 17th, 2003

Permission is hereby granted, free of charge, to any person or organization obtaining a copy of the software and accompanying documentation covered by this license (the "Software") to use, reproduce, display, distribute, execute, and transmit the Software, and to prepare derivative works of the Software, and to permit third-parties to whom the Software is furnished to do so, all subject to the following:

The copyright notices in the Software and this entire statement, including the above license grant, this restriction and the following disclaimer, must be included in all copies of the Software, in whole or in part, and all derivative works of the Software, unless such copies or derivative works are solely in the form of machine-executable object code generated by a source language processor.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE, TITLE AND NON-INFRINGEMENT. IN NO EVENT SHALL THE COPYRIGHT HOLDERS OR ANYONE DISTRIBUTING THE SOFTWARE BE LIABLE FOR ANY DAMAGES OR OTHER LIABILITY, WHETHER IN CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
```

**日本語参照訳（非公式。正式な表示は上の原文）：**

Boost Software License、Version 1.0、2003 年 8 月 17 日。

本ライセンスの対象のソフトウェアと付属文書（本ソフトウェア）の写しを取得する個人・組織に、無償で、使用、複製、表示、
頒布、実行、伝達、派生著作物の作成、および提供先の第三者にそれを許す権利を与えます。ただし、次の条件に従います。

本ソフトウェアの著作権表示と、この許諾・制限・以下の免責を含む全文を、本ソフトウェアの全部・一部のすべての写しと
すべての派生著作物へ含めます。ただし、ソース言語処理系が生成した機械実行可能なオブジェクトコードだけの場合は除きます。

本ソフトウェアは現状のまま、明示・黙示の保証なしで提供します。商品性、特定目的への適合性、権原、非侵害を含みますが、
それらに限りません。著作権者および頒布者は、本ソフトウェア、その使用、その他の取扱いに起因・関連する契約、
不法行為その他による損害・責任を一切負いません。

### BSD 3-Clause License

テストだけで使う GoogleTest のものです。その `LICENSE` ファイルです。

```text
Copyright 2008, Google Inc.
All rights reserved.

Redistribution and use in source and binary forms, with or without
modification, are permitted provided that the following conditions are
met:

    * Redistributions of source code must retain the above copyright
notice, this list of conditions and the following disclaimer.
    * Redistributions in binary form must reproduce the above
copyright notice, this list of conditions and the following disclaimer
in the documentation and/or other materials provided with the
distribution.
    * Neither the name of Google Inc. nor the names of its
contributors may be used to endorse or promote products derived from
this software without specific prior written permission.

THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
"AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
(INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
```

**日本語参照訳（非公式。正式な表示は上の原文）：**

Copyright 2008, Google Inc. すべての権利を留保します。

変更の有無によらず、次の条件を満たせばソース形式・バイナリ形式で再頒布・使用できます。

1. ソースの再頒布には、上の著作権表示、この条件一覧、以下の免責を残します。
2. バイナリの再頒布には、頒布に付属する文書・その他の資料で同じ著作権表示・条件一覧・免責を再現します。
3. 事前の書面による個別の許可なく、Google Inc. や貢献者の名前を、派生製品の推奨・販売促進に使ってはいけません。

本ソフトウェアは著作権者・貢献者が現状のまま提供し、商品性・特定目的への適合性など、明示・黙示の保証を
すべて否認します。本ソフトウェアの使用から生じる直接・間接・付随・特別・懲罰的・結果的損害（代替品・サービスの調達、
使用・データ・利益の喪失、事業中断など）について、契約、厳格責任、不法行為（過失など）その他の責任理論によらず、
原因のいかんを問わず、その可能性を知らされていても、著作権者・貢献者は責任を負いません。

### libfst, bundled LZ4 and FastLZ

Official `LICENSE` at libfst commit `17f6a5d27b2f576d65b550114fbde3c035cc74a8`:

```text
MIT License

Copyright (c) 2009-2025 Tony Bybell

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all
copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
SOFTWARE.

=============================================================================

Parts of libfst are based on these libraries:

-----------------------------------------------------------------------------

LZ4 Library
Copyright (c) 2011-2020, Yann Collet
All rights reserved.

Redistribution and use in source and binary forms, with or without modification,
are permitted provided that the following conditions are met:

* Redistributions of source code must retain the above copyright notice, this
  list of conditions and the following disclaimer.

* Redistributions in binary form must reproduce the above copyright notice, this
  list of conditions and the following disclaimer in the documentation and/or
  other materials provided with the distribution.

THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND
ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE FOR
ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
(INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON
ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
(INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

-----------------------------------------------------------------------------

FastLZ - lightning-fast lossless compression library

Copyright (C) 2007 Ariya Hidayat (ariya@kde.org)
Copyright (C) 2006 Ariya Hidayat (ariya@kde.org)
Copyright (C) 2005 Ariya Hidayat (ariya@kde.org)

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in
all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
THE SOFTWARE.
```

**日本語参照訳（非公式。正式な表示は上の原文）：**

MIT License。Copyright (c) 2009-2025 Tony Bybell。

このソフトウェアと付属文書（本ソフトウェア）の写しを取得するすべての人に、無償で、制限なく扱う権利を許諾します。
使用、複製、変更、結合、公開、頒布、再許諾、販売、および提供先に同じことを許す権利を含みますが、それらに限りません。
条件として、上の著作権表示とこの許諾表示を、本ソフトウェアのすべての写しまたは重要な部分に含めます。

本ソフトウェアは現状のまま、明示・黙示のいかなる保証もなく提供します。商品性、特定目的への適合性、非侵害の保証を
含みますが、それらに限りません。著作者・著作権者は、本ソフトウェア、その使用、その他の取扱いに起因・関連する
契約、不法行為その他による請求、損害、その他の責任を一切負いません。

libfst の一部は次のライブラリに基づきます。

**LZ4 Library**：Copyright (c) 2011-2020, Yann Collet。すべての権利を留保します。
変更の有無によらず、次の条件を満たせばソース・バイナリの再頒布・使用を認めます。

1. ソースの再頒布に著作権表示、この条件一覧、以下の免責を残します。
2. バイナリの再頒布に付属する文書・資料に、著作権表示、この条件一覧、以下の免責を再現します。

本ソフトウェアは著作権者・貢献者が現状のまま提供し、商品性・特定目的への適合性など、明示・黙示の保証を
すべて否認します。本ソフトウェアの使用から生じる直接・間接・付随・特別・懲罰的・結果的損害（代替品・サービスの調達、
使用・データ・利益の喪失、事業中断など）について、契約、厳格責任、不法行為（過失など）その他の責任理論によらず、
原因のいかんを問わず、その可能性を知らされていても、著作権者・貢献者は責任を負いません。

**FastLZ**：非常に速い可逆圧縮ライブラリ。
Copyright (C) 2007 Ariya Hidayat、Copyright (C) 2006 Ariya Hidayat、Copyright (C) 2005 Ariya Hidayat。
原文の連絡先は上の正式な表示に残しています。

このソフトウェアと付属文書（本ソフトウェア）の写しを取得するすべての人に、無償で、制限なく扱う権利を許諾します。
使用、複製、変更、結合、公開、頒布、再許諾、販売、および提供先に同じことを許す権利を含みますが、それらに限りません。
条件として、上の著作権表示とこの許諾表示を、本ソフトウェアのすべての写しまたは重要な部分に含めます。

本ソフトウェアは現状のまま、明示・黙示のいかなる保証もなく提供します。商品性、特定目的への適合性、非侵害の保証を
含みますが、それらに限りません。著作者・著作権者は、本ソフトウェア、その使用、その他の取扱いに起因・関連する
契約、不法行為その他による請求、損害、その他の責任を一切負いません。

### System zlib

ローカル SDK で見つかった版、上流 zlib v1.2.12 の `README` の著作権表示：

```text
(C) 1995-2022 Jean-loup Gailly and Mark Adler

  This software is provided 'as-is', without any express or implied
  warranty.  In no event will the authors be held liable for any damages
  arising from the use of this software.

  Permission is granted to anyone to use this software for any purpose,
  including commercial applications, and to alter it and redistribute it
  freely, subject to the following restrictions:

  1. The origin of this software must not be misrepresented; you must not
     claim that you wrote the original software. If you use this software
     in a product, an acknowledgment in the product documentation would be
     appreciated but is not required.
  2. Altered source versions must be plainly marked as such, and must not be
     misrepresented as being the original software.
  3. This notice may not be removed or altered from any source distribution.

  Jean-loup Gailly        Mark Adler
  jloup@gzip.org          madler@alumni.caltech.edu
```

**日本語参照訳（非公式。正式な表示は上の原文）：**

(C) 1995-2022 Jean-loup Gailly and Mark Adler。

本ソフトウェアは現状のまま、明示・黙示の保証なしで提供します。著作者は、その使用によるいかなる損害にも責任を負いません。
商用を含む任意の目的の使用、変更、自由な再頒布を、次の制限に従って認めます。

1. 出所を偽らず、元のソフトウェアを自分が書いたと主張してはいけません。製品で使う場合、製品文書での謝辞は歓迎しますが必須ではありません。
2. 変更したソース版は変更版と明示し、元のソフトウェアと偽ってはいけません。
3. ソースの頒布からこの表示を削除・変更してはいけません。

Jean-loup Gailly と Mark Adler の連絡先は上の原文にあります。

### System bzip2

公式 bzip2 1.0.8 ソース配布物の `LICENSE`：

```text

--------------------------------------------------------------------------

This program, "bzip2", the associated library "libbzip2", and all
documentation, are copyright (C) 1996-2019 Julian R Seward.  All
rights reserved.

Redistribution and use in source and binary forms, with or without
modification, are permitted provided that the following conditions
are met:

1. Redistributions of source code must retain the above copyright
   notice, this list of conditions and the following disclaimer.

2. The origin of this software must not be misrepresented; you must
   not claim that you wrote the original software.  If you use this
   software in a product, an acknowledgment in the product
   documentation would be appreciated but is not required.

3. Altered source versions must be plainly marked as such, and must
   not be misrepresented as being the original software.

4. The name of the author may not be used to endorse or promote
   products derived from this software without specific prior written
   permission.

THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS
OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY
DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE
GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING
NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

Julian Seward, jseward@acm.org
bzip2/libbzip2 version 1.0.8 of 13 July 2019

--------------------------------------------------------------------------
```

**日本語参照訳（非公式。正式な表示は上の原文）：**

本プログラム bzip2、付属ライブラリ libbzip2、すべての文書は Copyright (C) 1996-2019 Julian R Seward。
すべての権利を留保します。

変更の有無によらず、次の条件を満たせばソース・バイナリ形式で再頒布・使用できます。

1. ソースの再頒布に、上の著作権表示、この条件一覧、以下の免責を残します。
2. 出所を偽らず、元のソフトウェアを自分が書いたと主張してはいけません。製品での使用に際する文書の謝辞は歓迎しますが必須ではありません。
3. 変更したソース版を変更版と明示し、元のソフトウェアと偽ってはいけません。
4. 事前の書面による個別許可なく、著作者の名を派生製品の推奨・販売促進に使ってはいけません。

本ソフトウェアは著作者が現状のまま提供し、商品性・特定目的への適合性など、明示・黙示の保証をすべて否認します。
使用に起因する直接・間接・付随・特別・懲罰的・結果的損害（代替品・サービスの調達、使用・データ・利益の喪失、
事業中断など）について、原因のいかん、契約・厳格責任・不法行為（過失など）その他の責任理論によらず、
その可能性を知らされていても著作者は責任を負いません。

Julian Seward。連絡先は原文にあります。bzip2/libbzip2 version 1.0.8、2019 年 7 月 13 日。
