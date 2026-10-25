# Passive strength worlds in the shared kernel

[日本語](passive-network-execution.ja.md). Status: implemented kernel checkpoint,
2026-10-26. Source passive switches, their SIR connection and full P1 remain work.

## Grounds and scope

`a2aa7573` extends the existing Context-owned origin bank and component publisher
with immutable passive graph records, exact uncertain-edge worlds and scalar
conduction timers. Grounds: the approved [MOS/switch contract 1/3/6/7](mos-switch-proposal.md),
its [normative mapping](p1-execution-contracts-2026-10-25.md), VALUE_SEMANTICS
5.3–5.6, SCHEDULER_SPEC 9/12 and IEEE 1800-2023 4.9.5/28.8/28.13/28.14.
The implementation and tests are original. No other simulator implementation or
tests were read, no standard text/examples were copied and no dependencies or
tools were introduced. The external C ABI, formatVersion, GC collection timing,
performance budgets and strict slang diagnostics are unchanged.

This is a real scheduler/Store execution path, rather than an isolated graph
calculation, but the SystemVerilog front end does **not** emit passive edges yet.
SIR edge/control/delay declarations, verifier/VM transport, source primitive
normalization, partial/retained port identities and registered-net force/release
remain required. `%v` full-interval formatting and the other P1 gaps remain too.
No new source-level Icarus gap reduction or complete switch support is claimed.

## Static ownership and exact resolution

`configureStrengthDrivers(nodes, origins, edges)` copies an optional edge table.
Each edge has two normalized scalar node IDs, an identity/nonresistive/resistive
map and an initial off/on/uncertain conduction state. Its stable ID is its table
ordinal. Endpoints must belong to the same declared publication component;
a component may still contain disconnected nodes, as with packed-vector
publication. Conditional-device startup off and unconditional startup on remain
front/SIR producer obligations under the approved contract.

Registration validates IDs, maps and states before changing the live bank.
Potential connectivity counts each distinct physical origin once for `uwire`,
even across an initially off edge, while multiple paths of one origin do not
create multiple drivers. Explicit implicit-root policy is unchanged: the caller
materializes pull/supply roots once and uses only effective core net kinds.
Configure owns all rows and prepares adjacency, world-choice, routing and
resolution scratch with the Context's memory resource. Failure leaves a
retryable empty registration, with no observable write or partial graph.

`Scheduler::solvePassiveStrengthComponent` computes each fixed conducting world
before taking the result hull:

1. Treat each uncertain edge as an independent binary choice. A carry vector
   enumerates choices without a machine-word device limit or approximate cutoff.
2. For each physical origin, propagate an eight-entry magnitude map through
   that fixed graph. A bounded queue retains the strongest reachable route
   for each source magnitude. The monotone maps never amplify, so improvements
   are finite even through cycles; identity edges preserve supply rank.
3. At each destination, fold that origin's exact mapped point set into the
   existing point-choice resolution states, counting it once across all paths.
   Point-set holes are not filled by a premature interval/path hull.
4. Apply that destination's local net-kind tie rule to all physical roots.
   Only then accumulate the hull across different uncertain-edge worlds.

Resolved intermediate readouts never become roots. For example, equal opposite
roots on wire/wand/wire publish X/0/X under the approved project interpretation;
there is no additional zero driver created by reading the middle node. A shared
uncertain connection transporting opposite roots together can keep a destination
known 1 with range `[1,6]`; independently hulling the two transported origins
would instead introduce a spurious X. Removing the last nonzero physical root
makes even a closed passive ring Z. The all-zero-root shortcut is exact for every
world and does not preserve old published values as charge.

Only the existing component publisher commits the completed range/value snapshot,
before callbacks. Callback-driven updates queue a distinct later Active solve;
no user code runs during world/path resolution. Components without passive edges
retain the earlier independent-node resolver. This avoids requiring a graph for
ordinary source designs or the earlier directed-MOS path.

## Conduction updates and resource boundaries

`setStrengthEdge` and `setStrengthEdgeDelayed` update owned three-way state by
numeric edge ID. Root and edge state now use common direct/inertial helpers:
identical pending values keep their deadline, different values cancel by
64-bit generation, committed reversion schedules no replacement, and failed
scheduling/overflow cannot replace an existing pending value. New on/off states
use their respective delays; uncertain uses the minimum. Explicit zero commits
in Inactive and queues component work in Active, including a Reactive issuer.

An edge delays conduction, not transported data. While a turn-off is pending,
new root data uses the still-conducting edge immediately. When a delayed turn-on
commits, it carries the then-current roots, not data captured at registration.
Timers contain numeric IDs/generations and no issuing process/frame ownership;
they add no synthetic fork children or RNG state. Scalar edges retain independent
pending values and deadlines.

Prepared solver scratch is linear in nodes, edges and roots, with no per-world
Cartesian result allocation. Exact world enumeration can still require
exponential time in the number of uncertain devices; this is not a scalability
claim. The existing interrupt/wall-clock budget is checked within work. A stop
before completion publishes none of the partial world accumulator. A private
poll wrapper maps allocation failure while forming a wall-limit note to the
existing resource-limit termination, preserving the no-exception boundary.
Publication continues to report commitment atomically under resource failure.
No extra budget, heuristic tolerance or hidden CLI switch was added.

## Independent evidence

`valsem::resolvePassive` is a separate slow reference: it enumerates root-point
combinations and simple paths in each edge world, then uses the reference local
resolver. It does not share the kernel's adjacency work queue, magnitude-table
propagation or compressed point-state fold. It separately checks potential
`uwire` reachability. It is not linked into the kernel.

Nineteen new tests include:

- 10,800 interval/opposing-point states: all 120 intervals × 15 points × six
  core destination kinds, with identity/nonresistive/resistive paths and
  uncertain reconvergence; and 3,645 three-edge transfer/control/load states.
  Their **46,980 destination intervals** match the independent reference.
- Exact shared-world correlation, positive known intervals and range-only
  notices; local mixed-kind resolution; a rootless ring whose callback changes
  the next snapshot; 130-node resistive transport across 65-bit element bounds
  in both directions; and static `uwire` origin identity/disconnected ownership.
- Six malformed graph configurations with retry, copied input tables, 48
  memory-resource budgets with failure/success and atomic failed publication,
  all nine conduction state transitions, pending equality/cancellation/reversion,
  live data during delayed conduction, independent scalar deadlines, explicit
  zero/Reactive ordering, and time overflow retaining the prior pending state.
- More than 64 uncertain edges with empty roots, and a nonzero-root case with a
  wall budget. That test accepts exact completion or honest interruption with
  the entire old snapshot; it does not require an exponentially slow algorithm.

[Fifteen new graph faults plus thirteen existing origin faults](../measurements/2026-10-26/passive-network-controls.json)
all build successfully and fail actual tests. The existing faults exercise the
newly shared timing helpers too. There are no compile failures counted as
fault detections. Restored production passes **300 focused checks**, including
all 154 earlier directed-MOS source designs, six source boundaries, the 29
unchanged dump goldens and PerfCount. The range-only test checks that
`slot.changes` stays fixed while a complete component publication still counts
its per-target `slot.writes`; a write attempt is not a value-change notice.
No existing expectation or assertion was changed or weakened.

## Validation and remaining work

The [full local CI record](../measurements/2026-10-26/passive-network-validation.json)
completes all 21 checks: **17 pass and four preset test steps fail only
`cli/options`**. Dev/ASan/UBSan pass 5,380/5,383; release passes 5,381/5,384.
Each has exactly one failure and the same two known skips
(`cb/input_sample_1step`, `nets/net_port_drivers_part`). All new graph tests,
earlier source MOS, unchanged dumps/PerfCount, Werror builds, policy checks,
clang-format and all 110 clang-tidy sources pass. A standalone public-header
probe also compiles as C++20 with exceptions/RTTI disabled. The manual
`a2aa7573^..a2aa7573` expectation check passes; the automatic check has no
upstream at validation time.

The separately proposed [nmos expectation migration](mos-expectation-migration.md)
remains unapproved and unapplied. Its unchanged rejection is the only failing
CTest case in every preset. This kernel work does not authorize that migration
or the independent managed-event instruction-safepoint amendment.

[Five alternating ordinary-path pairs](../measurements/2026-10-26/passive-network-speed.json)
compare release `e7e2ec59` with `a2aa7573` after CI, at 200,000 iterations with
small-run startup subtraction. Frame access is 0.260870→0.263807 s (+1.1%);
event bounce is 0.047777→0.048948 s (+2.4%). All counters match. Median peak RSS
is 9.265625→9.1875 MiB and 9.1875→9.1875 MiB, respectively. These few loaded-machine
observations are not a stable regression/speedup proof or seven nightly baselines.
Neither workload uses passive edges; graph/world throughput remains unmeasured.
No threshold or budget changed.

The user requested a checkpoint for commit/push and stopping. The tested branch
is `codex/p1-event-sir`, based on `62148877`, with slang configure version
`12.0.561+3b351a52b` (pin `3b351a52b15755b896d3b0144ae0432547d86618`).
At fetch, `origin/main` is `6432683f`, including a separate slang 12.1 merge.
That integration is not in this tested branch; merging and revalidating it
remains a separate next step.

Next connect static edge/control/delay ownership through verified SIR and the
mechanical VM, then normalize all six source bidirectional kinds and structural
ports without losing physical origin identity. Registered-net force and full
strength formatting remain required under the same approved contract.
Global allocator exhaustion and 64-bit generation exhaustion are not exhaustively
executed; the tests cover the scoped memory resource and actual time overflow.
Use a future scoped allocation seam and controlled generation injection to
strengthen those checks without adding mutable global state. No external
sv-tests/population or new Icarus probes, Linux/minimum compiler run or complete
five-simulator speed table is claimed by this kernel checkpoint.
