# FST dependency setup

[日本語](fst-dependency-setup.ja.md) · This English text is the main version.

Status: **dependency setup implemented (2026-10-26)**, requested by the user.
This records preparation for FST implementation, not approval of new waveform
semantics. The waveform backend is still unimplemented; `--wave=<file>.fst`
still reports a command-line error. The separate P1 implementation goal remains
paused.

## Dependency and build boundary

The build fetches the standalone [libfst repository](https://github.com/gtkwave/libfst)
at commit `17f6a5d27b2f576d65b550114fbde3c035cc74a8` (2026-10-18).
Its project metadata says version 1.0.0; this setup pins the commit and makes no
claim of a release tag. Its [official license at that commit](https://github.com/gtkwave/libfst/blob/17f6a5d27b2f576d65b550114fbde3c035cc74a8/LICENSE)
contains MIT terms for libfst, BSD-2-Clause terms for bundled LZ4 and MIT terms
for bundled FastLZ. Full notices are in [THIRD_PARTY_NOTICES](../../THIRD_PARTY_NOTICES.md).

`cmake/FstDependency.cmake` defines the static target `kirimasim::fstapi`;
kernel links it privately. No public kernel header exposes the FST API, and
this dependency adds no slang or LLVM requirement to kernel. No fetched source
is patched or vendored. The build does not fetch the GTKWave application or any
simulator runtime as an implementation reference.

The CMake wrapper follows upstream's [root build metadata](https://github.com/gtkwave/libfst/blob/17f6a5d27b2f576d65b550114fbde3c035cc74a8/meson.build)
and [source build metadata](https://github.com/gtkwave/libfst/blob/17f6a5d27b2f576d65b550114fbde3c035cc74a8/src/meson.build).
Only licensing, build metadata and API declarations were used for this setup;
runtime implementation bodies and upstream tests were not used as design
references. The smoke test is original kirimasim code.

- System zlib >= 1.2.0 and bzip2 >= 1.0.0 supply compression headers and
  libraries. Debian/Ubuntu needs `zlib1g-dev libbz2-dev`; macOS uses its
  Command Line Tools SDK. CMake reports missing development packages during
  configuration. These libraries are discovered, not pinned source downloads.
- The wrapper builds libfst's bundled LZ4 and FastLZ. It requires no separate
  LZ4 package, Meson installation, GTK or GLib, and does not build upstream tests.
- It probes `fseeko` and `realpath`, supplies `_GNU_SOURCE` and
  `FST_INCLUDE_CONFIG`, and generates a private `config.h` in the build tree.
- Background writer threads are disabled (`FST_WRITER_PARALLEL` and
  `HAVE_LIBPTHREAD` are undefined). Enabling them is separate future work.
- FastLZ alone gets `-fno-sanitize=alignment` when supported, matching the
  exception in upstream's build metadata. Other checks and kirimasim's own
  sanitizer instrumentation stay enabled.

The CI and nightly workflows prepare system compression packages before every
build configuration. Their FetchContent cache keys include all `cmake/*.cmake`
and `cmake/*.in` inputs, including the FST pin and generated-header template.

## Checks performed

On macOS arm64, Apple clang 21.0.0, CMake 4.4.4 and Ninja 1.13.2:

- Full dev build and CTest: 5,384 tests, 5,382 passed, zero failed, two skipped
  (`suite/cb/input_sample_1step`, `suite/nets/net_port_drivers_part`).
- `tools/fst_dependency` passes in dev, release, ASan and UBSan configurations.
  It writes files using zlib, FastLZ and LZ4 pack modes, then opens them with
  libfst's reader and checks variable count and time bounds. It compiles as
  C++20 without exceptions or RTTI. This checks dependency health, not decoded
  waveform values or interoperability with another waveform viewer.
- CMake found zlib 1.2.12 and bzip2 1.0.8 in the SDK. `otool -L` shows the
  corresponding system dynamic libraries; bundled compressor objects are
  static. The fetched libfst tree matches the pin and is unmodified.

Reproduce the full dev check with the normal README commands. For the focused
dependency check, repeat with `p` set to `dev`, `release`, `asan` and `ubsan`:

```sh
cmake --preset "$p"
cmake --build --preset "$p" --target kirimasim_fst_dependency
ctest --preset "$p" -R '^tools/fst_dependency$' --output-on-failure
```

Full release/ASan/UBSan suites and `tools/local_ci.py` were not run for this
setup; only the focused dependency target was built and tested in those three
configurations. Linux and hosted CI execution are **not confirmed**: run the
configured workflows after publication to check package installation, compiler
variants and the smoke test there. Actual waveform values, viewer compatibility,
dump-task integration and performance require the future FST backend and its
tests. Existing VCD behavior and existing expected values are unchanged.
