# Ordinary observers alongside class objects

**Clocked-body update (2026-11-04):** [Object operations in explicitly clocked bodies](class-clocked-execution.md) are now supported. Actual object dependency paths and implicit-sensitivity object computations remain unsupported.

[日本語](class-independent-observers.ja.md)

**Approved integration update (2026-11-01):** the old wait expectation is now migrated in its own commit. The [combined integration record](p1-approved-class-inout-integration.md) supersedes the pre-approval validation status below.

Status: implemented under the [approved class contract, section 7](class-runtime-proposal.md#7-event-dependencies-and-observation).
This connects ordinary observation in designs that also allocate objects. It
does not implement object-property dependency paths or complete P1.

## Boundary and implementation

IEEE 1800-2023 8.4, 9.2, 9.4, 10.4 and 21.2, SCHEDULER_SPEC 6/9/13,
and the class contract preserve the existing meaning of ordinary signal/event
observation. Previously, either frontend or loader rejected the whole design
when objects coexisted with any sensitivity process, dependency wait, monitor
or event-controlled NBA, even if the observer never used an object.

`sir::classExecutionShapeSupported` now provides one implementation-boundary
check shared by `Lowerer::checkClassExecutionSupport` and the VM loader. It
classifies observer roots and the transitive ordinary-call graph, propagating
object use through recursive call groups without recursive C++ traversal.
Front uses the result for a positioned unsupported diagnostic; the VM checks
verified native SIR before allocating runtime storage. There is no new
instruction, semantic option, scheduler behavior, dependency or ABI version.

Supported combinations include object-free `always_comb`, `always_latch`,
`always @*`, continuous assignments, `always_ff`, explicit/conditional/named
waits, monitors and eligible event-NBA layouts. Static class methods and
properties use ordinary typed slots. Waiting object methods can observe
ordinary signals/events while retaining their existing receiver roots.
Observers keep their existing scheduling and value-change behavior; in
particular, `always @*` gains no automatic time-zero execution.

The boundary remains conservative. Property/handle reads, object calls and
object-containing values in an observer's evaluation closure remain
unsupported. This includes indirect calls, iff guards, monitor callbacks and
event-NBA watch/guard callbacks. Generic refs are rejected in observer
closures and event-NBA payloads because their object-property provenance is
not connected. Implicit-sensitivity process bodies containing object operations remain
unsupported. Explicit clocked bodies use the later integration linked above.
Procedural overrides, pattern/class formatting, managed event fields and
nonstandard initialization overrides keep their previous boundaries.

## Verification

The original [generator](../../tools/gen_class_observer_matrix.py) provides
280 source programs: declaration homes in compilation units, modules,
packages, interfaces, programs and generate scopes; static/automatic handles;
direct/two-level function reads; 1/129-bit two/four-state values; ordinary
observation, NBA delivery and receiver retention across allocation pressure.
Reactive module items are not generated inside programs. Monitor expectations
check that changing an unrelated property does not add output.

Three additional frontend unit tests cover static-method sensitivity,
positioned boundaries through recursive calls, and well-formed native SIR
with an observer call redirected to an object-reading function. The latter
checks wait, monitor and both event-NBA callback forms, requiring loader
rejection before slots, heap cells, objects or activations are allocated.
Existing test sources, expectations and performance budgets are unchanged.

Five original finite Icarus probes accompany the matrix. They measure only
these coexistence combinations; they do not establish complete class parity.
See the [validation record](../measurements/2026-11-01/class-independent-observers-validation.json). The separate partial-inout expectation migration is unaffected;
slang posting remains on hold.


## Pre-approval results and former pending work

- Release, ASan and UBSan each pass all 314 selected checks with zero failures/skips. The one old expectation below is explicitly excluded from this selection.
- Full dev passes 11,068 of 11,069 checks, with zero skips. The sole mismatch is the [existing expectation migration](class-observer-expectation-migration.md): the repository still requires the old unsupported result for a constant-true wait. That expectation remains unchanged pending approval.
- The proposed copy is independently compiled/linked to the implementation and passes all 22 ClassVmExecution tests.
- All six compiled faults are caught by the new checks. After restoration, 2,050 related regressions pass, excluding the one old expectation. All 1,077 mutation anchors match; targeted clang-tidy reports no user-code diagnostics in four translation units.
- The same 161 probes run before/after with the explicitly selected Icarus 13.0/v13_0 compiler and runtime. Reference-passing gaps fall from eight to three; all five added probes pass both engines. Prior 156 sources, oracles, native statuses and stdout remain unchanged. The 21 reference mismatches stay separate.

The [before](../measurements/2026-11-01/icarus13-class-independent-baseline.json)
and [after](../measurements/2026-11-01/icarus13-class-independent-observers.json)
records retain versions, sources, hashes and results. The first default command selected Icarus 14.0;
only the rerun explicitly selecting 13.0 is adopted here. Source bytes during fault injection match
the implementation commit; only the new shared monitor-oracle filename was subsequently aligned
with its source case's stem.

All five count documents were temporarily bootstrapped for full dev, then restored byte for byte.
No successful count is published from the failing JUnit. Full 21-step local CI, count publication,
main integration and push have not run. Population, sv-tests and nightly fuzz are not remeasured.
Full regression and count publication follow approval of the existing expectation migration.
