# Third-party notices

kirimasim itself is licensed under the Apache License 2.0 with LLVM
Exceptions (see `LICENSE`). It does not include copies of third-party
sources in this repository. The components below are downloaded at configure
time by CMake `FetchContent`, pinned to the versions listed, and compiled
into kirimasim or its tests. Each component's license is the one stated in
the license file of the fetched sources.

## Compiled into the `kirimasim` executable

| Component | Version | License | Source |
| --- | --- | --- | --- |
| slang | v12.0 (`545f88b2ed3a2d20325fbd17413af177a6bca72f`) | MIT | https://github.com/MikePopoloski/slang |
| {fmt} (fetched by slang) | 12.2.0 | MIT | https://github.com/fmtlib/fmt |
| mimalloc (fetched by slang) | v3.5.3 | MIT | https://github.com/microsoft/mimalloc |
| boost::regex (fetched by slang, slang's mirror) | boost-1.91.0 | BSL-1.0 | https://github.com/MikePopoloski/regex |
| toml++ (fetched by slang) | v3.4.0 | MIT | https://github.com/marzer/tomlplusplus |
| boost::unordered, boost concurrent containers (vendored in slang's `external/`) | as vendored in slang v12.0 | BSL-1.0 | https://github.com/MikePopoloski/slang |
| expected-lite (vendored in slang's `external/expected.hpp`) | as vendored in slang v12.0 | BSL-1.0 | https://github.com/MikePopoloski/slang |
| BS::thread_pool (vendored in slang's `external/`) | as vendored in slang v12.0 | MIT | https://github.com/MikePopoloski/slang |

The versions of the components fetched by slang are the ones slang v12.0
pins in its own `external/CMakeLists.txt`.

## Used only by the tests

| Component | Version | License | Source |
| --- | --- | --- | --- |
| GoogleTest | v1.18.0 (`063de7e9578f82b369302001269680b4b1553359`) | BSD-3-Clause | https://github.com/google/googletest |

## Not used

slang's source tree also contains files that kirimasim does not build or
ship, such as the IEEE 1800 VPI and DPI headers under `external/ieee1800/`
and documentation data under `docs/`.
