# Source bidirectional switches through the shared strength graph

[日本語](source-passive-execution.ja.md). Status: implementation checkpoint,
2026-10-26; historical expectation migration awaits human approval.

**Subsequent main integration (2026-10-26):** the exact tran expectation
migration was separately approved and applied in `77c9d188`. The
[main integration checkpoint](main-integration-2026-10-26.md) records the
post-migration validation. Failure counts, unapproved/unapplied statements and
unmerged status below describe the earlier measured checkpoint.

## Scope and grounds

All six source kinds (`tran`, `rtran`, `tranif0`, `tranif1`, `rtranif0`,
`rtranif1`) now lower to the [verified passive SIR path](passive-sir-execution.md)
and the existing Context-owned graph. Grounds: the approved
[MOS/switch contract 1/3/6/7](mos-switch-proposal.md), its
[normative mapping](p1-execution-contracts-2026-10-25.md), SIR_SPEC 3/5/7/10,
VALUE_SEMANTICS 5.3–5.6, SCHEDULER_SPEC 9/12 and IEEE 1800-2023
4.9.5/28.3.6/28.8/28.13/28.14. No new dependency, public C ABI, text-format
version, performance budget or global-state exception is introduced.

This is partial implementation of the complete approved switch contract.
Registered-net force/release, partial/retained port identities, full strength
formatting and other P1 features remain work. The existing restrictions on
ordinary fixed-array driver import remain. Source acceptance of these six
kinds is not complete switch support or full P1/Icarus parity.

## One normalization path

`Lowerer::lowerPassive` reads strict slang-bound terminal expressions and maps
static scalar net bits through the existing typed Place/physical-range model.
It handles module/generate/interface-owned nets, aliases/eligible whole ports,
packed selections, fixed unpacked elements, concatenations and primitive arrays.
A scalar actual can broadcast; a wider actual must match the primitive array.
Positions follow the actual's right edge and the instance dimensions, including
ascending and multidimensional bounds. No source text is rewritten.

The pinned slang API sometimes supplies wider lvalues for nonresistive
bidirectional switches and already selected arguments for other primitives.
The integration uses the bound width and `arrayPath`/dimensions, not a branch
for a particular spelling or failing fixture. Slang's MIT dependency binding
metadata was inspected in `PrimitiveInstanceSymbol::getPortConnections` and
its public declarations at pin `16bd3b920dccfbcde000195f9d38d9b00432c9c8`;
no Icarus/Verilator implementation or tests were read. Normalization is original
code against the approved contract and the typed API.

A conditional switch has one static cont owner. It captures its scalar control
and normalized on/off delays under the existing implicit-sensitivity rules and
executes one edge update. Range-only network work never reruns that expression.
Unconditional switches leave no executing process after endpoint normalization.
Both forms preserve source positions and use the same shared-kernel publisher.

`finishStrengthNets` imports physical ordinary/MOS roots as before, binds edge
owners and joins publication groups. Resolved readouts never become new roots.
No driver is created by a path, alias or cycle. Force-marked nets remain a
positioned unsupported boundary; illegal types, dynamic terminals and uwire
inout connections retain strict slang errors. Six original
[positioned boundary observations](../measurements/2026-10-26/source-passive-boundaries.json)
record those distinctions and the current array-width/force limitations.

## Independent graph factorization, unchanged publication

The first mixed-control vector tests exposed a real cost problem: a publication
group can include many disconnected scalar graphs. Enumerating all their
uncertain edges together forms an unnecessary Cartesian product. The live test
run was interrupted to correct the algorithm; it is not counted as a pass.

`StrengthDrivers::configureEdges` now reuses potential connectivity to prepare
independent graph groups, including singleton nodes with no edges.
`Scheduler::solvePassiveStrengthIsland` solves each group exactly with the
existing physical-origin/world/path method. Connectivity includes initially
off edges; it does not approximate a currently uncertain connection. Roots in
disconnected groups cannot meet, so their world choices can be factored without
changing any destination interval. No uncertain-edge limit or fallback X is
introduced. Complexity can still be exponential within one connected group.

Only after all groups succeed does the original component publisher commit
all values/ranges and invoke callbacks. Interrupt/resource failure still
publishes none of a partial result. Static scratch is allocated transactionally
at registration and swapped together; solving adds no per-world allocation.
Ordinary designs without passive edges keep their existing path.

A new kernel test puts 65 uncertain groups and two singleton nodes into one
132-node publication, crossing an element boundary. Each pair matches an
independent reference; every callback sees the completed 132-node snapshot,
both initially and after a root update. No user process is created.
Existing graph/resource/timer/reference tests pass too.

## Source and reference evidence

`tools/gen_source_passive_matrix.py` generates **143 original source designs**:
strength/load/direction/control matrices in module, generate and interface
contexts; 1/9/65/129-bit arrays; mixed vector controls; ascending/rank-two/concat
connections; scalar broadcast; rootless rings; correlated uncertain worlds;
mixed wired kinds; full-range-only sensitivity; delayed conduction; and all four
three-state root kinds preserving one-sided high-Z information. Oracles are
AI-drafted from independent signed-point/world enumeration and the approved
contract, not copied from another simulator's tests.

All 143 sources pass. A
[compiled fault record](../measurements/2026-10-26/source-passive-controls.json)
contains eight new faults plus a reused side-mask fault: all compile and fail
actual source/snapshot tests, excluding anchor self-checks. Sources are restored
byte for byte. The first eight restored checks cover 141 tests; the separate
side-mask repeat checks the four additional high-Z-root sources.

The [Icarus 13.0 comparison](../measurements/2026-10-26/source-passive-icarus.json)
uses compiler/runtime `13.0 (stable) (v13_0-dirty)` and records executable hashes.
It compiles 135/143 designs and matches 110 complete stdout results:

- Twenty vector-control cases differ. Twelve original reduced
  [array/scalar observations](../measurements/2026-10-26/source-passive-control-shapes.json)
  show that Icarus 13.0's explicit scalar-select/literal forms agree with the
  per-bit oracle, while its array form differs. The implementation cause is
  **not confirmed**; reproduce those sources and vary data/control widths.
- Four delayed startup results and one mixed-wired result differ under the
  explicitly approved startup/local-resolution choices.
- Four rank-two primitive arrays are rejected; four scalar-broadcast arrays
  abort reference elaboration. No cause is inferred from those exits.

The installed reference's `-dirty` suffix does not prove a pristine official
source build. No source was inspected to infer its changes. Binary hashes and
observed versions make this comparison repeatable; the complete reference
feature inventory remains unproven.

The unchanged 87 original probes were rerun against
[Icarus 13.0](../measurements/2026-10-26/icarus13-source-passive-probes.json) and
[14.0 devel](../measurements/2026-10-26/icarus14-source-passive-probes.json).
Icarus-passing gaps fall from fourteen to eight on each. Reference/oracle
mismatches remain four and seven, respectively. Remaining gaps include classes,
specify paths, the real-net extension, event-controlled NBA, partial strength
ports, automatic events and dense UDP execution. These finite probes are not a
feature-coverage percentage or proof of complete parity. External sv-tests have not been refreshed by this checkpoint.
The unchanged 1,466-path own population was
[remeasured](../measurements/2026-10-26/source-passive-population.json): 1,456 pass,
zero fail and ten remain unsupported (99.3%). The chapter-28 subset passes 22/22.
The selected source/expected paths are clean at tests revision `a733b838`;
three source files differ from the original selection revision `179c55de9c83`.
The external test repository was not modified. This is a current corpus run,
not an isolated attribution of the five additional passes to this commit alone.

## Historical expectations and publication

The existing bare tran inputs now succeed. Their obsolete rejection oracles
are preserved pending the separate
[expectation migration](passive-expectation-migration.md); the complete patch is
reviewable and unapplied. No fixture-name special case is added to preserve a
false rejection. No main integration or push is claimed by this checkpoint.

## Final local validation

The [validation manifest](../measurements/2026-10-26/source-passive-validation.json)
records source hashes and exact check scopes. All changed runtime source bytes
match implementation `76203da2`; measurement binaries retain the prior CMake
configure head in their version banner because the implementation was committed
after measurement. The manifest makes that distinction explicit.

Full builds pass in dev/release/ASan/UBSan, with 224 focused tests in each.
The final full dev run has 5,536 tests: 5,532 pass, two known skips, and only
`cli/options` / `tools/source_mos_boundaries` fail at their obsolete tran
rejections. All 29 existing SIR dump expectations and PerfCount pass unchanged.
Five changed C++ units pass targeted clang-tidy; formatting, policy checks,
C++20 public headers without exceptions/RTTI, release flags, 759 mutation
anchors and regenerated README counts pass. Full release/ASan/UBSan suites,
`tools/local_ci.py`, Linux/minimum compilers and graph performance comparisons
were not run for this checkpoint. No missing check is represented as a pass.

The separate [managed-event safepoint amendment](managed-event-safepoints-proposal.md)
was separately approved as recommended option 1 on 2026-10-26, after this
passive-switch measurement. Its complete publication protocol and evidence
remain implementation work. It concerns a future automatic-event execution step,
not this passive switch implementation. Approval of either proposal is not
inferred from general continuation or from asking about approval status.
