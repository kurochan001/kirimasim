# Full-strength MOS and bidirectional-switch execution proposal

[日本語](mos-switch-proposal.ja.md). Status: **pending review**, 2026-10-24.
The English text is the proposed contract. No implementation is authorized by
this document alone.

## Scope and reason for review

Implement all six directed MOS forms (`nmos`, `pmos`, `rnmos`, `rpmos`, `cmos`,
`rcmos`) and all six bidirectional forms (`tran`, `rtran`, `tranif0`, `tranif1`,
`rtranif0`, `rtranif1`). Include primitive arrays, structural bit connections,
full resolved intervals, strength-only propagation, force/release, delays,
strength-preserving partial net ports and real strength formatting. These
mechanisms account for thirteen switch probes and one partial-port probe in
the 20 Icarus-13.0-passing gaps in the 79-item inventory at proposal base
`b5491de`; the probe count includes duplicates
of basic mechanisms and is not a feature-coverage percentage.

[P1_SCOPE](../spec/P1_SCOPE.md) advances their scope but explicitly requires
review of missing detailed semantics. Amend VALUE_SEMANTICS 5.3–5.6/5.8,
SIR_SPEC 3.1/3.5/4.1/5, SCHEDULER_SPEC 9/12/14 and SYSTEM_TASKS 3.10 as
specified below. Existing four-state projection, local strength resolution,
ordinary event semantics and strict slang diagnostics remain authoritative.
This is a proposed execution contract, not a claim of P1 completion.

## 1. Complete signals and physical drivers

A strength signal is one existing `StrengthRange` per normalized packed bit:
signed endpoints in −7…+7, with `lo <= hi`. Its four-state value is exactly
the existing `strengthValue` projection. Keep both endpoints even when that
projection is a known 0 or 1. No strength consumer may replace a range by its
largest magnitude or reconstruct it from the projected value.

Each physical driver output bit has a stable origin ID, its current complete
range and its underlying update state. Ordinary assignments, gate/UDP outputs,
MOS outputs and implicit pulls/supplies are physical roots. A path through a
passive network is not a new driver. Materialize each implicit root once;
do not add it again when applying the existing local resolver. Legacy APIs
that infer implicit pulls from the net kind retain their existing behavior.
Apply eligible port collapse and its effective net kind before constructing
implicit origins; retained distinct nodes each keep their own implicit roots.

Preserve current ordinary-driver initial values and side-field initialization
when importing their contributions. Existing net initialization remains until
its first scheduled resolution; loading metadata must not expose an already
resolved value before that scheduled work. Section 6 separately specifies the
new MOS and conditional-edge startup choices.
In particular, importing an ordinary previously unresolved single driver starts
from its existing initial four-state value at its static strengths; an unused
side field must not turn that initial X into Z just because metadata is now
observed. Already resolved legacy driver tuples keep their existing mask
initialization. These import cases must preserve the prior value behavior.

Store complete ranges for network nodes and every net whose strength can be
observed by a device, port, formatter or future VPI request, including nets
with one driver. Mark all net-reference actuals of runtime-format calls, not
only calls whose constant format visibly contains `%v`. Optimization may omit
metadata only when all observable values, events and strength reads are
equivalent. Strength does not become a new SystemVerilog variable type.

## 2. Directed MOS operations

Use 28.7/28.9 truth behavior with full input intervals. An enabled device maps
every input strength point through the selected reduction. A disabled device
contributes only zero strength. An uncertain control admits both the enabled
contribution and no contribution; retain their hull. In particular, data Z
stays Z even with X/Z controls. For `cmos`/`rcmos`, combine the parallel n- and
p-controlled legs: one definitely conducting leg suffices to pass the data;
both definitely nonconducting legs give Z. Apply the reduction once to the
input contribution, not successively through those parallel legs.

For nonresistive devices, cap magnitude 7 at 6 and preserve other magnitudes
(28.13). For resistive devices, the output magnitude for input ranks 0…7 is
`[0, 1, 1, 2, 2, 3, 5, 5]` (28.14). Preserve sign. The same maps apply to
bidirectional edges. No device assigns its own arbitrary output strength.

A MOS output is an owned, event-driven driver contribution. Recompute it on
data-range or control-value changes. Do not feed it into a passive-network
solver as an undirected edge. Directed feedback can retain a previously
propagated value; the passive no-root rule in section 3 does not erase it.

Following 4.9.6, a structural data-net reference retains its strength. Other
data expressions use an ordinary implicit continuous-assignment carrier;
variables and expression results enter that carrier with the existing static
strength policy. Lower nontrivial control expressions to value carriers too.
An internal data-strength notice must not reexecute an unrelated control
function, repeat its side effects or consume its RNG state.

## 3. Passive networks and uncertain controls

Normalize bidirectional terminals to static scalar net-bit nodes after strict
slang binding; use elaborated primitive-array element mappings and source
positions. Dynamic terminal indices rejected by slang remain errors. Edges
have two node IDs, a nonresistive/resistive transfer map and a committed
conduction state: on, off or uncertain. Unconditional edges are always on.

The reference meaning of a component is:

1. For each assignment of on/off to its uncertain devices, solve a fixed
   conducting graph. Devices are independent choices, including complementary
   controls that happen to read the same X. Do not introduce symbolic Boolean
   correlation absent from the four-state control model (4.9.5).
2. For a fixed graph and one chosen strength point of a physical root, find
   its strongest reachable contribution at each node. Multiple paths of the
   same origin count once, selecting the greatest reachable magnitude for
   that origin point. Identity port edges do not attenuate; switch edges use
   section 2. No path amplifies strength, so cycles cannot improve a route
   indefinitely. An eight-entry magnitude map is sufficient for each
   root-to-node transfer in that fixed graph.
3. Resolve the independent physical origins at each node with existing
   VALUE_SEMANTICS 5.4 and that node's net kind. Keep choices of a root point
   consistent across its multiple paths. Resolve before combining different
   uncertain-device worlds; taking path or driver hulls earlier can lose
   correlations and create spurious X.
4. The node's complete result is the hull across worlds. A logical level
   common to all worlds stays that level; otherwise its projection is X.

This proposes **local net-kind resolution at the destination**, not a new
driver created by an intermediate `wand`/`wor` readout. Uniform `wand` nodes
with equal opposite roots resolve 0; uniform `wor` nodes resolve 1. A mixed
wire/`wand`/wire chain with equal 1 and 0 roots gives X/0/X. These are explicit
project interpretations for review: the observed reference programs instead
give X throughout, and mixed-kind channel handling is not confirmed by that
agreement. Preserve the approved strength-first wired tie rule; do not adopt
the references' different ordinary wired-strength policy either.

Recompute from current roots whenever roots or conduction change. Cached
resolved node values never become roots, so removing the last external drive
from a passive ring makes it Z. Static `uwire` checks count distinct possible
physical roots, not paths, across potentially conducting connections. Keep
all stricter slang diagnostics; do not defer an illegal connection until run
time. Force overrides are not additional ordinary drivers for the `uwire` count.
Charge storage and user-defined-net resolution need their own contracts;
both installed references reject the original boundary examples in the
supplemental data, which is not proof of their entire feature inventories.

## 4. Partial net ports and observability

Extend the existing collapse rule to structural bit/part/concatenation mappings
whose two sides are nets with matching aggregate width, signedness and state
domain. Use elaborated positional mapping, including ascending declarations
and multidimensional packed bounds. No bitwise mapping may introduce a
conversion. Apply Table 23-1's effective net kind per eligible collapsed bit,
retain names as aliases, and preserve physical origins. A partial collapse is
an identity connection, not `tran`: supply strength remains supply.

Keep the existing noncollapse rule whenever collapsing changes force/release
or required observability (SCHEDULER_SPEC 14.4). For retained net-net inout
connections, keep distinct nodes with identity edges and no attenuation. For
retained input/output connections, keep their existing directed implicit
continuous assignments and strong conversion policy; do not replace them by
bidirectional edges. Conversions, variables and nonstructural expressions
also keep their existing directed carriers. This explicitly selects the
existing uncollapsed semantics where collapse is prohibited, rather than
silently losing strength on an otherwise eligible identity connection.

The supplemental input-port force example produces `00` in both references;
the parent-bit output-port force example produces `01`. Thus the older
annotation that Icarus never collapses input ports must not be treated as a
universal observation. The project still follows its existing reviewed
force-observability rule, even where optional collapse changes a reference
result. Record such choices separately from missing execution support.

## 5. Force, release and transit

For a forced net bit, suppress its ordinary and implicit local roots from the
network and insert a virtual root for the force's current four-state value at
strong rank 6. The forced bit's own published range is exactly that force
contribution, overriding remote contributions at that readout too; its value
is the projection of that range. Forcing Z therefore publishes `[0,0]` there
and does not create a nonzero drive. This strong-force rule and its transport
are proposed project choices; the current specification does not define all
strength details.

Remote roots can still pass through the forced node. It is not a routing
barrier. Consequently forcing a transit node to Z need not stop a remote 1
from reaching a third node. Resistive edges attenuate a virtual force root
in the same way as another root. Force values are value expressions; they do
not inherit the source net's strength.

Continue updating underlying ordinary drivers and their pending propagations
while force is active. On release, remove the virtual root and immediately
resolve from those current underlying contributions before the next statement
observes the released net. Do not restore a cached pre-force result. Preserve
the existing per-bit override ownership and overlapping-force rules. Publish
all fields of one committed override update before invoking its callbacks;
no callback may see a new value paired with stale strength.

## 6. Scheduling, startup and inertial state

Internal subscriptions distinguish complete-range changes from four-state
changes. A strength-only update wakes dependent devices and network work,
but ordinary `@`, edge controls, waits and VCD value notifications still use
the existing projected-value rules. A full-range comparison must happen even
when the four-state store reports no value change.

Queue dirty components once in Active, including when a root changes from
another scheduling region. Solve a complete component against one committed
input snapshot, then publish all node ranges and projected values before
callbacks. Callbacks may queue subsequent Active work; do not run user code
inside the solve or partially publish nodes. Component-level atomic publication
is this project's proposed selection among the processing orders allowed by
4.9.5. Several root updates before the queued solve can share its input
snapshot, as with ordinary continuous resolution. Distinct committed component
publications keep their own value-change notices; do not retrospectively merge
or erase an already published intermediate transition.

**Startup choice requiring approval:** new directed MOS contributions start
at `[0,0]`, and conditional passive edges start off until their first control
propagation commits. Unconditional edges start on. Queue an initial component
resolution before first MOS evaluation, so a rootless directed loop can begin
floating rather than inventing a source. Ordinary-driver initialization is
unchanged. An explicit MOS delay retains that initial undriven contribution
until the first complete propagation is due. This is a new MOS-specific
exception to the ordinary initial-X driver rule. Its IEEE interpretation is
**unconfirmed**; it deliberately does not claim to reproduce the references'
delayed startup observations. Approval must include this choice before code.

For MOS, retain one committed complete range and at most one owned pending
range per scalar output. An identical pending result keeps its deadline;
a different result cancels it; reverting to the committed result schedules
nothing. A genuinely new result schedules from the current simulation time.
Compare all endpoints, not just their projection. Use rise/fall/off for a
new 1/0/Z result and the minimum for X/L/H; one/two/three delays normalize as
in 28.7/28.16. For a strength-only change with the same known value, propose
that same destination-value delay classification. This latter case is an
explicit project interpretation. No delay commits in the existing Active
evaluation; an explicit zero delay uses the existing Inactive policy.

Conditional bidirectional devices delay **conduction changes**, not transported
data. One/two delays specify on/off; a new uncertain state uses their minimum
(28.8). Use the same pending-equality, cancellation and committed-reversion
rules for the complete three-way state. Data propagates through a committed
edge without an additional path delay.

The [delayed three-state side proposal](delayed-gate-side-proposal.md) remains
separate. Its source boundary stays unsupported until that amendment is
approved. The complete-state ownership/cancellation machinery should be
shared where both contracts apply; this proposal does not silently authorize
that other pending source path or change its startup fields.

## 7. SIR and shared-kernel contract

Add a non-language SIR value type `strength<w>` holding `w` complete ranges.
It has no arithmetic signedness and integer operations cannot consume it. Add:

- `strength.make`: four-state words, static ranks and an optional two-state
  `2w` side mask → ranges, using existing driver interpretation.
- `strength.load`: a statically identified net range, optionally selected by
  captured typed runtime indices → owned strength value. Evaluate selectors
  once with their value read; unknown/out-of-bounds reads retain the existing
  X result and use strong-X metadata rather than selecting an arbitrary node.
- `strength.value`: strength value → `i4<w>` through existing projection.
- `strength.mos`: device kind, strength input and scalar control values →
  ranges through the pure shared-kernel operation of section 2.
- `strength.drive` / `strength.drive.delay`: a declared driver ID and complete
  result, with normalized delays for the latter. Each scalar device output
  has its own pending state; primitive arrays do not become vector-wide
  inertial timers. Registration copies all required fields before returning.

Add static network rows for bit nodes, effective net kinds, physical origins,
identity/switch edges, control bindings, delay descriptors and source positions.
Declare internal strength sensitivity separately from ordinary sensitivity.
Only the designated kernel component publisher writes a network net; ordinary
driver processes write their owned contributions. Preserve the existing
write/drive-range distinction and source-level multiple-driver checks.

The verifier checks widths, endpoint validity, net eligibility, normalized
bit mappings, legal device kinds/delay counts, control types, driver ownership,
declared write coverage, unique IDs and complete operands. Reject metadata in
ordinary arithmetic, unsupported conversions, foreign targets and borrowed
SSA values crossing waits. Add deterministic text roundtrips and malformed
SIR cases. This internal encoding does not select a new external C ABI.

Context owns the graph, origin/range stores, pending generations, subscriptions
and their cleanup. The VM mechanically executes typed SIR and transports
operands to C++20 kernel interfaces. The kernel imports neither slang nor
SIR nor LLVM. Timers and dirty work contain copied IDs/ranges, not pointers
to temporary interpreter values. Destroy registrations before frame teardown;
these POD strength values add no GC root category. Device work creates no
hidden fork child and does not retain an issuing user thread or change
`wait fork`, program completion, origin or RNG ownership.

## 8. Strength-aware formatting

Replace SYSTEM_TASKS 3.10's provisional always-strong rendering for net
references with complete captured intervals (21.2.1.4). Format `[0,0]` as
`HiZ`; format a nonzero point with the rank mnemonic and 0/1. For a known
value spanning several strengths, print maximum then minimum magnitude and
0/1. A zero endpoint with one nonzero side uses that rank's mnemonic and
L/H. Opposite endpoints use the mnemonic and X when magnitudes match,
otherwise zero-side then one-side rank digits followed by X. Thus an input
range `[3,6]` is `631`, not `St1`.

For accepted non-net integral values, preserve the existing strong fallback.
Do not copy the reference runtime's warnings for literal actuals. As an
explicit compatible extension beyond the standard's scalar `%v`, propose
formatting vectors from most significant bit to least, joining scalar
tokens with `_`, as observed in both references. Apply field width to the
whole joined string; `%0v` does not remove signal tokens.

Capture strength metadata at the same argument-evaluation point and in the
same order as its existing value argument. Strobe/monitor evaluation follows
their existing region rules for both fields. Runtime formats retain actual
net provenance through typed operands; never recover it by a signal-name
lookup. A future VPI strength read uses the same kernel metadata, but VPI's
external interface is not declared implemented by this amendment.

## 9. Exactness, resources and validation

The slow reference solver enumerates uncertain-device worlds and root points;
the fast solver may cache fixed-world transfer maps or use shared decision
structures, without adding a dependency. It must agree with that reference,
including permutations, duplicate paths, cycles, mixed net kinds and force.
Do not replace large components with unconditional X, truncate origins, cap
unknown switches silently or infer origins from cached net readouts.

Check dimensions and arithmetic before allocation. Poll existing stop and
wall-clock requests during long graph/world walks. An explicit existing
resource limit or allocation failure returns the kernel's resource error;
no partial component publication or stale callbacks may survive. No new
dependency, installation or implicit solver-work cutoff is selected here.
Device and component work uses normal counted scheduler events, so existing
slot/zero-delay guards can detect real oscillation rather than bypassing it.
Keep ordinary designs on equivalent fast paths; existing performance-count
expectations must not be edited to hide extra or missing execution.

Acceptance includes all six forms of each family; all data/control values;
all ranks, interval endpoints and reduction chains; wide and unaligned
primitive arrays; duplicate-path diamonds and rootless rings; structural
ports, conversions and force observability; strength-only wakeups without
ordinary events; directed feedback; force Z and transit; delayed startup,
one/two/three/zero delays, cancellation and identical pending deadlines;
runtime formats and all interval-format cases; cleanup, reentrancy, isolation,
interrupts and allocation failure. Use independent valsem/kernel/SIR tests,
original source matrices and compiled negative controls that fail actual new
outputs. Existing expectation changes, if a conflict is found, require their
own reasoned review/PR; approval here is not blanket permission to rewrite
tests. Add new public headers to the existing kernel C++20 header check;
preserve the generated-code exception/RTTI restrictions. Run full local CI
before any implementation push.

## Measured evidence and open interpretation

The earlier [28-source investigation](mos-switch-investigation.md) remains
observational evidence. Its positive-interval, mixed wired-net, force and
startup examples are not automatically approved oracles. The proposed
root/path, force and startup choices above are explicit review decisions.

The new [18-source matrix](../measurements/2026-10-24/mos-strength-matrix-observations.json)
has 768 scalar samples per mode per engine, using isolated outputs and outputs
opposed by weak/pull drivers. All 2,304 value samples per engine agree with
the independent truth/reduction/local-resolution calculation. Both loaded
modes also agree in every strength token. Isolated resistive outputs have
252 strength-token mismatches per engine, including 72 of the 144 definite
enabled samples across all devices. The same mismatch disappears when a
competing driver requires resolution. This is evidence that context matters;
the implementation cause is unconfirmed, and no other simulator code was read.

[Ten supplemental original programs](../measurements/2026-10-24/mos-switch-boundary-observations.json)
record formatting, force/port asymmetry and rejected terminal/net boundaries.
They preserve warnings as observations, not successful feature claims based
only on exit zero. Extract `source` by name from either JSON and run the
recorded compile/runtime commands with a matching version pair.
The dynamic strength-format selection also records both references choosing
bit zero for an X index. Keep the existing invalid-index value semantics;
this observation is not an oracle authorizing that selection.
`shared_uncertain_edge` gives 0 in both worlds at the destination, with range
`[-6,-3]` and token `630`. Treating the opposing roots as independently
optional after their shared uncertain edge would incorrectly permit 1.
Both references match the world-preserving result in this original program.

The 79-item feature inventory and its 20/26 gaps have not been remeasured or
reclassified by this proposal. Neither the new data nor approval of this
contract would establish full P1 or full Icarus coverage. Classes, event NBAs,
specify/timing, binary memory addressing, external interfaces and the other
remaining requirements continue to be part of the goal.
Independent [fork work](fork-local-declarations.md) subsequently expands the
current comparison to 81 probes with 21/27 gaps; the proposal's observations
and semantic choices remain pending.
