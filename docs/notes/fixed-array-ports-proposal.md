# Whole fixed-array ports and element-wise net resolution

[日本語](fixed-array-ports-proposal.ja.md). Status: **pending review**,
2026-10-24. This English text is the proposed contract. Publication is not
approval to implement it.

## Why an amendment is needed

Original programs exercise a remaining Icarus-supported mechanism: whole
fixed-size unpacked input/output ports. The current native front rejects
connected unpacked array ports and resolved arrays of nets. The scalar
collapse and continuous-assignment rules in SIR_SPEC 3.5 do not specify the
array alias views, element-wise drivers or unconnected array pulls needed to
remove those diagnostics. VALUE_SEMANTICS 5.8 explicitly retains an
unconnected-array-port rejection. P1_SCOPE advances the required execution
scope but requires review of missing detailed contracts.

Amend SIR_SPEC 3.1–3.5, 4.2, 5.3, 7.1 and 10, VALUE_SEMANTICS 3.13/5.3/5.5/5.8,
and the corresponding implementation descriptions in SCHEDULER_SPEC 6.1/12.
The existing rules for values, strength, process origin, continuous assignment,
force, sensitivity and initialization remain authoritative. No dependency,
installation, external ABI change or alternate diagnostic mode is selected.

## 1. Types, dimensions and correspondence

Use strict slang's elaborated port direction, kind, type and connections.
An omitted `var` does not make an explicitly typed input a variable; preserve
slang's input-net inference and diagnostics (23.2.2.3). In particular, accepting
an implicitly inferred real/string input does not generally enable explicit
`wire real` or other otherwise illegal net declarations.

Support fixed unpacked dimensions whose leaf types already have an executable
value representation: integral types, including enums and packed aggregates,
real/shortreal, string and chandle. A net requiring strength resolution still
requires a supported integral net element type. Keep automatic event ownership,
class/object roots, unpacked-structure port layout and dynamic-container implicit
sensitivity behind their separate contracts. Unsupported leaf types must remain
positioned diagnostics; opaque handles are not safely untraced values.

Corresponding dimensions have equal extents and match in declaration order,
left end to left end, recursively; actual numeric bounds and ascending versus
descending declarations may differ (6.22.2, 7.6, 23.3.3.5). The last unpacked
dimension varies fastest in storage. Preserve each declaration's own bounds
for source indexing, foreach and type/array queries. Do not match equal numeric
indices or merely equal total flattened lengths across incompatible ranks.

Apply the elaborated type compatibility and conversions, not a new general
per-element numeric-widening policy. Whole unpacked-array assignment requires
equivalent element types under 7.6. The original incompatible width/signedness
and two-state/four-state connections are legality controls, not execution gaps.
The equivalent two-state output case is separate and passes both references.
Preserve ordinary normalization and defaults of each supported leaf type.

## 2. Whole-net connections and alias views

Extend the existing whole-net collapse rule to whole fixed arrays with matching
rank, extents and leaf width/signedness/state domain, for input, output and inout.
The connection joins corresponding physical net elements. Apply the existing
Table 23-1 net-kind merge rule; retain strict slang warnings and uwire checks.
There is one resolved storage location per physical element, including all
parent, child and transitive whole-array aliases. Collapsing is not a copy and
must not reconstruct a stronger driver from a four-state value.

A canonical slot can retain one array layout. Front records an immutable
source view for each alias and translates its declared coordinates into the
canonical coordinates in SIR. Check the source view's bounds and unknown bits
before translation. A wide, unknown or out-of-view index must not wrap into a
valid neighboring element after adding an offset. Use sufficiently wide checked
ordinal calculations; mapping changes neither default reads nor ignored writes.
Reads, writes, selects, refs, force targets, read/write sets and connections
through another instance must use the same view translation.

Names and diagnostic/type-query views retain each source declaration. Runtime
signal lookup remains numeric. This selects no VPI object model or array VCD
extension: WAVEFORM's existing array omission and future VPI rules remain.
A physical write publishes one committed change; alias views do not publish
independent changes or start extra resolution loops.

This proposal does not enable general partial-bit net connections, arbitrary
net concatenations, dynamic network topology or MOS interval transport. Keep
the existing positioned rejection where a noncollapsed net connection needs
strengths the current path cannot carry. The pending
[MOS/port network contract](mos-switch-proposal.md) remains a separate review.
An approval of this array contract does not approve that contract.

## 3. Noncollapsed input/output value connections

Connections to variables and supported expressions follow the existing
implicit-continuous-assignment rule of 4.9.6/23.3.3.2–3. Use the same typed
array-copy semantics as a source aggregate assignment, including declaration
order, independent string/value storage and the existing RHS-before-target
policy. A whole-array expression is evaluated and captured before destination
writes. Do not interleave its later source effects with early destination
members or retain borrowed SSA values beyond their defined lifetime.

The port processes react to supported source read ranges, including a single
changed element or packed bit and all statically read branches of a conditional.
Writing one member must not leave other members stale; copying a whole array
must not introduce a second source evaluation for each destination member.
Preserve the longest-static-prefix rules and normal typed change publication.
Direct integral/real/string input values must update continuously, including
an explicitly declared input variable. The references' explicit-input-var
mismatch is evidence to retain, not an initialization-only execution policy.

Create and own each implicit process in the existing instance/region rules.
Program port processes keep program identity and use the reactive set;
module/interface processes use their current scheduling rules. Do not invent
one user process/RNG per array element. Signal updates and resolution still
use the shared kernel and the existing scheduling regions.

This does not implement ref ports, whose source-binding contract is still
missing. Inout requires eligible nets on both sides; variables remain illegal.
Unmatched partial-array views and instance-array distribution/replication must
keep explicit diagnostics until their own view lowering is implemented and
validated under 23.3.3.5. This is not permission to silently skip a connection.
Their required coverage remains part of P1; compilation failures in these
particular reference examples do not settle their complete support inventory.

## 4. Element-wise drivers and resolution

A resolved net array has driver storage with the same element layout. Each
driver retains its own strength pair and supported side representation.
Resolve one physical element/bit from its applicable drivers using the shared
VALUE_SEMANTICS 5 rules and valsem reference; never resolve the entire unpacked
array as a single packed word or give every element another element's drivers.
Per-element driver coverage and bit ranges distinguish written bits from Z.
Initial covered bits use the approved X/strength policy; uncovered bits drive Z.
Built-in pull/supply behavior remains element-wise under the existing net kind.

The immutable SIR layout and driver ranges identify the affected cells. Whole
port assignments cover their entire array view; constant element/bit writes
cover only those ranges. A runtime-selected target, when enabled, needs its
own current driver coverage: releasing a previously selected element must not
leave an extra driver there. Do not approximate it with a permanent write to
every element. Until that coverage is implemented, retain a positioned
unsupported diagnostic for that form, rather than accepting incorrect values.
Delayed drivers retain the existing value capture, delay selection and inertial
cancellation rules; no new array timer or side-timing policy is authorized here.

A changing driver schedules the existing resolution work for the affected
range. Publish actual resolved value changes for watches/edges; supported
side changes participate as in the current scalar contract. Preserve startup
order, normalized unchanged-write behavior and deterministic wake order.
Delayed/unknown three-state side boundaries and full-strength MOS/source
network boundaries remain unsupported as documented in their separate proposals.

A force of a supported constant array element or packed selection acts on the
same physical net element through every collapsed alias; release recomputes
from that element's real drivers. Keep existing restrictions on variable
part-select force, runtime net selections and unsupported force shapes.
Do not turn an aggregate force into a series of source operations without
its own valid lowering and semantics.

## 5. Unconnected ports and pulls

Ordinary unconnected net elements retain Z, including outputs/inouts; a
variable input retains its typed default under 23.3.3.2. Under 23.2.2.4, evaluate an input default as a constant in its definition
scope and insert it as the implicit input connection when no actual is given.
Copy that array value in the ordinary port process/region, as a supplied
constant connection; do not introduce a different static-initialization phase.
A present default is not an absent connection to be replaced by a pull or zero.

Where `unconnected_drive` applies to an unconnected input net array, create
the equivalent pull driver on every integral element and bit, at the existing
approved pull strength 5 (22.9, VALUE_SEMANTICS 5.8). Use the directive at the
definition and the declared port direction, as in the scalar path. Pull0/pull1
must resolve against internal weak/other drivers; they are not prefilled
variable values. Output/inout and variable inputs are not pulled. Preserve the
existing warning for an unconnected input variable under that directive.

Do not generalize pulls to nonintegral implicit input nets or compound port
expressions whose pull mapping remains unspecified. Keep those boundaries
explicit pending their detailed contract. Array pull creation is ordinary
kernel driver initialization, not a hidden process that keeps a program alive.

## 6. Representation and verification

Front selects compatible geometry, alias correspondence, conversion/copy order,
coverage and dependency ranges. Encode them through typed storage/range metadata
and explicit SIR operations; VM dispatch must not infer array-port meaning from
a name or source syntax. Text form deterministically retains all alias/view and
driver information needed to reproduce execution. Validate view extents and
bounds, leaf types, physical targets, range arithmetic, process origins and
read/write/drive sets. Roundtrips must preserve those properties.

The shared kernel owns physical storage, drivers, resolution, change publication
and force behavior. It imports no slang/SIR/LLVM types; future C++ execution uses
the same C++20 APIs without exceptions/RTTI. Add new public headers, if any, to
the header compilation check. No mutable global registry, runtime environment
read, signal-name lookup or separate VM-only resolver is allowed.

Check element-count, stride, byte-size and per-driver allocation products before
creating storage. Overflow/allocation failures follow existing resource_limit
handling; do not add an undocumented array-size cap or partly replace a live
array. Metadata scales with the represented cells/drivers, and must not reserve
an enormous dense driver-by-array product when coverage is sparse without
accounting for that cost. No new dependency is required by this choice.

## Evidence, acceptance and remaining scope

[Original sources and observations](../measurements/2026-10-24/fixed-array-port-design-observations.json)
record 59 programs at native source `c6d9c3d` (binary banner `392687e`), on
macOS/arm64, against installed 13.0-stable and 14.0-development compiler/runtime
pairs. Seven programs are legality boundaries; strict slang rejects all seven
and accepts the other 52. In each reference, the 52 legal examples have 36
model matches, 15 compilation failures and one output mismatch. Native has
four passing controls and 48 execution rejections. Thirty-four model-matched
examples per reference remain native gaps. These design cases are separate
from the finite feature inventory; the numbers are not coverage percentages.

The mismatch uses explicit input `var`: the proposed continuous-value model
prints 7/9 while both references print X/X. Two illegal writer programs are
accepted by the references and rejected by strict slang; they are legality
observations, not native missing features. Incompatible element conversions
are likewise rejected rather than treated as arbitrary numeric array casts.
Backend assertions are recorded as exits without their implementation text.
No other simulator source/tests were read. Standard examples were not copied.

Acceptance needs original matrices across widths 1/32/65/129 and larger word
boundaries, two/four-state domains, rank/direction/bounds, input/output/inout,
module/generate/interface/program/hierarchy, constant and changing data, supported
scalar leaf types, ordinary and strength resolution, partial driver coverage,
force/release, unconnected/default/pulled inputs and malformed connections.
Add independent kernel/value/SIR checks for view bounds and X/Z indices,
driver isolation, reset and Context isolation, resource failures and typed
change/edge notifications. Do not rely only on a passing port copy example.

Compile controls that corrupt declaration-order mapping, alias bounds,
continuous reevaluation, RHS snapshots, per-element resolution, pull strength
or force sharing. They must fail actual new outputs or validity checks; judging
self-check failures are separate. Keep scalar controls and old SIR dump/suite
expectations unchanged. Any obsolete CLI rejection expectation must be reviewed
in a separate expectation-only branch/PR, with the source PR based on it.
Run full local CI before pushing an implementation.

This proposal adds no implemented support. Classes, VPI, specify/timing,
dynamic-container sensitivity, broader array connection forms and the other
pending contracts remain P1 work. Neither these observations nor an eventual
passing finite inventory establish complete Icarus 13.0 coverage or Linux results.
