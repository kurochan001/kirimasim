# CLAUDE.md: rules for Claude Code working on kirimasim

[日本語](CLAUDE.ja.md) · This English text is the main version; the Japanese version follows it, and where they differ, this text holds.

## Purpose of the project

kirimasim is an open-source SystemVerilog simulator. It takes 4 states (0, 1, X, Z) as the default and, while keeping scheduling exactly as IEEE 1800-2023 says (every region of the time slot and the reference algorithm), aims at practical speed. The front end uses slang unmodified at a pinned version; through the intermediate representation SIR, an interpreter (tier 1) and C++ generation (tier 2) run sharing one execution kernel. It is now at stage P0 (preparation), with only the foundations of the build and a skeleton.

## Rules of library dependencies

Dependencies go only in the directions of the table below. Do not add to `target_link_libraries` a dependency not in the table.

| Library | Content | May depend on | State |
| --- | --- | --- | --- |
| `front` | Calling slang, and lowering to SIR | slang, `sir` | Only reads and elaborates with slang |
| `sir` | SIR's definition, verifier, text form | Nothing | Only a function returning the version |
| `analysis` | X-reachability analysis, static islands, levelization, observability | `sir` | Empty |
| `vm` | Tier 1 (making and running bytecode) | `sir`, `kernel` | Empty |
| `codegen` | Tier 2 (generating and building C++) | `sir`, `analysis` | Empty |
| `kernel` | Execution kernel (scheduler, value operations, GC, VPI, DPI-C, waveforms and so on) | libfstapi (later). Optional dependencies are isolated in `solver_adapter` and `fiber_backend`, and `kernel` sees only their C++ interfaces | 4-state words and `and4`, event counters, Context |
| `solver_adapter` | The replaceable port for constraint solvers | `sir`, the external solver adopted | Made in P3 |
| `fiber_backend` | Fibers that stop and resume time-consuming DPI-C tasks | Only the library adopted | Made in P3 |
| `valsem` | Reference implementation of value semantics (slow but plainly correct) | Nothing | 1-bit 4-state values and AND |
| `driver` | The executable `kirimasim` | Everything | `--version`, `parse`, `--stats=json` |

- `kernel` is linked together with the user's design, so it must not depend on slang or LLVM.
- `kernel`'s public headers (`src/kernel/include/kirimasim/kernel/`) must compile as C++20. When adding a new header, add it to `tests/unit/kernel_headers_cxx20.cpp` too.
- Namespaces are `kirimasim::<library name>`.

## C++ versions and prohibitions

- The main body is C++23. C++26 features are used only with compilers that have them, through feature-test macros.
- `kernel`'s public headers and generated code are C++20. Generated code uses no exceptions or RTTI.
- Forbidden in `src/kernel/` and `src/vm/` (and in future generated code): `std::shared_ptr`, `dynamic_cast`, `std::function`, `getenv`, `typeid`. `tools/check_banned.py` checks this. Words inside comments are detected too, so explanations use other wordings.
- `std::unordered_map` and `std::unordered_set` may be used only with `// kirimasim: unordered-ok <reason>` on the same line or the line before. Do not iterate over them where it affects results.
- No mutable global variables, static variables or singletons. State belongs to the Context.
- Do not read environment variables during execution. Settings are read once at startup (`driver` too reads no environment variables now).
- Do not look up signals by string during execution (name tables are only for VPI, waveforms and diagnostics).
- Do not throw exceptions across the boundary of the execution kernel. Errors are returned as values.
- The meaning of operations, edge detection and case matching is set in one place, the reference implementation `valsem`, and fast versions have tests that cross-check them with it.

## Build and test commands

```sh
cmake --preset dev && cmake --build --preset dev && ctest --preset dev
cmake --preset release && cmake --build --preset release && ctest --preset release
python3 tools/check_release_flags.py      # after configuring release
cmake --preset asan  && cmake --build --preset asan  && ctest --preset asan
cmake --preset ubsan && cmake --build --preset ubsan && ctest --preset ubsan
python3 tools/check_banned.py
ctest --preset dev -L PerfCount           # only the count regressions
python3 tools/local_ci.py                 # all of the above plus clang-format and clang-tidy
```

- Before pushing, run `tools/local_ci.py` and attach its summary to the report. GitHub's macOS runners are often unavailable for this repository, so macOS results are covered by this (the jobs of `ci.yml` run only on self-hosted Linux (x86-64) runners with the label `kirimasim-linux`, and there is no macOS job).

- The test suite (`tests/suite/`) has headers compatible with sv-tests. For now it only checks that `kirimasim parse` passes. Tests expected to fail write `:should_fail_because:` and `:expected_error:` (`<line>:<column>: error: <text>`), and both exit code 2 and the diagnostic at that position are checked.
- PerfCount's expected values are in `tests/perf_counts/expected.json`. How to update them is in `tests/perf_counts/README.md`.

## Decide meaning in one place, and do not fix by the shape of the symptom (approved 2026-10-16)

Fixing a bug with a branch for the shape of the symptom leaves the neighbouring shapes broken, and the interactions between branches bring back bugs that were fixed. These rules prevent that.

- **Where meaning is decided:** the meaning of constructs is decided in `front` (when making SIR) and `kernel`, and the meaning of values in `valsem`. `vm` and `codegen` only execute and translate SIR's instructions mechanically, and hold no judgment of the form "special case for this shape".
- **The container does not change meaning:** in a module's `initial` or `always`, a class method, an automatic task, a fork branch, an interface, generate or a package, the same statement becomes the same SIR. Only the place (slot or frame) may differ.
- **Do not make the shape of a symptom a condition:** do not put issue numbers or the shapes of particular tests in `if` conditions. A commit that fixes a bug states these three things:
  1. The rule it broke (the clause of the standard, and which item of `docs/spec/`)
  2. The one place that implements that rule (the function's name)
  3. The other entries through the same rule (an explanation that, coming from other constructs or containers, they pass through that one place)
- **Do not rewrite the user's source as strings:** work only on slang's syntax tree and semantic model.
- **Do not treat libraries specially:** do not make branches on the C++ side that recognize class names of UVM and the like. uvm-core runs as SystemVerilog as it is. When part of it is replaced for performance, the execution of the original SystemVerilog is kept as the reference, with tests that cross-check them.
- **No hidden knobs:** behaviour is switched only by options listed in CLI.md.
- **Do not put knowledge of the test suite in the simulator:** the main body prints no pass/fail strings (PASS, FAIL). It does not change behaviour by file names or paths. It does not read test metadata (`:should_fail_because:` and the like). Pass or fail is decided only by the judging tools in `tools/`. `tools/check_banned.py` checks this.
- **Reject illegal input by default:** do not loosen slang's diagnostics. When loosening for compatibility, keep strict as the default, and list the loosening option and what it covers in CLI.md.
- **Add bug tests as a matrix of the mechanism:** add not one reproducing example but a matrix test of the mechanism (place × way of reading or writing × spelling of the name × container), made by a generator (TEST_SUITE.md). Name tests by the clause of the standard and the mechanism, not by issue numbers (numbers go in the header's `:issue:`).
- **Fix mechanism by mechanism:** when 3 bugs of the same mechanism (the same clause) have gathered, stop fixing them one by one; make the mechanism's matrix first, then fix.
- **When to close an issue:** only when the number of symptoms the issue lists equals the number of check points of the tests, and the closing comment states the result of checking the neighbouring shapes with the matrix. Do not close with "the rest later" (if something remains, make it a separate issue).

## How to write commits

- One commit per purpose. Messages in English, the first line at most 72 characters.
- When an AI helped, add the trailer `Assisted-by: Claude Code:<model name and version>`. Do not add `Co-Authored-By`.
- Do not add `Signed-off-by`. DCO is not in operation now (DCO or a CLA will be introduced when PRs of code from outside are accepted).
- The author email of commits is the GitHub noreply address.

## Changes of expected values go in separate PRs

Changes that change, remove or loosen existing expected values (existing items of `tests/perf_counts/expected.json`, existing lines of `tests/**/expected*`, `:expected_stdout:` lines, the bodies of existing tests in `tests/suite/`), and removing or renaming existing test files, do not go in the same PR as changes of `src/`. Unit tests in `tests/unit/` may be fixed in the same PR as the implementation to follow a renamed function and the like, but `EXPECT_*` and `ASSERT_*` must not be weakened (CI shows changed assertions as warnings, and a person looks at them in review). Make a separate PR stating the reason and the clause number of the standard. Adding new tests and new expected values may be in the same PR as the implementation. Do not rewrite expected values or tests to make tests pass. When they disagree, first show a person which of the implementation and the expected value follows the standard. `tools/check_expected_change.py` checks this in CI.

## What must not go in the public repository

- Private design material (the originals of design specifications, figures, reports). Documents in the repository are summaries rewritten from them.
- The name, code or internal information of the private predecessor project.
- Copies of the text, figures or example code of IEEE 1800 (giving clause numbers and summarizing in your own words is fine).
- Absolute paths of home directories, user names, email addresses (except GitHub noreply addresses and the contact in `CODE_OF_CONDUCT.md`), machine identifiers.
- Information about workplaces or work.
- Output of commercial simulators.

## Do not read the source of other simulators

Do not read the source code of Icarus Verilog and Verilator (clean room). Learn their behaviour from the standard and from the results of running them. slang is a dependency, so it may be read.

## Write "not confirmed" for what is not known

Do not fill in by guessing what has not been checked: version numbers, licences, interpretations of the standard, the behaviour of tools and so on. Documents and reports say "not confirmed", with a way to check it. Version numbers use values actually looked up.

## Changes that disagree with the specification

When a change that disagrees with the detailed specifications in `docs/spec/` is needed, propose it to a person as an amendment of the specification before implementing, and implement after it is approved. Documents with official wording (licences, codes of conduct and so on) are not written from memory but fetched from the official source. When adding a new dependency, or when a new tool needs to be installed, stop and ask a person.
