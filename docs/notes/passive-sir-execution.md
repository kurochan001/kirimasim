# Passive-edge execution through SIR and tier 1

[日本語](passive-sir-execution.ja.md). Status: implementation checkpoint,
2026-10-26. Source bidirectional switches and complete P1 remain work.

## Grounds and scope

Implementation `9c663835`, based on main `07b7de82`, connects the existing
passive solver to typed SIR and the mechanical VM. The grounds are the approved
[MOS/switch contract 3/6/7](mos-switch-proposal.md), its
[normative mapping](p1-execution-contracts-2026-10-25.md), SIR_SPEC 3/5/7/10,
VALUE_SEMANTICS 5.3–5.6, SCHEDULER_SPEC 9/12 and IEEE 1800-2023
4.9.5/28.8/28.13/28.14. Implementation and tests are original. No simulator
implementation or upstream tests were read or copied, and no dependency,
external ABI, format version, expected output or performance budget changes.

This completes the edge/control transport missing from the earlier
[kernel checkpoint](passive-network-execution.md). It does not lower source
`tran`/`rtran`/conditional switches yet. Partial/retained port identity,
registered-net force/release, full-interval formatting and other P1 gaps remain.
No source-level Icarus gap reduction or whole-tool parity is claimed.

## Static representation and ownership

`Design::strengthEdges` holds normalized scalar endpoints, a kind, one optional
control process, a delay-shape flag and source position. Table ordinals are
stable typed `StrengthEdgeId` values, printed as `!eN`. Kinds are identity,
tran, rtran, tranif0, tranif1, rtranif0 and rtranif1. Identity and unconditional
switches have neither control owner nor delay and start on. Conditional edges
start off and require exactly one declared static cont owner; their transfer
map and polarity follow their kind.

A process's `strengthEdgeWrites` is distinct from physical origin writes and
ordinary net writes. `strength.edge !eN, control` and
`strength.edge.delay !eN, control, on, off` update only that owned edge. Controls
are captured integral scalar values of either domain/signedness. Delays are
captured integral ticks of any width, using the existing delay rules: X/Z is
zero and a known value above the time range is a runtime error. Neither
instruction produces a value or waits. A delayed update uses an owned scalar
kernel timer, not a user process or pointer into the issuing frame.

The verifier rejects invalid endpoints/kinds/positions, cross-component edges,
foreign or non-static owners, duplicate or missing write declarations, forged
operand shapes, non-scalar/uncaptured controls, mismatched delay shapes and edge
IDs in ordinary instructions. Potential uwire connectivity counts each distinct
physical origin across all possible edges, including initially off controls;
duplicate paths are not new drivers. Malformed graphs are rejected before
kernel registration. Only the existing component publisher writes net bits.

The text writer prints edge rows and ownership deterministically. Format stays
0. This is not a parser or print/parse roundtrip claim; those remain P2 work.

## Shared execution and independent evidence

The VM copies nodes, origins and edges into the existing Context-owned graph
and dispatches edge updates to `setStrengthEdge` / `setStrengthEdgeDelayed`.
`kernel::strengthConduction` maps a captured scalar control to off/on/uncertain;
`valsem::passiveConduction` independently defines the reference. The VM holds no
source-shape conditions, graph solver or approximation. The kernel retains
world correlation, physical origin identity, Active publication and inertial
cancellation/commit rules established by its earlier tests.

Seven new tests include:

- 4,560 valid SIR executions: all 120 intervals, every edge kind, all applicable
  four-state controls, opposing physical roots and both transport directions.
  All 9,120 destination intervals match the independent world/path reference.
  Expected kind/polarity choices do not call the production schema helpers.
- Thirty malformed declaration/operand variants, including 27 VM entry
  rejections before registration; potential uwire origins through initially
  off and duplicate paths; deterministic text without changing format 0.
- Forty-eight signed/domain/control cases with explicit zero delay; wide
  overflow rejection and X/Z-delay normalization; and a timed control/data
  execution checking cancellation, identical pending deadlines, current data
  while turn-off is pending and uncertain-state delay selection.

Full dev CTest passes 5,391 tests: 5,389 passed, zero failed and the two known
skips (`suite/cb/input_sample_1step`, `suite/nets/net_port_drivers_part`). The
29 existing dump expectations and PerfCount pass unchanged. README counts are
regenerated from that run, without changing the external population/probe dates.

## Next work and limits

Next normalize source primitive terminals and arrays into these static edges,
merge publication components without losing physical origins, and capture
control expressions/delays once. The existing source test expecting a bare
`tran` to be unsupported must be reviewed as a separate expectation migration
when that source path is implemented; this checkpoint does not change it.

The baseline already includes slang 12.1 and the separately applied historical
nmos expectation migration (`6f301460`). Statements about that migration still
being pending in older checkpoint records describe their original validation,
not current main. The pre-change main baseline's full local CI is recorded in
[FST setup validation](fst-dependency-setup.md#main-integration-validation-2026-10-26).
These results do not prove full P1 coverage. New external sv-tests/population,
Icarus probes, Linux/minimum compilers and graph performance are not confirmed
by this SIR checkpoint; run their existing harnesses after source integration.

## Final validation

The [machine-readable validation record](../measurements/2026-10-26/passive-sir-validation.json)
contains source hashes and check scopes. All nine new negative controls compile
and are caught by actual tests, excluding the mutation-anchor self-checks.
All runtime source bytes are restored to the implementation commit. The
swapped-delay fault is also caught separately at tick 27 after refining the
test to sample strictly between deadlines, avoiding a same-region race.

Final full builds pass in dev/release/ASan/UBSan; 48 focused strength/passive/FST
checks pass in each. All seven changed C++ units pass targeted clang-tidy,
with a test-only optional-access/time-width correction followed by a clean
rerun of that test. Changed-source/header clang-format, C++20 public headers
without exceptions/RTTI, policy checks, release optimization, existing-oracle
protection, README counts and all 751 mutation anchors pass. Every fatal
presence assertion and state/value comparison is retained.

The earlier full dev run remains the full-suite evidence; the final test-only
refinements were rerun in all four focused configurations. Full
release/ASan/UBSan suites and `tools/local_ci.py` are not run for this checkpoint.
No push or main integration of this P1 checkpoint is claimed.
