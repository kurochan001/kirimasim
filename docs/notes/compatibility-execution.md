# P1 frontend compatibility modes and executable driver checks

[日本語](compatibility-execution.ja.md) · 2026-11-03.

The already approved CLI 2.1 delegation is connected as
`--compat=default|vcs|all`. The full argument reaches the pinned slang driver
for compilation, ordinary parse and SIR output. Omission retains strict defaults.
No project-specific profile, language-version switch or dependency change is
introduced. Slang remains unmodified at
`e2d1021661f78b3f17d1edf7ea17f9d6f95fb168`.

## What each pinned mode changes

The [machine-readable profile inventory](../measurements/2026-11-03/compatibility-profiles.json)
lists every implied compilation/analysis flag and diagnostic baseline override
from that revision's public settings and diagnostic definitions. This inventory
is frontend configuration evidence, not native execution proof for every form.

- `default` (also omission): no compatibility compilation/analysis flags;
  normal local-include and macro rules; the default diagnostic baseline.
- `vcs`: hierarchical constants, integral-to-enum conversion, use before
  declaration, string-to-integral conversion, recursive calls without explicit
  parentheses, bare module parameter assignment, self-determined streaming
  concatenations, ANSI port redeclarations/merging, assignment patterns in array
  concatenations, first library definition on library redefinition, legacy
  `cross_auto_bin_max`, and typed input-port variable inference. It also enables
  the frontend's multiple-driven-local relaxation and the recorded diagnostic
  overrides. Explicit include directories are searched before the source
  directory. Trailing whitespace is permitted after later macro-continuation
  backslashes; the first continuation retains the frontend's restriction.
- `all`: the same twelve compilation flags, plus top-level interface ports,
  references to unnamed generate blocks and virtual-interface assignment with
  bind/defparam overrides. Analysis additionally permits initial drivers of
  specialized procedural variables. Its diagnostic overrides differ from
  `default` and `vcs`. It **does not** enable the `vcs` include-directory-first
  or macro-whitespace defaults: `all` is not simply a superset of that profile.

The inventory preserves the exact error/ignored diagnostic group names.
`default` and `vcs` promote different groups to errors; `vcs` and `all` ignore
the recorded eight groups. Explicit `-W` handling remains the existing frontend
warning policy. A profile name is not a claim that another product's execution
results are reproduced. Broader lowering limitations remain in [P1 Support](../P1_SUPPORT.md).

Repeated mode arguments retain the pinned enum parser's **last** occurrence
in the tested direct and command-file routes. Empty/unknown names are input
errors. Only the documented `=<name>` direct spelling is added; existing
command-file handling remains frontend-owned.

## Source errors before executable SIR

The existing source from the 2026-10-31 compatibility observation is retained
byte for byte. With `--compat=all` or a suppressed multiple-writer diagnostic,
ordinary parse succeeds, but execution/SIR output formerly reached the verifier
and returned internal-error 3. The [before/after record](../measurements/2026-11-03/compatibility-validation.json)
preserves all twelve observations and the original source hash. Those executable
paths now return source compilation error 2, while ordinary parse stays unchanged.

`front::checkExecutableDrivers` checks typed semantic driver diagnostics before
lowering, under the existing SIR 3.4/10 and scheduling 6.4 writer constraints
(IEEE 1800 6.5 and 9.2.2.2–9.2.2.4). It reuses the existing analysis for normal
settings; where compatibility omitted checks, a local analysis clears the two
writer relaxations without changing the original driver. It selects the typed
multiple-specialized-writer, multiple-variable-continuous-writer, mixed-variable
writer and unresolved-net driver diagnostics, preserving locations and notes.
Suppressed warnings therefore cannot create invalid executable SIR.

The result is a source rejection, not a pass or a rewrite of the SIR contract.
The SIR verifier still runs on otherwise eligible lowered designs, and genuine
verifier failures retain internal-error classification. No verifier error-text
matching, catch-all downgrade, runtime mode branch or source rewrite is used.
Analysis arguments are formatted while owned, and the analysis is released
before lowering. Runtime values, scheduling, force/strength semantics, C ABI
and public kernel headers are unchanged.

Ordinary parse can consequently accept more than the executable subset.
Compile-only VPI workflows already requiring SIR lowering use the same compiler
preflight; ordinary parse and compile-only VPI are not identical APIs.

## Mechanism coverage

The generator creates **106 original SV files and two headers**, including
96 writer cases: ff/comb/latch, continuous-variable and mixed-variable writers;
1/129-bit storage; module/interface/generate scopes; direct/function writes;
overlap and separate objects/bits. The mode cases exercise enum, hierarchical
constants, declaration order, string conversion, include precedence, later
macro continuation, invalid/repeated modes and ordinary resolved-net, task-write
and static-initializer boundaries.

- Mode CLI: **409 checks**, across direct/file transport, argument position,
  ordinary parse, execution and SIR output.
- Driver CLI: **1,728 checks**, including strict/suppressed/all settings.
  Rejections require source-position errors, no design/SIR output and exit 2;
  crashes, hangs and internal errors fail. Separate ranges, resolved nets,
  task writes and declaration initializers remain eligible.
- Four API unit tests check compile result classification, parse separation,
  disjoint wide bits and repeated independent frontend calls.
- **Eight compiled semantic controls caught**, covering the forwarded mode,
  strict default, compatibility reanalysis, suppressed raw diagnostics, three
  writer families and stopping before lowering. No build failure/timeout is
  counted as detection. Source hashes and the nine-test baseline were restored
  ([record](../measurements/2026-11-03/compatibility-controls.json)). All 1,149
  mutation anchors apply, and focused clang-tidy passed.

Existing expectations and the frozen P1 populations/comparisons are unchanged.
No Icarus/sv-tests/fuzz remeasurement, commercial-tool execution, upstream post
or new package installation was performed. **P1 remains in progress.**

## Complete validation

Full macOS dev passed 11,594/11,594 without failures/errors/skips (148.14 seconds).
Counts were updated from that JUnit in a separate commit. Final local CI passed
all 21 steps on clean `f8cbfeae`: dev/ASan/UBSan each passed 11,594 tests, release
passed 11,595, with no skips, and clang-tidy passed on all 191 project files.
The validation record stores the result in `final_local_ci`. Subsequent changes
touch documentation/records only and preserve tested code hashes. Other OSes,
compilers and full input equivalence were not established by this measurement.

After building, reproduce this coverage with
`ctest --preset dev -R '^(DriverChecks\.|cli/compatibility_modes|cli/executable_drivers|tools/gen_compatibility_matrix)'`.
