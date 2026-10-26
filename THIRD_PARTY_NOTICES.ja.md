# 第三者のソフトウェアについての表示

[English](THIRD_PARTY_NOTICES.md) · 英語版が正本で、これはその日本語版です。食い違ったときは英語版に従います。

kirimasim 自体のライセンスは、Apache License 2.0 with LLVM Exceptions です（`LICENSE` を見てください）。このリポジトリには、第三者のソースの写しを含めていません。下の部品は、構成のときに CMake の `FetchContent` で取得し、挙げた版に固定して、kirimasim かそのテストにコンパイルして組み込みます。ライセンスと著作権の表示は、2026-10-21 に、取得したソースのライセンスのファイルとファイルの先頭の注記から読みました。ライセンスの全文は、このファイルの終わりにあります。FST設定とシステム圧縮ライブラリの表示は、別に2026-10-26に確認しました。

ビルドした kirimasim は、まだ公開していません。ビルドした実行ファイル `kirimasim` を配布する人は、このファイルを一緒に配ります。MIT License は、ソフトウェアのすべての複製と重要な部分に、著作権の表示と許諾の表示を含めることを求めているからです。Boost Software License 1.0 は、機械が実行するオブジェクトコードだけの複製には、これを求めていません。

## 実行ファイル `kirimasim` に組み込むもの

| 部品 | 版 | ライセンス | 著作権 | 取得元 |
| --- | --- | --- | --- | --- |
| slang | 12.1 のリリースの main（`16bd3b920dccfbcde000195f9d38d9b00432c9c8`） | MIT | Copyright (c) 2015-2026 Michael Popoloski | https://github.com/MikePopoloski/slang |
| {fmt}（slang が取得する） | 12.2.0（`1be298e1bd68957e4cd352e1f676f00e07dcfb57`） | MIT | Copyright (c) 2012 - present, Victor Zverovich and {fmt} contributors | https://github.com/fmtlib/fmt |
| mimalloc（slang が取得する） | v3.5.3（`d4881d338125e1cb7c47ba4cfb398d6f7c0c8d45`） | MIT | Copyright (c) 2018-2025 Microsoft Corporation, Daan Leijen | https://github.com/microsoft/mimalloc |
| toml++（slang が取得する） | v3.4.0（`30172438cee64926dc41fdd9c11fb3ba5b2ba9de`） | MIT | Copyright (c) Mark Gillard <mark.gillard@outlook.com.au> | https://github.com/marzer/tomlplusplus |
| BS::thread_pool（slang の `external/BS_thread_pool.hpp`。slang が手を加えている） | 5.1.0 | MIT | Copyright (c) 2021-2026 Barak Shoshany | https://github.com/bshoshany/thread-pool |
| Boost.Regex（slang が取得する。slang の複製） | boost-1.91.0（`2b3ac0834f31086c6e3c0e0ceb8516e427d5c39d`） | BSL-1.0 | John Maddock ほか。各ファイルの先頭に書かれている（注 1） | https://github.com/MikePopoloski/regex |
| Boost.Unordered と Boost の並行コンテナ（slang の `external/boost_unordered.hpp` と `external/boost_concurrent.hpp`） | Boost 1.91.0（slang のコミット `31d0107c8`） | BSL-1.0 | Peter Dimov、Joaquin M Lopez Munoz、Christian Mazakas、Braden Ganetsky、Daniel James ほか。ファイルの先頭に書かれている | https://github.com/MikePopoloski/slang |
| expected-lite（slang の `external/expected.hpp`） | 0.9.0 | BSL-1.0 | Copyright (C) 2016-2025 Martin Moene | https://github.com/nonstd-lite/expected-lite |

1. 取得した Boost.Regex の木には、それ自身のライセンスのファイルがありません。各ファイルの先頭に、Boost Software License 1.0 で配布すると書かれています。

slang が取得する部品の版は、その slang のコミットが自分の `external/CMakeLists.txt` で固定しているものです。slang の `external/` は v12.0 と同じです。slang は静的ライブラリとしてビルドされ、実行ファイルにリンクされます。2026-10-21 に、`kirimasim` の release ビルドを `nm` で調べ、上の部品それぞれのシンボルを見つけました。expected-lite だけはシンボルが見えませんが、slang のヘッダが取り込むので挙げています。BS::thread_pool の作者は、使うソフトウェアにそのリポジトリへのリンクを求めています。取得元の列がそのリンクです。

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

### Boost Software License 1.0

Boost.Regex、Boost.Unordered と Boost の並行コンテナ、expected-lite のものです。slang が `LICENSES/BSL-1.0.txt` に載せている文です。

```text
Boost Software License - Version 1.0 - August 17th, 2003

Permission is hereby granted, free of charge, to any person or organization obtaining a copy of the software and accompanying documentation covered by this license (the "Software") to use, reproduce, display, distribute, execute, and transmit the Software, and to prepare derivative works of the Software, and to permit third-parties to whom the Software is furnished to do so, all subject to the following:

The copyright notices in the Software and this entire statement, including the above license grant, this restriction and the following disclaimer, must be included in all copies of the Software, in whole or in part, and all derivative works of the Software, unless such copies or derivative works are solely in the form of machine-executable object code generated by a source language processor.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE, TITLE AND NON-INFRINGEMENT. IN NO EVENT SHALL THE COPYRIGHT HOLDERS OR ANYONE DISTRIBUTING THE SOFTWARE BE LIABLE FOR ANY DAMAGES OR OTHER LIABILITY, WHETHER IN CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
```

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

### System zlib

The upstream zlib v1.2.12 `README` copyright notice (the version found in the local SDK):

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

### System bzip2

The `LICENSE` from the official bzip2 1.0.8 source distribution:

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
