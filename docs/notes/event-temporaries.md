# Typed SSA event regions across native calls

[日本語](event-temporaries.ja.md). Status: component implemented and checked,
2026-10-26. Full instruction-boundary publication and source automatic-event
construction remain in progress.

## Contract and implementation

The grounds are RUNTIME_ABI 6's [event-only exception approved on 2026-10-26](managed-event-safepoints-proposal.md),
automatic-capture 5/6/8 and SIR_SPEC 5.1/7/8. The implementation and tests are
original; no external simulator source or tests were read.

`EventTemporaries` in `src/vm/event_temporaries.h` links activity-scoped SSA
regions without allocating. The VM compiles event word offsets once from SIR
types and its existing temporary layout. Each `runBody` installs a region before
executing source instructions. The descriptor stores the body base and immutable
word offsets, rather than addresses into the temporary vector: nested calls and
callbacks can grow and relocate that vector while every caller remains visible.

Only words declared as events are enumerated; numeric, real, string and reference
bits are not guessed to be roots. Null and permanent legacy event values are
separate from generation-checked registry tokens. Event-free bodies neither link
a descriptor nor scan words. Scope entry and exit clear event words only, and
normal return, waiting, disable propagation and C++ exception unwind remove the
region through its destructor. Its borrowed offsets must outlive the region;
immutable VM tables satisfy that lifetime.

The provider enumerates every linked region without allocating, executing source
callbacks or changing pins. A malformed, overlapping or truncated region returns
an error. The registry aborts the entire sweep on provider failure, including
when an earlier region/word was already marked. Runtime operations continue to
own concrete pins independently of SSA-region lifetime.

The VM registers this provider with existing region/slot collection. It is empty
at those boundaries. This checkpoint changes neither collection timing nor source
acceptance, and introduces no public kernel header, C ABI version or dependency.

## Original component matrix and controls

Seven new tests cover typed offsets and opaque aliases, nested callbacks with
forced vector relocation and an event-free intervening body, 128 recursive
regions, 1,024 repeated normal/throwing entries, stale tokens with atomic sweep
failure, seven malformed-region shapes, legacy/null values and pending pins.
They check actual registry validity/live counts, preservation of ordinary words,
retired-region cleanup and bounded physical rows under explicit caller collection.
The repeated two-event component path uses two physical rows throughout.

[Eight compiled faults](../measurements/2026-10-26/event-temporaries-controls.json)
are detected by actual component tests: omit callers, discard the caller on
callback return, omit clearing, ignore offsets, omit marking, allow overlapping
regions, trace legacy values as registry tokens and allow a partial sweep.
The [initial attempt](../measurements/2026-10-26/event-temporaries-controls-initial.json)
retains a word-offset fault that failed compilation under unused-variable
`-Werror`; that attempt is excluded from detection counts. The
[corrected offset rerun](../measurements/2026-10-26/event-temporaries-controls-offset-rerun.json)
preserves syntactic use of the index, compiles and is caught. Final controls use
the corrected fault, and restored component tests pass. None of these tests
claims VM instruction-level or source-level bounded allocation.

## Verification and cost observation

[Validation](../measurements/2026-10-26/event-temporaries-validation.json) records
full dev CTest: 5,543 total, 5,539 passed, two known skips and two failures in the
unchanged historical tran-rejection expectations (`cli/options` and
`tools/source_mos_boundaries`). The [reviewable migration](passive-expectation-migration.md)
remains unapproved and unapplied. Each of dev/release/ASan/UBSan passes all 69
focused tests, including PerfCount. Changed C++ units pass clang-tidy, changed
headers/units pass clang-format, and policy/mutation-anchor checks pass.

The [five alternating before/after pairs](../measurements/2026-10-26/event-temporaries-speed.json)
reuse our original ordinary frame/call and static named-event workloads. Median
large-minus-small estimates are 0.26335 → 0.26406 s for frame/call work (+0.3%)
and 0.04786 → 0.04811 s for named events (+0.5%). Median large-run peak RSS is
9.1875 MiB on both sides, and all counters match exactly. These observations
follow PERFORMANCE 5's five-run/startup-subtraction method on one macOS arm64
system. They are not confidence intervals or seven nightly baselines, and wall
time is not a pass/fail gate. No budget or expected value changed.

Full release/sanitizer CTest, full local CI, Linux/minimum compiler, refreshed
population/sv-tests/probes and the five-simulator speed table were not run for
this component. Existing source fixtures, dump expectations and PerfCount
oracles are unchanged. The 87-probe gap count remains its prior measurement,
not a new result or evidence of complete Icarus coverage.

## Remaining publication work

This component retains typed SSA words conservatively until the body exits;
per-instruction liveness is not implemented. Its explicit collection tests do
not prove complete VM publication. Before enabling the approved timing or source
construction, the remaining work includes:

- Publish active synchronous callee fields as well as suspended task frames;
  retain all ordinary/scoped/captured roots through cancellation and reentry.
- Own unpublished argument, result, copyback and moved-out deferred values
  across callbacks until their destination or runtime pin takes ownership.
- Define active SSA liveness at each collection point and retire inactive roots
  without invalidating values that caller/guard execution still needs.
- Permit collection before event-row growth only after every provider is complete;
  preserve DPI, termination/final and container-heap timing restrictions.
- Complete the approved source/SIR matrix, peak-row bounds, omitted-root and
  premature-reuse controls, allocation failures and ordinary-path measurement.

No pending requirement is treated as satisfied by installing this provider.
The full P1/Icarus objective remains active and incomplete.
