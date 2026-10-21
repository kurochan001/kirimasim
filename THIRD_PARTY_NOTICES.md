# Third-party notices

[日本語](THIRD_PARTY_NOTICES.ja.md) · This English text is the main version; the Japanese version follows it, and where they differ, this text holds.

kirimasim itself is licensed under the Apache License 2.0 with LLVM
Exceptions (see `LICENSE`). It does not include copies of third-party
sources in this repository. The components below are downloaded at configure
time by CMake `FetchContent`, pinned to the versions listed, and compiled
into kirimasim or its tests. Each component's license is the one stated in
the license file of the fetched sources.

## Compiled into the `kirimasim` executable

| Component | Version | License | Source |
| --- | --- | --- | --- |
| slang | main after v12.0 (`b6c67a8b06c6b98fbc1eaeaf0d7bf734e1b86dfa`) | MIT | https://github.com/MikePopoloski/slang |
| {fmt} (fetched by slang) | 12.2.0 | MIT | https://github.com/fmtlib/fmt |
| mimalloc (fetched by slang) | v3.5.3 | MIT | https://github.com/microsoft/mimalloc |
| boost::regex (fetched by slang, slang's mirror) | boost-1.91.0 | BSL-1.0 | https://github.com/MikePopoloski/regex |
| toml++ (fetched by slang) | v3.4.0 | MIT | https://github.com/marzer/tomlplusplus |
| boost::unordered, boost concurrent containers (vendored in slang's `external/`) | as vendored in that slang commit | BSL-1.0 | https://github.com/MikePopoloski/slang |
| expected-lite (vendored in slang's `external/expected.hpp`) | as vendored in that slang commit | BSL-1.0 | https://github.com/MikePopoloski/slang |
| BS::thread_pool (vendored in slang's `external/`) | as vendored in that slang commit | MIT | https://github.com/MikePopoloski/slang |

The versions of the components fetched by slang are the ones that slang
commit pins in its own `external/CMakeLists.txt`; `external/` is the same
as in v12.0.

## Used only by the tests

| Component | Version | License | Source |
| --- | --- | --- | --- |
| GoogleTest | v1.18.0 (`063de7e9578f82b369302001269680b4b1553359`) | BSD-3-Clause | https://github.com/google/googletest |

## Not used

slang's source tree also contains files that kirimasim does not build or
ship, such as the IEEE 1800 VPI and DPI headers under `external/ieee1800/`
and documentation data under `docs/`.
