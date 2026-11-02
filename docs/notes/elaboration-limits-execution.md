# P1 frontend elaboration limits

[日本語](elaboration-limits-execution.ja.md) · 2026-11-03.

The four already approved CLI 2.1 / UNSPECIFIED_BEHAVIOR A11–A12 options
now reach pinned slang directly, as they already did through a command file:
`--max-hierarchy-depth=<n>`, `--max-generate-steps=<n>`,
`--max-constexpr-depth=<n>` and `--max-constexpr-steps=<n>`.
The corresponding language mechanisms are hierarchy/generate elaboration
(IEEE 1800 chapter 27) and constant functions (13.4.3); numeric budgets are
frontend choices, not IEEE-mandated values.

## One forwarding boundary

`parseArgs` recognizes the four documented `=<n>` spellings in one branch and
passes each full argument unchanged. The existing `front::setUpDriver` supplies
them to the same slang driver for execution, `parse` and `--dump-sir`. No
runtime limiter, scheduler, SIR instruction, ABI or dependency changes.
No default argument is synthesized. Slang remains unmodified at
`e2d1021661f78b3f17d1edf7ea17f9d6f95fb168`; its defaults are hierarchy depth
128, generate steps 131072, constant-call depth 128 and constant steps 1000000.
`--help` now lists all four switches. No new bare/space-separated aliases
are introduced.

Numeric syntax, range validation, repeat handling and source diagnostics
remain slang's. A rejected compilation returns 2 before simulation, with no
design stdout or SIR output. These limits are separate from simulation
`--max-steps`, `--max-call-depth`, time and resource limits. In particular,
zero is forwarded as zero, **not an unlimited-mode shortcut**. Ordinary
strict acceptance and all existing test expectations stay unchanged.

## Original mechanism matrix and controls

`tools/gen_elaboration_limit_matrix.py` produces 21 original source fixtures:
16 finite workloads (four per budget), four nonterminating workloads bounded
by explicit limits, and one clean input. Hierarchy variants include ordinary,
generate, instance-array and interface nesting. Generate variants cover module,
interface, program and nested generate scopes. Constant functions are exercised
in module/package/interface declarations and from generate-local constants.
The recursive hierarchy uses distinct parameter specializations so its limit
is actually exercised rather than an earlier identical-instance cycle check.

`cli/elaboration_limits` passes **871 checks** covering default/high/low budgets,
source positions, three modes, direct/file transport, zero, numeric bounds,
malformed/missing values, repeats, independent simultaneous budgets, bounded
nontermination and help. Failure checks require exit 2, no stdout and the
written reason/source position when applicable; a crash, hang or internal
error is a failure. `tools/gen_elaboration_limit_matrix` checks regeneration.
The existing CLI, source-warning and new tests pass together.

Five controls were compiled: each configured budget replaced by a larger one,
and forwarding omitted for execution. All five were caught by a semantic
mismatch (expected compilation rejection instead ran successfully), not a build
failure or timeout. The original source hash and four-case CTest baseline were
restored. All **1,141** mutation catalogue anchors apply. See the
[control record](../measurements/2026-11-03/elaboration-limit-controls.json) and
[validation record](../measurements/2026-11-03/elaboration-limits-validation.json).

## Observed pinned-frontend boundaries

These observations preserve frontend behavior; they do not broaden acceptance
or declare a previously unsupported design successful:

- Repeating a direct option is an input error. Within the tested flat command
  file, the first occurrence is kept. No kirimasim-side precedence shim is added.
- `--max-hierarchy-depth=4294967295` is numerically accepted but rejects even
  the recorded shallow hierarchy. Both direct and command-file routes retain
  this exit-2 diagnostic. The pinned frontend adds one in an unsigned hierarchy
  allowance, which overflows at this value. This is an upstream boundary,
  **not successful hierarchy execution**. Use practical finite limits; the
  dependency is not patched or advanced, and no upstream report was posted.
- The remaining uint32 maximum-value checks do not establish behavior for all
  possible workloads. General compatibility and complete P1 coverage remain
  unconfirmed.

No existing expectation migration is required. Frozen populations, optional
`wire real` classification, Icarus/sv-tests comparison results and nightly fuzz
records are unchanged. P1 remains in progress.

## Complete validation

Full macOS dev passed 11,587/11,587 with no failures/errors/skips (150.46 seconds).
Counts were refreshed from that JUnit in a separate commit. Final local CI
passed all 21 steps on clean `95a19956`: dev/ASan/UBSan each passed 11,587 tests,
release passed 11,588, with no skips, and clang-tidy passed for all 189 project
files. Subsequent changes touch only documentation/records and retain the
tested code hashes. This does not validate other operating systems or inputs.

After building, reproduce this matrix with
`ctest --preset dev -R '^(cli/elaboration_limits|tools/gen_elaboration_limit_matrix)$'`.
