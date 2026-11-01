# kirimasim

[日本語](README.ja.md)

[User manual](docs/USER_MANUAL.md) · [Reference manual](docs/REFERENCE_MANUAL.md) · [Documentation guide](docs/README.md)

[![CI](https://github.com/kurochan001/kirimasim/actions/workflows/ci.yml/badge.svg?branch=main)](https://github.com/kurochan001/kirimasim/actions/workflows/ci.yml?query=branch%3Amain)
[![Nightly](https://github.com/kurochan001/kirimasim/actions/workflows/nightly.yml/badge.svg?branch=main)](https://github.com/kurochan001/kirimasim/actions/workflows/nightly.yml?query=branch%3Amain)
[![C++23](https://img.shields.io/badge/C%2B%2B-23-00599C?logo=cplusplus&logoColor=white)](#building)
[![Linux](https://img.shields.io/badge/OS-Linux-FCC624?logo=linux&logoColor=black)](#building)
[![macOS](https://img.shields.io/badge/OS-macOS-000000?logo=apple&logoColor=white)](#building)
[![License: Apache-2.0 WITH LLVM-exception](https://img.shields.io/badge/license-Apache--2.0%20WITH%20LLVM--exception-blue)](LICENSE)

kirimasim is an open-source SystemVerilog simulator in early development.
It aims to preserve four-state values (0, 1, X, Z) by default and the full
IEEE 1800-2023 time-slot scheduling algorithm, while pursuing practical speed.
AI assists implementation; people approve specifications, test expectations
and reviews under the [AI policy](docs/spec/AI_POLICY.md).

## Current status

<a id="status-2026-11-01"></a>

**As of 2026-11-01, P1 is in progress; there is no release yet.** The first
execution tier directly interprets SIR. It runs a broad RTL and simple-testbench
subset, including the documented class-object subset. It is not ready for
production projects. C++ generation is planned for P3.

The front end is unmodified slang, pinned at
[`e2d1021661f78b3f17d1edf7ea17f9d6f95fb168`](cmake/Dependencies.cmake),
a development commit after 12.1. Accepted syntax can still lack executable
lowering; unsupported forms generally stop with a source-position diagnostic.

### What works

The [P1 support inventory](docs/P1_SUPPORT.md) gives exact type, lifetime,
selection and execution-context limits. [P1 Features](docs/P1_FEATURES.md)
provides runnable examples.

- **RTL and values:** modules, parameters, generate, interfaces/modports,
  packages, arbitrary-width two-/four-state integers, real/string values,
  fixed arrays and structures, and the supported dynamic-array/queue subset.
- **Processes:** initial/always/final, comb/latch/ff procedures, delays,
  guarded signal/expression events, NBA/event-NBA, automatic captures,
  fork joins and named-fork disable, calls/refs and programs in the reactive set.
- **Classes:** staged construction, typed fields, inheritance, direct/virtual
  methods, waiting tasks, shallow copies and casts within the
  [source subset](docs/notes/class-source-execution.md); non-parameterized
  static properties use shared storage. Objects also coexist with
  [ordinary signal/event observers](docs/notes/class-independent-observers.md).
- **Nets:** strengths, multiple-driver resolution, gates, wide UDPs, aliases,
  all six directed MOS and six passive switch kinds on supported static
  terminals, and whole fixed-array ports. Eligible static input/output
  partial ports preserve physical strength and effective wired/pull/supply
  kinds; integral input/output conversions use directed value carriers. Eligible
  static partial inout ports share identity without value carriers.
- **Tasks and output:** typed `%p`/`%0p` formatting for supported values,
  immediate/deferred assertions, display and runtime
  formatting/scanning, file and memory I/O, random functions, plusargs,
  VCD including interface/program scopes, statistics and resource limits.

### What does not work yet

<a id="remaining-p1-functionality-2026-10-26"></a>
<a id="remaining-p1-functionality-2026-10-29"></a>
<a id="remaining-p1-functionality-2026-10-30"></a>
<a id="remaining-p1-functionality-2026-11-01"></a>

#### Remaining P1 functionality

P1's outstanding coverage criterion is **standard execution functionality at
least as broad as Icarus Verilog 13.0**, including necessary mechanisms brought
forward from later phases, with the approved optional `wire real` exception below.
It is not a test-count percentage.

The latest comparison has **176 original probes: 175 evaluated for P1 and one
out of scope**, `real_net`, whose source depends on Icarus's `wire real` extension.
The **two required-scope gaps** are specify module paths and dense UDP front-end
compilation exceeding the comparison deadline. The three raw passing-reference
gaps remain recorded; the extension's compile rejection is not rewritten as a pass.
**25 reference mismatches** are tracked separately and are not counted as kirimasim
defects. See the [measurement](docs/measurements/2026-11-01/icarus13-wire-real-optional.json) and
[comparison interpretation](docs/notes/p1-icarus-progress.md).
A finite inventory cannot establish complete functional coverage.

The [first VPI execution subset](docs/notes/vpi-execution.md) now supports explicit
`--vpi=<file>` loading, registered tasks/functions, static numeric reflection,
variable writes/force/release and supported callbacks in tier 1. Full VPI and
cocotb integration remain work.

Remaining implementation includes VPI beyond that subset; specify paths
and timing checks; the FST backend; remaining `%p` handle/storage forms and complete strength `%v`
formatting; class dependency observation/property references and other typed
boundaries; implicit container sensitivity and remaining container/event forms;
and remaining port/force, input and selection combinations documented in
[P1 Support](docs/P1_SUPPORT.md). Eligible static partial inout ports use [physical identity](docs/notes/partial-inout-port-execution.md). [Source pattern formatting](docs/notes/source-pattern-formatting.md) now connects
the shared renderer/SIR path. FST build dependencies alone do not provide its backend.

Comparison work must map the necessary standard subset, extensions and external
interfaces separately. Deferred extensions and broader `with`/string streaming
targets or event assignment retain their documented decisions; a later-phase
label cannot defer anything required by [P1_SCOPE](docs/spec/P1_SCOPE.md).

### P1 exit criteria

[TEST_SUITE](docs/spec/TEST_SUITE.md) and [P1_SCOPE](docs/spec/P1_SCOPE.md)
are authoritative. **Support for Icarus Verilog's `wire real` extension is not a
mandatory P1 condition. Cases with a confirmed dependency on this extension do
not prevent P1 completion when unsupported.** Standard `real` features retain
their existing scope; crashes, hangs and internal errors remain failures. See
the [classification and evidence](docs/notes/p1-wire-real-exception.md).
Recorded pass-rate, mandatory-test and seven-night fuzz
gates have evidence of success. Their dates, populations and reference builds
are in [Recorded validation counts](docs/VALIDATION_COUNTS.md).
Final M8 assessment must reconcile the target revision's comparisons on both
operating systems, mandatory tests, the latest seven formal nightly fuzz runs
and required human reviews. Passing ordinary CI alone does not complete P1.

### Test results

<!-- counts:begin -->
<!-- Written by tools/readme_counts.py from docs/counts.json; do not edit by hand. -->

The recorded dev run has **11,364 / 11,364 tests passing**, 0 skipped and 0 failed. It contains 1,135 unit tests, 9,948 suite cases (175 generators), 193 tool tests, 29 SIR-dump tests, 21 Context tests and 38 CLI/event-count tests.
<!-- counts:end -->

Release has one additional default event-limit test. Full local CI checks
dev/release/ASan/UBSan plus static rules, formatting and clang-tidy at a clean,
frozen candidate. See the [approved integration record](docs/notes/p1-approved-class-inout-integration.md)
for the current scope and reproduction. The suite has no skip entries.
These results are distinct from dated external
comparisons. [Validation](docs/VALIDATION.md) explains reproduction and limits;
[Benchmarks](docs/BENCHMARKS.md) holds dated speed/memory results and conditions.

### Phases

| Phase | Purpose | Status |
| --- | --- | --- |
| P0 | Build, CI, specifications and test groundwork | Complete |
| P1 | Front end, shared kernel and first execution tier; Icarus 13.0-or-broader required coverage with the optional `wire real` exception | In progress |
| P2 | Finalize SIR v1 and the runtime boundary | Direction approved; detailed contracts under review |
| P3 | Tier 2: ahead-of-time C++ generation and speed work | Planned |
| P4 | Broader testbench features | Planned beyond the subset required by P1 |
| P5 | Broader switch-level, parallel execution and embedding work | Planned beyond the subset required by P1 |

Current SIR formatVersion is 0 and C ABI is 0.3. P2 preparation can accompany
P1, but its freeze follows P1 completion; see the [SIR v1 proposal](docs/notes/sir-v1-proposal.md).
Broader constrained randomization/UVM, concurrent assertions, covergroups,
virtual interfaces and DPI-C remain unimplemented. VHDL and analog/mixed-signal
simulation are outside the current language targets.

## Building

Requirements: CMake 3.28+, Ninja, Python 3.10+, and a C++23 compiler:
GCC 13+; Clang 18+ with libc++ 18+, or Clang 19+ with libstdc++ 13+;
or Apple clang 17+ (Xcode 26). Clang 17/18 with libstdc++ cannot build slang
as C++23. Linux and macOS are supported build platforms.

The first configure downloads pinned slang, GoogleTest and standalone libfst.
FST dependency preparation also requires zlib/bzip2 development libraries
(`zlib1g-dev libbz2-dev` on Debian/Ubuntu; the Command Line Tools SDK on macOS).
Meson, GTKWave, GLib and separate LZ4 are unnecessary.
See the [User Manual](docs/USER_MANUAL.md#build-and-version) for setup and presets.

```sh
cmake --preset dev
cmake --build --preset dev
ctest --preset dev
```

Each preset writes to `build/<preset>/`. Other presets are `release`, `asan`,
`ubsan`, `tsan` and `fuzz`; the [Development Guide](docs/DEVELOPMENT.md)
describes their checks and the required pre-push `tools/local_ci.py` run.

## Usage

```sh
build/dev/src/driver/kirimasim [options] <file>... [+plusarg]...
build/dev/src/driver/kirimasim parse [options] <file>...
build/dev/src/driver/kirimasim --version
```

Run the supplied counter testbench from the repository root:

```sh
build/dev/src/driver/kirimasim --top=tb \
  -Idocs/examples/user_manual/include -DCOUNTER_WIDTH=8 \
  -y docs/examples/user_manual docs/examples/user_manual/tb.sv +cycles=6
```

Design stdout starts with `width=8 cycles=6` and ends with `t=70 ns q=6`.
`-I` searches includes, `-y` searches module libraries, `-D` sets a compile-time
macro and `+cycles=6` is read by the design at runtime. Bare `-DNAME` means `1`.
The simulator's own messages go to stderr.

Common controls include `--top`, `--timescale`, `--seed`, `--max-time`,
`--max-wall-time`, `--max-procs`, `--init-values`, `--cond-x`, `--report-pulses`,
`--bounds-check`, `--wave=<file>.vcd`, `--dump-sir` and `--stats=json`.
For exact defaults, supported forms and exit codes, use the
[Reference Manual](docs/REFERENCE_MANUAL.md); for a failing design, use
[Debugging](docs/DEBUGGING.md).

## How it works

```mermaid
flowchart LR
    source[SystemVerilog] --> slang["slang: parse and elaborate"]
    slang --> front["front: lower to SIR"] --> sir[SIR]
    sir --> vm["Tier 1: SIR interpreter"] --> kernel["Shared execution kernel"]
    sir -.-> cpp["Tier 2: generated C++, planned P3"]
    cpp -.-> kernel
```

The kernel owns scheduling and value operations; all tiers share it.
`valsem` supplies independent reference value semantics. Mechanism matrices,
compiled negative controls, reference comparisons, fuzzing, sanitizers and static
checks validate the implementation. See [Architecture](docs/ARCHITECTURE.md)
and [Validation](docs/VALIDATION.md).

## Contributions

The project currently accepts **specification proposals and issues**, with
questions/early ideas in [Discussions](https://github.com/kurochan001/kirimasim/discussions).
Pull requests are disabled. See [CONTRIBUTING](CONTRIBUTING.md),
[Code of Conduct](CODE_OF_CONDUCT.md) and [Security](SECURITY.md).
Commercial simulator results are not used or accepted; see the
[comparison policy](docs/FAQ.md#do-you-check-against-commercial-simulators).

## Documentation

Public prose documents have complete English/Japanese versions, including
CHANGELOG, dated audits and public report drafts. English is authoritative
where versions differ. Code, raw diagnostics, machine-readable evidence and
the canonical license retain their original identifiers/text; bilingual guides
explain them. Private untracked handoffs/drafts are outside the public set.

Start with the [Documentation Guide](docs/README.md),
[Glossary](docs/GLOSSARY.md) or [FAQ](docs/FAQ.md).
The [support inventory](docs/P1_SUPPORT.md) describes current behavior;
[specifications](docs/spec/) define approved contracts;
[CHANGELOG](CHANGELOG.md) and [Status History](docs/STATUS_HISTORY.md) retain
changes and older checkpoints. The [previous README](docs/history/README_2026-11-01.md)
keeps removed chronology, comparison tables and performance evidence.

## License

Apache License 2.0 with LLVM Exceptions (`Apache-2.0 WITH LLVM-exception`):
see [LICENSE](LICENSE), its [bilingual reading copies](LICENSE.md), and [Third-party notices](THIRD_PARTY_NOTICES.md).
Files use this license unless stated otherwise. “The kirimasim Authors” refers
to the contributors recorded in the commit history.
