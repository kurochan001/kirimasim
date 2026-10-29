# kirimasim Development Guide

[日本語](DEVELOPMENT.ja.md) · The English version is authoritative when the versions differ.

This guide is for maintainers implementing an agreed feature or fixing a bug.
It complements the rules in [CONTRIBUTING](../CONTRIBUTING.md) and
[AGENTS.md](../AGENTS.md). External participation currently uses specification
proposals and issues; pull requests are disabled and authorized collaborators
push to `main`. This guide does not change that policy.

The worked trace follows existing `$countones` / `$countbits` execution at
source `5e8ab597`, checked on **2026-10-29**. It demonstrates how to find the
implementation and evidence for a feature; adding a new system-function name
also requires support in the pinned, unmodified slang frontend.

## Prepare a development checkout

Install the prerequisites listed in the [User Manual](USER_MANUAL.md#build-and-version).
Ask before adding a dependency or installing a new tool. From the repository root:

```sh
cmake --preset dev -DKIRIMASIM_WERROR=ON
cmake --build --preset dev
build/dev/src/driver/kirimasim --version
ctest --preset dev -N
```

`-N` lists tests without running them. Each preset has a separate build tree.
The executable currently interprets SIR directly; tier 2 C++ generation is
planned for P3. Use [Architecture](ARCHITECTURE.md) for library boundaries and
the [phase mapping](notes/phase-renumbering.md) for historical phase labels.

## Begin with the contract and an original reproducer

Find the relevant section of [docs/spec](spec/) and its approval status.
For bit queries, start with [System Tasks 8](spec/SYSTEM_TASKS.md#8-conversion-and-queries)
and the value representation in [Value Semantics](spec/VALUE_SEMANTICS.md).
Record the standard clause and the supported operand/result types, widths,
X/Z behavior, evaluation order, side effects, lifetime and error outcomes.

If the required behavior conflicts with the detailed specification, propose
the specification amendment and obtain approval before implementing it.
Do not decide a language rule merely from another simulator's output.
Use the standard, approved contracts and independent reference semantics.
Read the clean-room rules in CONTRIBUTING: do not inspect Icarus or Verilator
implementation code or copy another project's reproducer. slang is an
unmodified dependency; report needed frontend fixes upstream.

The self-written [bit-query example](examples/development/bit_queries.sv)
has an 8-bit value containing X/Z and a 65-bit value crossing a word boundary.
It assigns them at runtime, checks the answers with `$fatal`, and prints:

```text
ones=3 unknown=2 wide=2
```

Run frontend checking, inspect the lowered form, then execute:

```sh
build/dev/src/driver/kirimasim parse --top=bit_queries \
  docs/examples/development/bit_queries.sv
build/dev/src/driver/kirimasim --dump-sir --top=bit_queries \
  docs/examples/development/bit_queries.sv > bit_queries.sir
rg -n '\$countbits' bit_queries.sir
build/dev/src/driver/kirimasim --top=bit_queries \
  docs/examples/development/bit_queries.sv
```

Each command should return 0. The dump is a diagnostic format, not a
loadable compiled model. The example is documentation data; it is not
automatically registered in the test suite.

## Trace one existing function through the implementation

### 1. Frontend and lowering

[front.cpp](../src/front/front.cpp) configures and runs the pinned frontend.
In [lower_expr.cpp](../src/front/lower_expr.cpp), find the branch handling
`$countbits`, `$countones`, `$onehot`, `$onehot0` and `$isunknown`:

```sh
rg -n 'countones|countbits' src/front/lower_expr.cpp
```

The branch converts a supported operand to a bitstream and lowers
`$countones(value)` to a `Syscall` named `$countbits` with a control bit of 1.
`$isunknown` supplies X and Z controls; one-hot queries also compare the count.
Explicit controls use their least significant bit. Typed coercions determine
the result width and state. Read the neighboring constant-evaluation paths
as well: a constant-only example can miss a runtime implementation defect.

Capture an operand once in the required order. When adding a new supported
source form, consider static/automatic storage, calls, refs, aggregates and
waits separately. Unsupported forms need a positioned diagnostic rather
than a guessed lowering. Frontend acceptance alone is insufficient evidence.

### 2. SIR boundary

The operation and typed operands are defined in
[design.h](../src/sir/include/kirimasim/sir/design.h). The
[verifier](../src/sir/verify.cpp) checks the representation; the
[text writer](../src/sir/text.cpp), with operation names in
[types.cpp](../src/sir/types.cpp), makes it inspectable.

This function family reuses `Op::Syscall`; it does not require a distinct
opcode for every source spelling. General representation validation does
not by itself establish all syscall-specific runtime semantics. If a change
needs a new operation, operand form or metadata, update the agreed SIR
contract, verifier, text form and their tests together. Test malformed forms
at this boundary, as well as valid compiler output.

### 3. VM dispatch

In [vm.cpp](../src/vm/vm.cpp), follow the `$countbits` name mapping to
`SysTask::CountBits` and its execution case. The VM collects the requested
0/1/X/Z categories and dispatches by operand width to the narrow or wide
kernel helper, then places the typed result in the instruction's temporary.

Keep the VM responsible for executing typed SIR and managing its frames.
Shared value semantics belong in the kernel's value helpers. For stateful
tasks, also trace the owning Context, scheduler region, termination,
suspension and cleanup paths. Address signal values through typed operands;
do not look up signals by source name during execution.

### 4. Kernel and independent reference

The fast narrow implementation is `countBits4` in
[w4ops.h](../src/kernel/include/kirimasim/kernel/w4ops.h).
The wide implementation is `wide::countBits` in
[wide.cpp](../src/kernel/wide.cpp), declared in
[wide.h](../src/kernel/include/kirimasim/kernel/wide.h).
The independent semantic implementation is `valsem::countBits` in
[value.cpp](../src/valsem/value.cpp).

Keep the reference simple and independent of the fast implementation.
Test padding bits at non-word-aligned widths and preserve X/Z distinctions.
Do not make `kernel` depend on slang or SIR, or link the production kernel
to `valsem` to reuse an algorithm; the test executable can compare them.
The allowed dependency directions are in
[Architecture](ARCHITECTURE.md#libraries).

### 5. Evidence at multiple boundaries

[W4Ops.CountBits](../tests/unit/w4ops_test.cpp) compares fast narrow counting
against the independent reference using generated words and widths and all
16 category masks. The existing
[aggregate source case](../tests/suite/values/aggregate_bit_query_record_fork_two_65.sv)
also exercises frontend lowering and execution for a wide aggregate in a fork.
These establish different properties; neither alone proves every source form.

Select and run these existing examples:

```sh
ctest --preset dev -N \
  -R '^(W4Ops[.]CountBits|suite/values/aggregate_bit_query_record_fork_two_65)$'
ctest --preset dev --output-on-failure \
  -R '^(W4Ops[.]CountBits|suite/values/aggregate_bit_query_record_fork_two_65)$'
```

The selection should contain two tests. Select new tests by their actual
registered names too; an empty selection is not validation of a new feature.

## Implement an approved extension

Use the trace above to decide which boundaries need changes:

1. Write an original minimal input and state its required result and basis.
   Separate valid-but-unsupported inputs from invalid language inputs.
2. Define or extend the independent value reference when value meaning
   changes. Choose a typed kernel service for behavior shared by execution tiers.
3. Extend SIR only when existing typed operations cannot express the behavior.
   Add verifier rejection tests for new invariants.
4. Implement the kernel helper and differential tests, then VM dispatch or
   frame handling where required. Respect evaluation and storage lifetimes.
5. Connect frontend lowering and add source-level coverage for both constant
   and runtime paths, required widths/types, X/Z and effectful arguments.
6. Check diagnostics and resource/termination behavior where applicable.
   Stateful tasks need tests of ordering, repeated calls, cleanup and
   independent Contexts.
7. Update [P1 Support](P1_SUPPORT.md), relevant user/reference sections and
   examples in both languages. For user-visible behavior changes, add the
   appropriate CHANGELOG entry under CONTRIBUTING's rules.

Not every change touches every library. For example, a new lowering that
reuses an existing well-tested helper may need no new kernel operation.
Keep implementation work within the approved contract and dependency graph.

## Add tests without weakening expectations

Choose tests by the boundary they verify:

- **Value helpers:** independent-reference comparisons in `tests/unit/`,
  including small/large widths, signedness and X/Z.
- **SIR and VM:** valid and malformed typed forms, execution and lifetime
  tests in `tests/unit/`.
- **Source language:** new `.sv` files under `tests/suite/`, with truthful
  sv-tests-compatible metadata and independently justified expectations.
- **Driver behavior:** tests under `tests/cli/` for argument parsing,
  stdout/stderr and exit status. Register a new runner in its CMake file.
- **Context isolation or operation counts:** the relevant `tests/context/`
  or `tests/perf_counts/` contract when the change affects those properties.

Suite files are discovered during CMake configuration. Use `:top_module:`,
`:type: simulation`, `:expected_stdout:` and the applicable oracle, clause,
state and review metadata described in [Test Suite](spec/TEST_SUITE.md).
For a rejection, include `:should_fail_because:` and the precise
`:expected_error:` location/message; check the intended diagnostic as well
as exit 2. Read [run_suite_test.py](../tools/run_suite_test.py) before relying
on a header field. Each design runs in an isolated temporary directory.
Add new unit test sources to [tests/unit/CMakeLists.txt](../tests/unit/CMakeLists.txt).

Existing expected values and test bodies are protected. If the specification
and an existing expectation disagree, present the evidence for review first.
Expectation corrections require their own approved commit, separate from
`src/` changes, with the reason and standard clause. New tests and new
expectations can accompany implementation. Do not weaken assertions or add
skips to obtain a pass. See [CONTRIBUTING](../CONTRIBUTING.md#changing-expected-results).

## Validate and prepare the change

Use the [Validation Guide](VALIDATION.md) for test scopes, seeds, external
corpora and result interpretation. Use [Debugging](DEBUGGING.md) to isolate
a failing stage and prepare a minimal reproducer.

Build and run focused tests while developing. When relevant, run the matching
sanitizer and Context tests and inspect PerfCount regressions. For a change
ready to push, the repository requires the local CI runner:

```sh
CTEST_PARALLEL_LEVEL=4 python3 tools/local_ci.py
```

Choose parallelism to fit the machine. By default the runner configures,
builds and tests dev/release/ASan/UBSan, checks release flags and project
policies, validates unpushed commits and README counts, and runs clang-format
and clang-tidy. Inspect its summary and logs under `build/local-ci/` and
report the scope actually run. Minimum-compiler and nightly coverage are
separate CI work; the local summary does not establish their results.

When new tests change counts, use the update procedure in CONTRIBUTING and
the runner's dev JUnit output, then review the generated README changes.
Do not edit expected PerfCount values simply to make a regression pass;
follow [its maintenance procedure](../tests/perf_counts/README.md).

The project is C++23, with kernel public headers and generated code restricted
to C++20. Add a new public kernel header to
[kernel_headers_cxx20.cpp](../tests/unit/kernel_headers_cxx20.cpp). Generated
code must avoid exceptions and RTTI. Follow the banned-feature and
global-state rules; runtime state belongs to Context and errors must not
escape a kernel boundary as exceptions.

Keep one purpose per commit and use an English subject of at most 72
characters. Use a GitHub noreply author email and the required `Assisted-by`
trailer when AI helped; identify the actual model/version, or mark an
unverified version as unconfirmed. Do not add AI `Co-Authored-By` or
`Signed-off-by` trailers. Before an authorized push, verify the intended
diff and upstream state, check each unpushed commit's expectation changes
and attach the local CI summary. Do not place private designs, personal paths
or copied standard text in the repository.
