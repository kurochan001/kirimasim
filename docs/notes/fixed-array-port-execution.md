# Fixed-array ports and sparse net resolution

[日本語](fixed-array-port-execution.ja.md). Status: implemented subset,
2026-10-25; P1 remains incomplete.

## Integrated validation

The [final record](../measurements/2026-10-25/fixed-array-final-validation.json)
at `d89a60c6` passes all 21 local-CI checks. Dev/ASan/UBSan pass 4,541/4,543
and release 4,542/4,544, with zero failures and the same two known skips.
The clang-tidy correction preserves [identical SIR for all 358 designs](../measurements/2026-10-25/fixed-array-lint-sir-equivalence.json).
Separate records retain the [13.0](../measurements/2026-10-25/icarus13-fixed-array-probes.json)
and [development](../measurements/2026-10-25/icarus14-fixed-array-probes.json)
cohorts, [358 reference observations](../measurements/2026-10-25/fixed-array-reference-matrix.json),
[59 proposal rechecks](../measurements/2026-10-25/fixed-array-proposal-recheck.json),
[sv-tests](../measurements/2026-10-25/fixed-array-svtests.json) and
[P1 population](../measurements/2026-10-25/fixed-array-population.json).

## Contract and implementation

The user approved [fixed-array-ports-proposal sections 1–6](fixed-array-ports-proposal.md)
on 2026-10-25. Its normative mapping is [recorded here](p1-execution-contracts-2026-10-25.md).
The rules are our paraphrases of IEEE 1800-2023 7.6, 23.3.3.2–7 and the
existing SIR_SPEC 3.5/10 and VALUE_SEMANTICS 5 contracts. No other simulator
implementation or tests were read, no external idea was adopted, and no
new dependency, installation or global state was added.

`Lowerer::mergePorts` makes whole compatible net-array connections share one
physical slot. A typed SIR `Slot.viewOf` retains each different declaration's
own bounds, rank and leaf type; corresponding declaration-order ordinals
share storage. Views point directly to an earlier root, including transitive
connections. VM maps the explicit numeric view to the existing kernel slot;
it invents no source-dependent meaning. Index validation uses the view before
ordinal lookup, including negative, wide and X/Z indices. Read/write sets
are normalized to the physical slot. Constant integral array-element force
and release, including packed selections, use that same kernel storage.
Scalar port-force and explicit unpacked-array `alias` restrictions remain.

`lowerPorts` uses `assignAggregate`, `copyArrayStorage` and `flushStores`
for noncollapsed value connections. All RHS values precede destination
writes; ordinal matching preserves both declared ranges. Integral, real,
shortreal and string values update through the existing continuous process.
Module/interface origin stays unchanged and program port processes retain
their reactive identity. Aggregate conditionals use block arguments: a known
condition evaluates one branch, X/Z evaluates both, and shared value semantics
merge the captured leaves. Hierarchical fixed-type queries use the declared
type without evaluating the signal expression. Port defaults use the
elaborated definition-scope expression. `unconnected_drive` supplies one
pull-strength implicit process writing each integral input-net element;
variables keep their existing typed-default warning policy.

`Lowerer::resolveArrayNet` calls the existing valsem-backed `resolve` operation
per affected physical element with each driver's own strength. Private driver
slots contain only covered elements: `driverElements` maps private ordinals
to physical ordinals. Initial covered bits are X and uncovered bits Z. Partial
packed writes retain their bit coverage. Built-in tri0/tri1 and supply kinds
use the existing scalar rule per element. Constant-element inertial delays
use ordinary `drive.delay` ranges. There is one resolver process per net;
no user process or RNG is invented per element. A 262,144-element probe with
64 single-element drivers has 64 private cells, rather than 64 full arrays.
The verifier checks direct roots, shape, coverage order, domain, write sets
and alias-aware writer conflicts; dimension products reject overflow.

SIR format remains 0 and the additive metadata prints deterministically as
`view-of=%sN` and `driver-elements=[...]`. Existing format-0 dumps remain
unchanged. No C ABI or kernel public-header change is required; ABI stays
0.3 and tier 1 continues to call kernel C++ directly.

## Verification and reference observations

[The original generator](../../tools/gen_fixed_array_port_matrix.py) makes
358 source cases and derives expected bytes from ordinal correspondence,
bit coverage and strength dominance, never from reference output. Four new
hand-built SIR tests exercise shared storage in two independent Contexts,
65-bit values, malformed/cyclic views, coverage and uwire conflicts.
[The boundary test](../../tests/tools/check_fixed_array_port_boundary.py)
checks sparse allocation, eight enum/packed-record bit-force/query paths
and five strict/unsupported boundaries. All 364
selected tests pass. The subsequent typed-query/force expansion also passes. Eight compiled negative controls are caught by actual
failures (84, 3, 125, 108, 48, 9, 4 and 1 tests respectively); restored sources
pass the same selection. All 536 registered mutation anchors apply.

The matrix spans 1/8/65/129-bit values, two-state variables and four-state
nets, different bounds/directions, rank two, module/generate/interface,
program origin, continuous input variables, copied outputs, shared inout,
transitive aliases, force/release, typed inputs, conditional value capture,
partial writes, pulls, defaults, per-element wired logic and inertial delay.
The 59 original proposal observations now have 51 model matches, seven
unchanged legality rejections and one still-unsupported array-slice connection.

Both installed references were rerun on all 358 designs: each matches 184,
differs on 152 and fails compilation on 22. The differences comprise 96
explicit-input-variable reevaluation cases, 48 collapsed-strength cases and
eight supply-array cases. These remain observations against the approved
project contract, not proof of a reference bug. Some legal typed/program and
force examples fail reference compilation. Expected values were not changed
to follow those results. On the unchanged broader 87-probe cohort, only
`fixed_array_ports` and `resolved_net_array` become passing; each reference's
passing gaps fall 24 → 22. Older native passing judged bytes and all reference
statuses are unchanged; reference mismatch counts remain four/seven.
The pinned 344 sv-tests remain 244 native passes and the P1 population remains
1,451/1,466, zero failures and 15 unsupported. Linux was not rerun.

## Remaining boundaries

Partial-array port connections, ref ports and unimplemented structure/object
leaf ownership still need work. Strict slang rejects explicit two-state nets
and chandle ports; its diagnostics are preserved. Net-array three-state side
resolution is explicitly rejected where multiple drivers meet. Moving driver
coverage is not enabled (strict slang also rejects the sampled nonconstant
net targets); out-of-range resolved driver targets remain unsupported.
Explicit array aliases, bare unpacked-net declaration initialization and
scalar port force retain their existing diagnostics. These limits do not
close P1; the other approved execution contracts and the full feature/interface
inventory still require implementation and validation.
