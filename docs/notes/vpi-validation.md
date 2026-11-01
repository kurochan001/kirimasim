# First P1 VPI validation

[日本語](vpi-validation.ja.md)

Measured on macOS arm64, 2026-11-01. The [machine record](../measurements/2026-11-01/vpi-execution.json)
contains case outcomes, source and binary hashes, configuration results and compiled
controls. See the [implementation and boundaries](vpi-execution.md).

## Execution and ABI

- Native CLI: **109/109** cases: the immutable 23 reference inputs, a **70-case**
  matrix (14 source contexts × 5 operations), and 16 lifecycle, loader, error and
  compilation-mode cases. Each run compiles an original C plugin against our header.
- Kernel/frontend: **32** tests cover registration/call sites, scalar and aggregate
  references, evaluation once, declared bounds, escaped names, Context restoration,
  invalid handles, copied payloads, cancellation, X/Z comparisons, reentry, limits,
  force/release, read-only writes and expired bindings.
- The independent C11 ABI assertions compile against both our header and the
  installed Icarus 13.0 public SDK. They check sizes/offsets/constants and selected
  function types, including the const registration descriptor. The
  [ABI observation record](../measurements/2026-11-01/vpi-abi.json) retains the numeric facts.
  The public headers also build as C++20. No simulator implementation sources were read.

## Configurations

The final implementation regression at source revision `405334dc` passed:

- dev: **11,354/11,354**, no failures or skips.
- release, ASan and UBSan: **163/163 related tests each**, no failures or skips,
  including the native CLI cases and existing scheduler, activation, class,
  reference, pattern and command-line checks selected by the recorded cohort.

A subsequent test strengthens Context teardown: end callbacks run once before
library release, libraries remain live until teardown and close in reverse order.
After that test-only change, configuration metadata is refreshed and the **34 VPI
and CLI-option checks** passed in all four configurations. The machine record
keeps both binary identities; an older cached version stamp is not used as source
identity. Source hashes and the recorded revisions identify the implementation.

Existing suite bodies, stdout expectations, PerfCount expectations and the fixed
P1 population are unchanged. Counts are updated separately from a successful full
dev JUnit. The full local_ci run, Linux, full suites in the other three presets,
external comparisons and the nightly fuzz streak were not remeasured.

## Compiled controls

**10/10 compiled faults were detected**, with passing unchanged/restored cohorts
and byte-identical source restoration. They break nested Context restoration,
compiletf placement, bval comparison, read-only protection, persistent force,
delayed payload ownership, post-NBA placement, single argument evaluation,
transient-binding detachment and nullable-reference confinement.

The initial nullable-reference fixture also lacked a source location, so that
control survived. The repaired test supplies the location, requires its ordinary
reference equivalent to verify, and checks the specific escape diagnostic. Its
compiled control was rerun and detected. Compilation failures are never counted
as detection. Configuration tests and source mutation are run separately; the
intermediate ASan result affected by an overlapping mutation was discarded and
replaced by the clean results above.

Clang-format, targeted clang-tidy (LLVM 23.1.4), the global-state probe, release
flags, banned APIs, SPDX, expectation-file pairing, mutation anchors and commit
expectation protection were checked. The finite VPI evidence does not establish
full VPI/Icarus parity, cocotb integration or P1 completion.
