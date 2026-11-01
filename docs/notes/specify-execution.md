# First scalar specify execution

[日本語](specify-execution.ja.md). 2026-11-02.

The first subset of [approved contract sections 1–8](specify-p1-proposal.md)
executes through typed SIR, tier 1 and the shared kernel. `parse` remains an
elaboration-only check. The feature has no switch that silently ignores timing.

## Supported boundary

Module cells accept scalar four-state input nets, scalar wire/tri output nets,
simple/full paths, multiple scalar inputs/outputs, unconditional posedge/negedge
qualification and data/polarity annotations. Each output has one ordinary
continuous-assignment or fixed-strength combinational logic driver. Internal
procedural state may feed that assignment. Specparams/parameters, real rounding,
negative clamping, min/typ/max, 1/2/3/6/12 delay values, hierarchy/generate,
force/release, monitors, VCD and VPI reads/callbacks are connected.

Vector/select/variable/inout terminals, passive or multiple-driver destinations,
three-state/MOS/UDP destination drivers, conditional paths, explicit pulse controls,
PATHPULSE$, SDF and timing checks remain positioned errors. They are not new P1
exceptions. The `wire real` policy is unchanged. The contract's direct 2023
edition audit remains unconfirmed; the reviewed primary timing text was 2017.

## Timing and ownership

Twelve destination transitions have distinct selected delays. Omitted X arrival
uses the earliest endpoint, X departure the latest. Each actual source logic
change updates an owned clock before callbacks. Latest eligible source wins;
same-time ties choose the smallest transition delay. The absolute path deadline
and distributed functional arrival combine by their maximum within a cell.
Separate cascaded cells clock from each preceding committed public arrival.

Each original physical origin retains its committed contribution and an owned
pending complete range/generation. Identical pending work retains its deadline;
a return to committed contribution cancels the pulse; stale timers do not publish.
Due/zero updates are Active. Force clamps the public net while underlying updates
continue. Release sees the current committed contribution without advancing a
future deadline. Public port aliases, ordinary/strength reads, monitors, VPI and
VCD share the committed snapshot; private functional carriers are not reflected.
Annotations do not run or consume RNG state.

SIR rows bind numeric instance/source/public/private/origin/owner identities and
twelve normalized delays. Positive overflow stays explicit until selection.
The verifier rejects malformed bindings/ownership before VM slot allocation.
Foreign net-reference capability is restricted to VPI net bindings; it cannot
escape into a language write or writable foreign variable. The kernel adds no
slang/SIR dependency or public C ABI change; public headers remain C++20.

## Evidence

The native matrix has **105 successful executions and 16 expected positioned
rejections** (121 cases), covering all 60 transition/arity pairs and stimuli in
initial/always, static/automatic tasks, functions, fork, generate, interface,
program, package and ref contexts. Directed cases cover force deadlines,
inside/outside visibility, strength snapshots, multiple outputs, generate
instances and cascaded cells. The **82 new unit tests** cover independent
value references, runtime timing, wide source addresses, resource retry,
isolation, malformed SIR and VPI/VCD coherence.

The [15 compiled native controls](../measurements/2026-11-02/specify-native-controls.json)
all built and were caught by semantic execution or ownership checks, including
wrong X choice, source selection, additive delay, zero region, stale timers,
private/public port confusion, executed annotations, forced pending loss and
VPI notification loss. Sources were restored and both native baselines passed
again. Registry/self-check failures and build failures are not counted.

The [same 176 probes](../measurements/2026-11-02/icarus13-scalar-specify.json)
retain every source/oracle/P1 classification. `specify_path` now prints the
unchanged expected early 0/late 1. Dense UDP frontend timeout remains the one
required-scope gap; optional `real_net` and 25 reference mismatches stay separate.
This finite inventory does not complete P1 or the remaining timing coverage.
The [102-case inventory](../measurements/2026-11-02/specify-execution-inventory.json)
records 83 native oracle matches, 16 unsupported inputs and three executions
without a frozen oracle. Those observations are not relabelled as native passes.
Population, sv-tests and Icarus 14.0 were not remeasured.

The [single historical expectation migration](specify-expectation-migration.md)
was separately approved and committed, preserving its input bytes. An intermediate
integration changed diagnostic counts on unrelated unsupported declarations;
`4aa68356` restores the previous diagnostic walk. The original diagnostic records
and CLI options matrix pass unchanged. Counts are refreshed only from a complete
successful dev JUnit, separately from implementation. Full local CI precedes push.

Complete dev verification passes **11,447/11,447**, with zero failures/skips (130.23 seconds). [Validation metadata](../measurements/2026-11-02/specify-execution-validation.json) binds the frozen source/oracle checks, restored controls and successful JUnit; the count-only commit is `b62cf990`. Historical population/sv-tests/14.0 records retain their dates.
