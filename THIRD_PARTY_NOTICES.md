# Third-party notices

[日本語](THIRD_PARTY_NOTICES.ja.md) · This English text is the main version; the Japanese version follows it, and where they differ, this text holds.

kirimasim itself is licensed under the Apache License 2.0 with LLVM
Exceptions (see `LICENSE`). This repository holds no copies of third-party
sources: the fetched components below are downloaded at configure time by CMake
`FetchContent`, pinned to the versions listed, and compiled into kirimasim
or its tests. Their licenses and copyright notices were read from the
license files and file headers of the fetched sources on 2026-10-21. The
full texts of the licenses are at the end of this file. FST setup and system
compression-library notices were checked separately on 2026-10-26. The slang
and mimalloc versions, copyright notices and MIT texts were rechecked from
their selected source distributions on 2026-10-27.

No built kirimasim has been released yet. Whoever distributes a built
`kirimasim` executable includes this file with it: the MIT License asks
that its copyright notice and permission notice be included in all copies
or substantial portions of the software. The Boost Software License 1.0
does not ask this of copies that are solely machine-executable object code.

## Compiled into the `kirimasim` executable

| Component | Version | License | Copyright | Source |
| --- | --- | --- | --- | --- |
| slang | master after 12.1 (`e2d1021661f78b3f17d1edf7ea17f9d6f95fb168`) | MIT | Copyright (c) 2015-2026 Michael Popoloski | https://github.com/MikePopoloski/slang |
| {fmt} (fetched by slang) | 12.2.0 (`1be298e1bd68957e4cd352e1f676f00e07dcfb57`) | MIT | Copyright (c) 2012 - present, Victor Zverovich and {fmt} contributors | https://github.com/fmtlib/fmt |
| mimalloc (fetched by slang) | v3.6.0 (`b18ab4eef383f655e760881b02eff8af02800879`) | MIT | Copyright (c) 2018-2025 Microsoft Corporation, Daan Leijen | https://github.com/microsoft/mimalloc |
| toml++ (fetched by slang) | v3.4.0 (`30172438cee64926dc41fdd9c11fb3ba5b2ba9de`) | MIT | Copyright (c) Mark Gillard <mark.gillard@outlook.com.au> | https://github.com/marzer/tomlplusplus |
| BS::thread_pool (in slang's `external/BS_thread_pool.hpp`, with slang's changes) | 5.1.0 | MIT | Copyright (c) 2021-2026 Barak Shoshany | https://github.com/bshoshany/thread-pool |
| Boost.Regex (fetched by slang, from slang's copy) | boost-1.91.0 (`2b3ac0834f31086c6e3c0e0ceb8516e427d5c39d`) | BSL-1.0 | John Maddock and others, in the headers of the files (note 1) | https://github.com/MikePopoloski/regex |
| Boost.Unordered and Boost's concurrent containers (in slang's `external/boost_unordered.hpp` and `external/boost_concurrent.hpp`) | Boost 1.91.0 (slang commit `31d0107c8`) | BSL-1.0 | Peter Dimov, Joaquin M Lopez Munoz, Christian Mazakas, Braden Ganetsky, Daniel James and others, in the headers of the files | https://github.com/MikePopoloski/slang |
| expected-lite (in slang's `external/expected.hpp`) | 0.9.0 | BSL-1.0 | Copyright (C) 2016-2025 Martin Moene | https://github.com/nonstd-lite/expected-lite |

1. The fetched tree of Boost.Regex has no license file of its own; the
   headers of its files say that they are distributed under the Boost
   Software License 1.0.

The versions of the components fetched by slang are the ones that slang
commit pins in its own `external/CMakeLists.txt`; the selected update raises
mimalloc to v3.6.0. slang is built as a static library and linked into the
executable; on 2026-10-21 `nm` found symbols of each component above in the
release build of `kirimasim`, except expected-lite, which is listed because
slang's headers include it. The author of BS::thread_pool asks software that
uses it to link to its repository; the Source column does.

kirimasim's build adds no option to link the C or C++ standard library
statically. On macOS, `otool -L` shows that the executable links only the
system's `libc++` and `libSystem` (checked 2026-10-21); on Linux this is not
confirmed (check with `ldd`). These system libraries are not part of a
kirimasim build and are not listed.

### Code that slang's comments say came from elsewhere

slang's `REUSE.toml` puts these files under slang's MIT license, with
slang's copyright. They are listed so that their origin is known. Whether
they need notices of their own is not confirmed.

| Place in slang | What its comment says | License of that project |
| --- | --- | --- |
| `include/slang/util/Hash.h` | The hashing is taken from unordered_dense, and is a stripped-down wyhash | unordered_dense: MIT, Copyright (c) 2022 Martin Leitner-Ankerl. wyhash: released into the public domain. Both read from the projects' default branches on 2026-10-21 |
| `include/slang/text/CharInfo.h` | A public-domain branchless UTF-8 decoder by Christopher Wellons | Released into the public domain (its `UNLICENSE`, read on 2026-10-21) |
| `include/slang/util/Function.h`, `include/slang/util/IntervalMap.h`, `source/numeric/SVIntHelpers.h` | Based on, or inspired by, code of the LLVM project | LLVM today: Apache-2.0 WITH LLVM-exception. Which version slang drew from is not confirmed; LLVM's code from before its relicensing was under the University of Illinois/NCSA license |

## Prepared for FST support

The 2026-10-26 dependency-setup request adds the standalone libfst static target,
privately linked by kernel. Its own API smoke test is a consumer; kirimasim's
waveform backend does not call it yet. It is fetched at the full commit below,
without GTKWave, GLib, upstream tests or simulator runtime sources. Licenses were
checked from the pinned upstream `LICENSE`, not inferred from GitHub's combined
license badge. The full upstream notice follows below.

| Component | Version | License | Copyright | Source |
| --- | --- | --- | --- | --- |
| libfst | project version 1.0.0; commit `17f6a5d27b2f576d65b550114fbde3c035cc74a8` (no release tag asserted) | MIT | Copyright (c) 2009-2025 Tony Bybell | https://github.com/gtkwave/libfst |
| Bundled LZ4 | same libfst commit; no separate version asserted | BSD-2-Clause | Copyright (c) 2011-2020, Yann Collet | libfst's `LICENSE` |
| Bundled FastLZ | same libfst commit; no separate version asserted | MIT | Copyright (C) 2005-2007 Ariya Hidayat | libfst's `LICENSE` |

System development packages supply zlib (minimum 1.2.0, Zlib license) and bzip2
(minimum 1.0.0, bzip2 license). CMake found headers/libraries 1.2.12 and 1.0.8,
respectively, in the local macOS SDK; other systems use their installed versions.
The exact libraries are not downloaded or vendored by this project. On
2026-10-26, `otool -L` also lists `/usr/lib/libz.1.dylib` and
`/usr/lib/libbz2.1.0.dylib` for the prepared executable; the earlier
2026-10-21 standard-library-only observation predates this setup. Their official
notices were obtained from zlib's v1.2.12 README and the sourceware.org bzip2 1.0.8
distribution, and reproduced below. A distributor that supplies different or
statically linked system libraries must retain those versions' own notices.

## Used only by the tests

These are linked only into the unit tests (`kirimasim_unit_tests`), never
into `kirimasim`.

| Component | Version | License | Copyright | Source |
| --- | --- | --- | --- | --- |
| GoogleTest | v1.18.0 (`063de7e9578f82b369302001269680b4b1553359`) | BSD-3-Clause | Copyright 2008, Google Inc. | https://github.com/google/googletest |

## Fetched or run only by CI

None of these is in this repository or in the executable. The nightly run
(`.github/workflows/nightly.yml`) fetches them for its comparisons.

| Component | Version | License | Use |
| --- | --- | --- | --- |
| sv-tests | commit `15490e95fd978ada1c1dc59b7c4b53a7edb23b78` | ISC, Copyright (C) 2020 The Symbiflow Authors (its `LICENSE`) | Its tests are run; none of its files are copied into this repository |
| UVM, the sv-tests submodule `third_party/tests/uvm` | commit `656f20d087370a7c742e00188d20bbf30fa95339` of https://github.com/SymbiFlow/uvm | Apache-2.0 (its `LICENSE.txt`) | Fetched with sv-tests, for its UVM tests |
| Icarus Verilog | commit `04cc493506bd4d75c861cfc9961e2a56517d7cc6` | GPL (GitHub reports GPL-2.0 for its `COPYING`; the exact terms are not confirmed) | Built by `tools/build_icarus.sh` and run as an external command to compare results. Its source is not read (`CLAUDE.md`) |

The tools installed on the CI runners (compilers, clang-tidy, bison and the
like) and the GitHub Actions that the workflows use are not listed.

## Not used

slang's source tree also contains files that kirimasim neither builds nor
ships, such as the IEEE 1800 VPI and DPI headers under `external/ieee1800/`
and documentation data under `docs/` (slang's `REUSE.toml` gives
`docs/cppreference-doxygen-web.tag.xml` as CC-BY-SA-3.0). slang fetches
Catch2 only for its own tests, which kirimasim turns off
(`SLANG_INCLUDE_TESTS`).

## License texts

### MIT License

slang, {fmt}, mimalloc, toml++ and BS::thread_pool. Each one's license
file is its own copyright line (in the tables above) followed by this text,
word for word; the files differ only in their line breaks, and those of
mimalloc and toml++ begin with the title "MIT License".

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

Boost.Regex, Boost.Unordered with Boost's concurrent containers, and
expected-lite. The text as slang gives it in `LICENSES/BSL-1.0.txt`:

```text
Boost Software License - Version 1.0 - August 17th, 2003

Permission is hereby granted, free of charge, to any person or organization obtaining a copy of the software and accompanying documentation covered by this license (the "Software") to use, reproduce, display, distribute, execute, and transmit the Software, and to prepare derivative works of the Software, and to permit third-parties to whom the Software is furnished to do so, all subject to the following:

The copyright notices in the Software and this entire statement, including the above license grant, this restriction and the following disclaimer, must be included in all copies of the Software, in whole or in part, and all derivative works of the Software, unless such copies or derivative works are solely in the form of machine-executable object code generated by a source language processor.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE, TITLE AND NON-INFRINGEMENT. IN NO EVENT SHALL THE COPYRIGHT HOLDERS OR ANYONE DISTRIBUTING THE SOFTWARE BE LIABLE FOR ANY DAMAGES OR OTHER LIABILITY, WHETHER IN CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
```

### BSD 3-Clause License

GoogleTest, used only by the tests. Its `LICENSE` file:

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
