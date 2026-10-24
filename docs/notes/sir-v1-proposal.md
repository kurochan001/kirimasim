# P2 proposal: SIR v1, ready for tier 2

[日本語](sir-v1-proposal.ja.md). Status: **direction approved (2026-10-24);
detailed contract pending review**. English is authoritative. Originally
proposed on 2026-10-24; the user subsequently approved the recommendations
recorded below. Detailed EBNF, record schemas, API prototypes and reference
layout remain drafts. The [phase mapping](phase-renumbering.md) is a separate
approved decision. This approval adds no dependency or installation and changes
no existing test expectation.

**Inventory follow-up (2026-10-25):** the tables below retain their original
2026-10-24 snapshot. The approved [runtime array-method implementation](runtime-array-methods.md)
adds `ContainerSwapValues` / `container.swap.values` and `Heap::swapValues`
(SIR_SPEC 5.8, RUNTIME_ABI 6). The current enum has 125 entries. Refresh the
full Op/API inventory before the P2 schema or ABI freeze; formatVersion remains
0 and C ABI remains 0.3. This follow-up does not approve the detailed v1 draft.

## Approved direction (2026-10-24)

The user's approval covers the following recommendations, including the
qualifications given when recommending them:

1. Prepare P2 alongside P1, keep P1 the priority, and freeze SIR v1 only after
   P1 completes. The final grammar freeze moves from the old P1 deadline to P2;
   other P1 scope and exit obligations remain in P1. Keep formatVersion at 0
   until the P2 gates pass.
2. Use a complete canonical record format 1 that preserves dump metadata,
   constant identity and declared counts. Keep the existing v0 diagnostic
   output during migration. The exact grammar/schema and public CLI spelling
   still need review.
3. Generate the Op enum, names, common verification metadata and specification
   table from one schema, with CI consistency checks. Keep complex type rules
   in named validators. Resolve the inventoried semantic gaps separately.
4. Evaluate 16-byte cells with 8-byte alignment as the conservative baseline.
   Measure before freezing the layout; settle reference-token representation
   after P1 activation and lifetime rules are established.
5. Organize the C boundary by typed services such as waits, process management,
   containers and files. Appendix B's 181 A rows are coverage candidates, not
   approval of 181 public functions or of their proposed names/prototypes.
   Preserve tier 1's direct C++ calls; keep ABI 0.x during preparation and
   target ABI 1.0 at P2 exit after coverage and boundary tests pass.
6. Fix analysis side-table format and invalidation rules in P2; missing or
   stale results mean unknown. Select and implement analysis algorithms in P3.

Approved preparation order: **Op schema and consistency checks → v1 parser and
roundtrip checks → shared semantic decisions and service API design → frame and
ABI freeze**. Detailed approval of the items still listed in section 6 is a
later review step. This record changes documentation only; it does not mark P2
started or complete, approve the entire draft verbatim, or approve the pending
P1 semantic proposals.

**Documentation synchronization (2026-10-24):** the approved direction and
post-P1/P2 freeze deadline are now reflected in SIR_SPEC 1/9/11, RUNTIME_ABI
4.2/8 and P1_SCOPE, together with README, ARCHITECTURE and P1_SUPPORT. This
updates the direction only; the detailed grammar, service API and reference
layout in section 6 remain under review. The inventory in section 2 retains its
original snapshot: the old P1 grammar deadline it records has since been amended.

## 1. Purpose and scope

Make tier 2 (C++ AOT generation in **phase P3**) and a possible tier 3
(JIT; adoption decided after P3) executable from the same SIR and kernel as
tier 1. Keep one IR. P2 establishes a complete, verifiable interchange and
runtime contract, rather than building another optimizer or simulator.

Include the text grammar/parser, a complete Op contract, frame/value/reference
layout, the C function-table boundary, shared semantic helpers and the format
of analysis side tables. Do not implement C++ generation, JIT, X analysis,
static-island discovery or new language features in P2 merely to complete
this contract. P1's Icarus-required scope remains P1, including its pending
classes, dynamic data, program and VPI work; the boundary must cover the
**completed** P1, not only today's 124 opcodes. No P1 gate or existing expected
result is relaxed. Tier 1 continues to call the kernel's C++ API directly;
public headers and generated code remain C++20, without exceptions or RTTI.

Basis: [SIR_SPEC 1, 5.1, 7.2, 7.4, 9–11](../spec/SIR_SPEC.md),
[RUNTIME_ABI 2–4, 6, 8](../spec/RUNTIME_ABI.md),
[CLAUDE: decide meaning in one place](../../CLAUDE.md),
[P1_SCOPE](../spec/P1_SCOPE.md). All recommendations here derive from those
repository contracts and the repository inspection below; no external
implementation or external simulator source was consulted.

## 2. Inventory: facts, gaps and verification methods

Snapshot: `d915150` (phase renumbering), whose execution code is unchanged
from `c0388da`. Line numbers refer to that snapshot. Sources are linked to
repository files; a later P1 change must refresh the inventory before freeze.
“not confirmed” always denotes an unverified claim, with a check to perform.

### 2-a. Every Op

Appendix A lists **all 124 enum entries**, each with operands, type rule,
terminator/suspension classification, effects and specification section.
The basis is [design.h](../../src/sir/include/kirimasim/sir/design.h),
[types.cpp](../../src/sir/types.cpp) (`opName`, `isTerminator`, `isWait`), and
[verify.cpp](../../src/sir/verify.cpp) (`checkInstr`, lines 973–2020).
The type column summarizes the implemented checks, not a proof of every
language rule. Common rules: Temp/Const value operands, valid IDs/ranges,
explicit domain conversions, nonzero integral widths and instruction source
locations. Calls can also use slot/frame actuals for array/ref/out parameters.
“may” distinguishes potential suspension from guaranteed blocking: an empty
join or an already true wait condition may proceed immediately. The structural
`isWait` predicate nevertheless marks wait.cond/call.task and a non-join_none
fork as wait boundaries; no SSA temporary can cross them.

Confirmed differences to resolve before SIR v1:

- SIR_SPEC 5.2 calls its table **examples**, not an exhaustive Op schema. It
  lists `inside` and `cond.x`, but the enum/op-name/verifier switches have
  neither. `front/lower_expr.cpp:2302–2335` lowers inside through comparisons,
  logical combination and branches. `cond.x` has no corresponding Op in this
  snapshot. Do not silently remove an approved diagnostic contract: decide
  whether to implement it or approve a documented replacement.
- `rnand`, `rnor` and `rxnor` exist in code; 5.2 only says “and negations”.
  Branch/return opcodes are described in 5.1/7.2/9, not exhaustively in the
  expression table. Container ops and pattern.format have separate tables or
  prose in 5.8/5.9. An exact enum-vs-5.2 string comparison would be wrong today.
- 5.2 says X-producing instructions are flagged and describes comparison masks
  separately from values. `Instr` has no X-production field or mask operand
  kind; constants use a/b words and match.mask receives values plus its mode.
  A semantic descriptor must state whether X production is an Op property or
  a conditional property, rather than treating analysis facts as SIR fields.
- 4.1/4.3 describe class/associative/unpacked-union possibilities that are not
  currently TypeKind/TypeEntryKind implementations. 11 still lists streaming,
  assignment patterns and dynamic-data instructions as future work, although
  lowering and the container/presentation encodings already cover parts.
  Presentation `pattern.format` does not itself implement every source `%p`
  path or all assignment-pattern/streaming semantics.
- Verifier coverage is uneven: Syscall/Builtin mostly validate a name, not a
  complete signature (1877–1909); plain Load's case only compares types when
  operand 0 is a Slot (1280–1283); unqualified ref writes and indexed ref writes
  do not have the same target-fit checks as ordinary stores (1400–1448).
  These are inspection findings, not validated misexecution reproducers.
  Actual acceptance of malformed designs and its effect are **not confirmed**;
  check with original negative verifier tests, without weakening expectations.
- SIR_SPEC 11 and text.h promise grammar/readback in P1, while this proposal
  would freeze them in new P2. This scheduling amendment needs explicit human
  approval. P1 is not deemed complete by moving its other obligations.

### 2-b. Text form as implemented (format 0)

Facts from [text.cpp](../../src/sir/text.cpp), lines 26–403, and
[text.h](../../src/sir/include/kirimasim/sir/text.h): printing only; no SIR
reader or parser entry point was found in tracked src/sir or driver code.
[version.cpp](../../src/sir/version.cpp) returns 0; toText separately hardcodes
`; sir format 0`. Readback is a documented intention, not a delivered roundtrip.

The current writer emits the following grammar skeleton. Tokens/constraints
come from the writer; this is an inventory, not an approved complete grammar.
Whitespace separates tokens; NL ends a declaration/instruction. IDs are decimal.
Q is a byte string in double quotes; escapes are `\"`, `\\`, `\n`, `\t`, and
`\xHH` for control bytes. Non-control bytes are emitted as stored.

```ebnf
text0       = "; sir format 0", NL, "precision", time, NL,
              { source }, { instance }, { type-row }, { pattern-row },
              { slot-row }, { alias }, { subroutine }, { process } ;
source      = "source", uint, Q, NL ;
instance    = "instance", uint, Q, "def=", Q, "parent=", (uint | "-"),
              [ "package" | "unit" | "program" ], NL ;
slot-row    = "slot", slotref, Q, ":", value-type, { dimension }, slot-kind,
              "inst=", uint, [ "sub=", subref ], [ "type=", typeref ],
              [ "driver-of=", slotref, "strength=(", uint, ",", uint, ")" ],
              [ "flags(", flag, { ",", flag }, ")" ], NL ;
alias       = "alias", Q, "->", slotref, NL ;
body        = "{", [ location ], NL, { frame-row }, block, { block }, "}", NL ;
frame-row   = "frame", frameref, Q, ":", value-type,
              [ "<", value-type, ">" ], { dimension }, [ "type=", typeref ], NL ;
block       = blockref, [ "(", tempref, ":", value-type,
              { ",", tempref, ":", value-type }, ")" ], ":", NL, instruction,
              { instruction } ;
instruction = [ tempref, "=" ], opname, [ value-type ],
              [ operand, { ",", operand } ], [ location ], NL ;
operand     = tempref | "const", value-type, literal | [ edge ], slotrange
            | frameref, [ bitrange ] | [ edge ], subref | processref
            | typeref | patternref | scoperef | Q
            | blockref, [ "(", operand, { ",", operand }, ")" ] ;
location    = ";", uint, ":", uint, ":", uint ;
```

The remaining current productions are finite writer cases, not an absent
universal “attribute parser”: type-row has enum item lists, packed/unpacked
fields, array element/dimensions, UDP input/state/output rows, dynarray and
queue capacity; pattern-row has kind/value/element/count/tag, enumerators and
members with escape/bit-offset flags. A subroutine header has name/kind,
automatic/static, optional may-wait, instance/return/frame-of and param rows;
a process header has name/kind/instance/parent, triggers/iff, writes/drives.
`operand`'s wait.event guard uses **`iff @N` without a comma**, and block args
consume the following `count` operands. @N denotes process or subroutine
according to the Op role, not one global ID namespace. See text.cpp 103–125,
202–239, 287–319 and 357–400. These dependencies require schema-aware parsing.

Loss/omission inventory (checked against Design and SIR_SPEC 2/9):

| Field | Printed now | Consequence for a reader |
| --- | --- | --- |
| dumpScopes, dumpVars | No; explicitly omitted by section 2 | Cannot reconstruct $dumpvars row references or full waveform selection from this text alone; verifier checks these rows |
| numScopes | No | Can infer minimum from scope operands, not unused trailing scopes |
| process/subroutine numTemps | No | Can infer max TempId+1, not unused trailing entries |
| slot offset/domain | Offset omitted; domain derived from type | Recompute with a documented target layout, do not serialize native addresses |
| primary names | Alias equal to slot name omitted | Rebuild primary entries; original name-table order/duplicates are not represented |
| constant table IDs/unreferenced constants | Values inlined at use/enum/pattern sites | Reinterning can change IDs/order; unused constants disappear |
| real/shortreal bit patterns | Decimal text via to_chars, not raw bits | Preservation of every NaN payload/sign is not guaranteed by this representation; exact failing inputs **not confirmed**, check all IEEE classes and payloads |
| Operand count/edge/role | Count becomes parentheses, edge contextual; @N overloaded | Recover with Op and table kinds; not a context-free split on commas |
| sources, instance kinds, frames/targets, patterns | Present in kind-applicable forms | Keep their spelling/IDs and byte escaping; source path normalization is a separate policy |
| Inactive fields, unknown flag bits, missing locations | Writer emits only applicable fields/known flags; line=0 suppresses a location | Not a lossless dump of arbitrary unchecked struct contents; v1 must reject or explicitly encode noncanonical fields |
| cont drives | Omitted when equal to writes | Reader must restore drives=writes only for cont, otherwise empty |

Current dump tests: **29 .sv designs**; frozen P1 population: **1,466 entries**.
Neither is presently a reader/roundtrip suite. Whether every population design
lowers to SIR at this snapshot is **not confirmed by this inventory**: the
population is a compatibility gate, not proof that all designs are supported.
Record exact unsupported cases now; P2 exit still requires all after P1.

### 2-c. Kernel C++ API versus function table

Appendix B lists every distinct kernel function/member referenced by vm.cpp,
including function pointers passed to wide arithmetic adapters, the `kw` alias,
Rng aliases and the safe-point hook's scheduler alias. Repeated calls share a
row, with all source lines listed. Data types, enum constants, aggregate construction, implicit trivial copies
and std/SIR/VM methods are not counted as API calls. HeapRoot factories and
small ID/status accessors are included. The existing compiler AST was also
checked for referenced free functions and kernel member calls; this caught
ID/status accessors that a receiver-name search alone misses.
This is a source-call inventory, not a dynamic call-graph claim about kernel
internals. Recheck newly introduced receiver aliases when refreshing it.

[abi.h](../../src/kernel/include/kirimasim/kernel/abi.h) exposes **ABI 0.3**,
18 callable fields: add_process, start, wait_delay, wait_event, wait_monitor,
wait_named, write, schedule_nba, trigger, trigger_nb, triggered, time,
request_end, ending, fail, slot_words, wait_any, wait_mixed.
[abi.cpp](../../src/kernel/abi.cpp) adapts to the C++ implementations.
Existing coverage is not always complete: waitNamed's `flushPoint=false`
join path (vm.cpp:1383) has no parameter in wait_named; heap-aware monitor
metadata also has no equivalent in the current C structs. waitAny's mixed
named-item use maps to **wait_mixed**, not merely wait_any.

Missing groups are the exact RUNTIME_ABI 4.2 categories: waits/scheduling,
writing values, time/ending, system tasks, objects, reports. Context/counters/
stack/host setup are not assigned a fictitious 4.2 group; their exceptions are
explicitly listed as infrastructure under RUNTIME_ABI 3/5/9. Pure header
operations fall under section 3, rather than pretending they are already
function-table entries.

### 2-d. Semantic decisions now inside vm.cpp

These are the nontrivial decisions beyond choosing an Op implementation and
operand types. This inventory identifies mechanisms to share; it does not
assert every branch is a bug. Mechanical dispatch, frame vector reuse and the
small-buffer optimization of parallel block copies need not leave VM.

| vm.cpp lines / mechanism | Extra condition and duplicate-semantics risk | Proposed owner / affected contracts |
| --- | --- | --- |
| 189–206, 391–421, 492–505 | String digit acceptance, signed integer/real conversion, real truth, X/Z branch truth and time spelling depend on values | valsem for pure value rules; kernel for diagnostics/time formatting; VALUE_SEMANTICS 3/7, SYSTEM_TASKS 10 |
| 660–680, 735–785 | Frame refs/arrays, default slot bits, net driver X initialization, per-element event creation and force restoration depend on storage/provenance | shared front/SIR layout descriptor; kernel initialization/holds; SIR 3/7.4, ABI 4.3 |
| 793–876, 1737–1769, 1791–1841 | RNG birth order, program origin, static-init order, time-zero startup and early vs late sensitivity vary by process kind/state | front makes explicit process descriptors; kernel owns startup/sensitivity and program policy; SIR 7.1/7.6, SCHEDULER 6/12 |
| 1265–1425 | Child birth/groups, empty join, retained descendants and no report-flush join wait determine lifetime and wakeup | kernel process/activation services; SCHEDULER 10, ABI 4.2/4.3/6 |
| 1463–1505, 2559–2563 | Postponed-write legality distinguishes local/static subroutine storage from external/heap refs | kernel effect check with explicit provenance; front records target scope; SCHEDULER 2.2, SIR 5/10 |
| 1531–1544, 2630–2641, 4098–4105 | Unknown delay becomes zero; high words/time overflow cause run error | valsem delay conversion; kernel time-bound check; SIR 1/7.2, SCHEDULER 8 |
| 1693–1731, 3485–3489, 4025–4072 | Declared direction, unknown/out-of-range indices, clipping and per-source warning counts | front preserves dimensions/window; valsem index/clip; kernel diagnostics; VALUE_SEMANTICS 3.11, CLI 2.2 |
| 1903–2083, 3258–3324 | Initialization, all-argument capture, ref binding, static vs automatic copy-out and disable-before-output differ by lifetime/state | front explicit call binding; kernel activation/ref lifetime, shared copy primitives; SIR 7.4/8, ABI 4.3 |
| 2085–2240, 2309–2359 | Slot/frame/heap tagged references, stale identities, root tracing of calls/reports/pending NBA values and nested receiver access | kernel opaque references/root visitors, front descriptors; SIR 5.8/8, ABI 6 |
| 2243–2306, 2362–2526 | Heap statuses become language diagnostics; allocation/notification/queue receivers depend on actual value and structure | kernel container facade and shared error policy; valsem cell rules; SIR 4.2.1/5.8, ABI 6 |
| 2537–2578, 2598–2600, 2760–2838 | Step limits, branch checkpoints, cancellation after callbacks, time overflow, end notes | kernel checkpoint/status/error service; SCHEDULER 16, ABI 4.4 |
| 2644–2709, 2886–3117 | Watches vs guards/named events, null handles, read dependency discovery, called-local filtering and live heap dependencies | front/SIR computes semantic read/effect descriptors; kernel binds refs and owns subscriptions; SIR 7.2/8, SCHEDULER 9.5 |
| 2747–2827, 2850–2884, 3235–3244 | File vs global monitor registration and held-bit restoration depend on descriptors and hold state | kernel monitor/override services; SIR 5.6/7.1, SYSTEM_TASKS 4/6.2 |
| 3120–3233 | Scope/task disable unwinds activations and descendants using call depths/birth numbers, suppresses copy-out | kernel activation/unwind service; front scope IDs/exit blocks; SCHEDULER 10.4/10.7 |
| 3246–3257, 3371–3439 | Default values and net strength-side resolution differ by invalid state, driver masks and width | valsem owns reference value rules; kernel optimized helpers; SIR 3.5, VALUE_SEMANTICS 5 |
| 3492–3856, 3952–4023 | Wide operations have local unknown/select/reduction/power/shift composition; shifts and logical implication depend on values | valsem reference rules, kernel width-generic helpers; VALUE_SEMANTICS 3/7; keep only type/width dispatch in VM |
| 4106–4259 | String replication warning, unknown conditional alternatives, invalid string index, shortreal rounding and mixed real truth are locally implemented | valsem rules plus kernel string/real operations and diagnostic state; VALUE_SEMANTICS 3.9/7.3–7.6 |
| 4270–4559 | Ref kind/invalid element, NBA captures, deferred assertion arguments and named-event null handling | kernel refs/NBA/reports; front supplies explicit captures; SIR 5.3/5.7/8 |
| 4655–4740, 5293–5383 | Unknown scan formats, readmem address direction/default bounds, scan writeback and partial fread values | kernel scan/memory-file facade (including current direct stdio on memory output); valsem conversion primitives; SYSTEM_TASKS 6/7 |
| 4748–4861 | Null triggered, string method bounds, case folding, numeric-string rules and substring range determine results | valsem reference string semantics; kernel string-method facade; VALUE_SEMANTICS 7.6, SYSTEM_TASKS |
| 4864–5040, 5062–5133 | Pattern layout traversal/union views, runtime container length and format consumption build value views | front selects presentation layouts; kernel captures/traverses/prints values; SIR 5.9, SYSTEM_TASKS 3.9/3.13 |
| 5135–5181 | Warning deduplication, owning procedure RNG vs initializer RNG, explicit seed reset | kernel diagnostic/RNG Context service; SYSTEM_TASKS 12/13, UNSPECIFIED K1–K3 |
| 5188–5290, 5386–5946 | Call-stack diagnostic scope, dump kind mapping, descriptor validation, timeformat, fatal/exit, random distributions, plusargs and syscall result conversions | front resolves semantic task/method IDs and metadata; kernel task facade; valsem value conversions; SYSTEM_TASKS, WAVEFORM, ABI 4.2/4.4 |

No loop should be moved into `front` when it depends on runtime state. No
scheduler policy belongs in valsem. Both tiers call the same kernel operation;
its optimized value algorithm is checked against valsem. Pure type/width
specialization remains mechanical tier dispatch. The ref/root/cancellation
services must preserve the already approved dynamic-container contract.

### 2-e. Outstanding specification items, by decision deadline

Every bullet of SIR_SPEC 11 and RUNTIME_ABI 8 is accounted for below. A
“P4+” classification never defers an Icarus-required P1 mechanism.

| Source / unsettled item | Before P3 (C++ generation) | P4 or later may remain |
| --- | --- | --- |
| SIR 11 streaming and assignment patterns | Document all completed P1 encodings, including presentation vs construction; extend schema for later P1 Ops | Additional non-P1 mechanisms only |
| SIR 11 unpacked-union layout | Fix it if accepted by completed P1; otherwise reject explicitly and record unsupported scope | Broader union layout only if outside required P1 |
| SIR 11 frame size/alignment | Required: canonical descriptor and shared layout | No deferral of existing types |
| SIR 11 class/dynamic/random/assertion/coverage instructions | All completed P1 class/dynamic/VPI-required representations; remove stale blanket later-phase claim | Constraints/assertions/coverage beyond P1 in P4 |
| SIR 11 hierarchical resolution | Complete front rule/error contract; execution must use IDs only | New outside-P1 resolution mechanisms |
| SIR 11 text grammar | P2 grammar/parser/roundtrip; amend “by end P1” explicitly | None for SIR v1 |
| SIR 11 slang correspondence table | Complete accepted P1 construct→SIR mapping and explicit rejection coverage | Later language additions |
| ABI 8 table fields/C layout | Required for all P1 runtime effects and model loading | Entries only for genuinely later features |
| ABI 8 status/error IDs | Freeze status meanings, structured errors and source association | New additive diagnostics with stable meanings |
| ABI 8 temporaries/shared frames | Required: no SSA across waits; explicit captured frame variables, local scratch | Tier migration can wait for P3 decision |
| ABI 8 Windows boundary | Decide exact C widths/calling convention/loader ownership, run minimal MSVC C++20 fixture before enabling Windows AOT | Full Windows AOT toolchain/performance validation in P3; ability to load is **not confirmed** until tested |
| ABI 8 class/mailbox/weak GC, fibers/DPI tasks | P1-required class roots/lifetimes and parent-frame retention; hooks extensible | Broader weak references/mailboxes/fibers/DPI task extension in P4 |
| ABI 8 parent frames retained by children | Decide activation ownership/layout/root interface now, even while P1's capture proposal awaits approval | No postponement if P1 needs captures |
| ABI 8 finish in DPI export / argument lifetime | Encode generic end/status and borrowed-buffer lifetime rules now | DPI-specific exports, svLogicVecVal/open arrays in P4 unless required by P1 |
| ABI 8 parallel static islands | Record side-effect summaries and serial reference semantics | Scheduling/thread partition algorithms and parallel execution in P5 |

## 3. Design details (approved direction; remaining drafts identified above)

### D1. Text grammar, parser and version

**Recommend a complete canonical record grammar for format 1**, retaining the
existing human-facing v0 dump as a separately selected diagnostic format during
migration. Format 1 serializes every semantic Design field, including dump
metadata and declared counts. Do not claim roundtrip by silently dropping
waveforms, renumbering IDs, or rebuilding a different design from source.
Use an in-repository hand-written byte lexer and schema-directed parser, with
no parser-generator dependency. Pass the parsed Design through the existing
verifier before publishing it. Syntax/verification failure returns diagnostics
and no executable Design; allocation/size limits are checked before resizing.

The proposed concrete syntax uses fixed record tags/field keys. This EBNF
fully defines the outer/lexical grammar; the mandatory row-field schemas below
and D2 define which records/types/roles are legal, rather than accepting arbitrary
unknown attributes. WS is ASCII space/tab/CR/LF between tokens, never inside a
number/identifier. A `//` comment lasts to LF; it has no semantic source location.
Source positions are explicit fields. EOF is required after the design record.
Backslash escapes in quoted EBNF terminals use C notation; letter ranges denote
ASCII code-point intervals. Enum atoms use schema names such as StaticInit;
legacy hyphenated display spellings are not implicit v1 enum tokens.

```ebnf
file       = "sir", "1", record, EOF ;
record     = ident, "{", { field }, "}" ;
field      = ident, "=", value, ";" ;
value      = sint | bytes | ident | list | record ;
list       = "[", [ value, { ",", value } ], "]" ;
sint       = [ "-" ], digit, { digit } ;
ident      = letter, { letter | digit | "_" | "." } ;
bytes      = '"', { printable | escape }, '"' ;
escape     = '\\', ( '"' | '\\' | "n" | "t" | "x", hex, hex ) ;
digit      = "0" | "1" | "2" | "3" | "4" | "5" | "6" | "7" | "8" | "9" ;
letter     = "A" … "Z" | "a" … "z" ;
hex        = digit | "a" … "f" | "A" … "F" ;
```

`printable` means one ASCII byte 0x20–0x7e except quote/backslash. Every other
byte prints as exactly two lowercase hex digits after `\x`; two digits are
consumed, even before another hex digit. Empty byte strings and embedded NUL
are allowed. No locale-sensitive numbers, exponent forms or UTF-8 validation.
Canonical integers use zero or a nonzero leading digit, no plus, no negative
zero. Parser may accept noncanonical whitespace/comments, printer never adds
them. bool is `true`/`false`, no-ID is `none`; uint fields reject negatives and
out-of-range values. Unsigned word fields allow 0..2^64−1 (not signed int64).
Real constants use fixed-width **raw IEEE bits**, carried as uint words, not
floating decimal text. Endianness of the file is irrelevant; word vectors are
least significant first and spare bits must be zero.

Required record schemas (field order below is print order; no duplicate or
unknown key; vector position is the row ID). All fields print, including empty
lists/defaults. A type record is kind,width,detail; a location is file,line,column;
a dimension is left,right; owner is kind,id; a range is slot,firstElem,numElems,
lsb,width. Non-applicable IDs use none, not invented sentinels.

| Record | Fields, in order |
| --- | --- |
| design | precision, sources, instances, types, patterns, slots, names, constants, subroutines, processes, numScopes, dumpScopes, dumpVars |
| instance | scopeName, definition, parent, kind |
| typeEntry | kind, name, value, items, fields, element, dims, inputs, sequential, rows, initial, maxElements |
| enumItem / field / udpRow | name,value / name,lsb,width,type / inputs,state,output |
| patternLayout | kind,value,enumerators,members,element,count,tag,tagEscaped |
| patternEnumerator / patternMember | name,value,escaped / name,layout,escaped,lsb |
| slot | name,type,kind,dims,flags,instance,subroutine,declType,driverOf,strength1,strength0 |
| nameEntry / constant | path,slot / type,aWords,bWords,bytes |
| subroutine | name,kind,automatic,mayWait,instance,params,returnType,frame,blocks,numTemps,frameOf,loc |
| param / frameVar | name,dir,type,slot,frameVar / name,type,declType,dims,target |
| process | name,kind,instance,triggers,writes,drives,frame,blocks,numTemps,parent,loc |
| trigger / block / blockParam | range,edge,guard / params,instrs / temp,type |
| instr | op,type,result,operands,loc |
| operand | kind,id,lsb,width,firstElem,numElems,count,edge,text |
| dumpScope / dumpVar | name,path,kind,parent / scope,name,range,type,width,slot |

Lists specify their element record or scalar type by schema (sources=bytes;
word lists=uint64; IDs=uint32 or none; enums use their canonical symbolic names).
The leading record must be `design`. Nested record tags are exactly those in
the table plus `type`, `loc`, `dimension`, `owner`, `range`. No arbitrary
extension record is executable. Slot offset is derived target layout, **not**
part of serialization. IDs stay vector indices, including unused rows. IDs and
operand kinds are explicit, so process/subroutine @N ambiguity disappears.
Process drives and dump metadata print in full. New P1 fields require a schema
update before v1 freezes, not silent data loss. This grammar is an interchange
encoding of the existing Design, not a new IR or a replacement set of Ops.

Guarantees: `print1(parse1(print1(D))) == print1(D)` byte-for-byte, and semantic
Design equality excluding derived offsets/caches; `parse1(print1(D))` passes
verifier and preserves simulation/VCD observations. Also `print1(parse1(T))`
canonicalizes an accepted noncanonical T, and then stays fixed. No general
`parse0` is promised. Existing dump goldens remain v0 until a separately reviewed
expectation-only change is approved. The new roundtrip job takes each design
through its front-generated in-memory SIR and format 1, without editing a
single v0 .expected file. Public CLI spelling of the optional v1 dump/load mode
needs a CLI.md amendment; no hidden runtime knob.

Set `formatVersion()` to 1 only after grammar/schema approval, a complete
reader/writer, all mandatory roundtrips, stable errors and Op consistency gates.
One version source supplies the writer header and loader checks. Subsequently,
any Op, field, type, operand-role or semantic-contract change that can alter
interpretation increments the integer format version; reject unknown versions.
Formatting-only changes that alter canonical bytes also bump it. Do not reuse
Op ordinal values as wire tags. Explicit v1→v2 migration, if needed, is a
separate tool/decision, not silent acceptance. Keep format version distinct
from product version, ABI major/minor and side-table schema version.

**Alternative:** preserve the line-oriented v0 syntax and write a complete
field-by-field extension grammar. It minimizes familiar syntax changes, but
needs explicit constant/dump/count sections and role disambiguation anyway.
Another alternative is standard JSON with a human dump alongside it; dependency
and byte-string/uint64 policy would still need approval. No JSON/MLIR/FileCheck
library or tool is introduced by this proposal. **Affected:** SIR 1/2/4/5/9/10/11,
CLI 2, TEST_SUITE dump tests and expected-value protection.

### D2. One Op definition, verified consumers

Recommend a checked-in declarative Op schema in `sir`: symbolic name, operand
roles/cardinality, type-relation key, result policy, terminator and suspension
rule (including join kind), effects, potential-X rule, and specification section.
Use the already required Python toolchain to generate the enum, spelling map,
parser structural checks, common verifier checks and the exhaustive normative
specification table. Irregular type rules remain named C++ validators; schema
lists each validator explicitly and CI verifies its presence. Syscall/Builtin
need a similarly explicit semantic ID/signature table; resolve names in front
or common SIR load preparation, never independently in each tier.

CI regenerates into a temporary directory, compares checked-in products and
verifies **set equality** across schema/enum/spec; it fails for unknown, missing,
duplicate and unhandled Ops, stale spelling and unimplemented type-rule keys.
Schema generation does not prove the semantics correct: keep mechanism matrices,
negative verifier tests and valsem differential checks. The `inside` lowering
and `cond.x` question above require a human decision before describing the table
as normative. Prefer no external schema generator.

**Alternative:** keep handwritten switches and use an extraction/consistency
checker. It is smaller initially, but checking names alone cannot guard effects,
waitness and operand roles; require all metadata even for that alternative.
**Affected:** SIR 5/7/8/10, CLAUDE's one-meaning rule, TEST_SUITE.

### D3. Shared frame and reference layout

Recommend a target-qualified **layout descriptor computed once from SIR**, used
by both tiers and validated against generated C++20 static assertions. No host
`sizeof(std::vector)` or native class layout crosses the boundary. Frame control
records are kernel-owned/opaque; payload offsets are integer byte offsets with
overflow-checked size/alignment and explicit activation/root maps.

Initial conservative payload rule: each value uses `ceil(width/64)` cells for
integrals (minimum one); one cell for real, shortreal, string/event/chandle/
container tokens; each cell is two uint64 words (16 bytes), aligned to 8 bytes.
Unused b words and high bits are zero where the type requires it; 4-state words
keep their established a/b encoding. Fixed arrays are row-major in declaration
order, rightmost index fastest. Place FrameVarId rows in order using
`offset=alignUp(previousEnd,8)`; size is `alignUp(end,8)`. Unpacked aggregate
leaves follow their finalized SIR descriptor, not an ad hoc C++ struct.

A reference is a kernel-defined opaque token, not the VM's current bit tagging.
A still-unapproved candidate is a fixed 16-byte token (two uint64 fields)
backed by a Context table
containing kind, activation/cell identity, generation, element/leaf/window and
permissions. It fits a cell; stale reference validation and GC roots are kernel
operations. If the completed P1 cannot safely fit the token identity/generation
in that representation, approve a different token size before freezing v1; do
not truncate a generation. Handles never serialize runtime pointers. Frame
root maps enumerate reference/container/class roots and element descriptors.

SSA temps and block parameters are local scratch, never live across a wait
(SIR 5.1/10). Values needed after waiting are explicit FrameVars already captured
by front; NBAs/deferred calls retain their own captured values. Resume state,
call-chain links, named-block activations and retained parents have explicit
kernel lifetime, separate from scratch. A future mid-wait tier switch needs
compatible descriptor/version checks, but is not a P2 execution requirement.

**Alternative:** use compact domain-specific fields or copy the current VM's
“scalars first, arrays later” W4-vector layout (vm.cpp:660–680). Either is possible,
but the former needs target-specific packing and more proof; the latter makes a
VM storage choice an enduring interface. Benchmark the conservative layout
before approval/freeze, with PERFORMANCE criteria. **Affected:** SIR 3/4/5.1/7.4/8/11,
ABI 3/4.3/6/8, automatic-capture and class-runtime proposals (still unapproved).

### D4. Complete C boundary; preserve tier 1's direct C++ path

Appendix B is the operation-coverage inventory: each **A** row records a
candidate C entry and its required behavior. The approved direction is to group
these operations into typed services, retaining a mapping from every inventory
row to its service. The 181 candidate functions, their names and prototypes are
not approved individually. Each finalized service reaches the same kernel
implementation as the C++ API. No std types, exceptions,
RTTI, VM Frame or SIR Design object crosses the C boundary. Use fixed-width
integers, pointer/count byte or word spans, opaque Context-owned IDs, and plain
C structs. Result buffers belong to the caller; borrowed views expire at the
specified call/checkpoint, never implicitly after some later allocation.

The required coverage is grouped as follows; final service boundaries,
entry names, prototypes and H/I exceptions require review before implementation:

- Scheduler/process/activation: child/program registration, startup/final/end
  reporting, delayed-driver operations, cancellation/kill/release, fork/join,
  scope/task activation/unwind, explicit no-flush join waits, full heap-aware
  monitor descriptors, override holds/releases and file/global monitors.
- Time/end/diagnostics: shared checkpoints, overflow/resource/runtime errors,
  structured error ID/source/scope, end reason/note and stack trace capture.
- Reports: deferred report/call enqueue, retained callback arguments, drop/run
  lifetime and cancellation. The existing C callback typedef is not an enqueue
  function and does not fill this gap by itself.
- System tasks: Files/Output/Dump/Strings functions and formatting, scanning,
  memory-file and random functions below; typed task/method calls resolved from
  the common signature table, with shared argument binding/writeback.
- Objects: typed layouts, allocation, stable cell access, queue mutation/copy/
  snapshot, root visitors/safepoints, monitor dependencies and borrowed reads.
- Value helpers that are out-of-line: C span-based wrappers for wide arithmetic,
  real/string conversions, strength/UDP and non-inline formatting helpers.

The new shared operations needed to move section 2-d policy (not yet a named
C++ API) are additionally proposed as `activation_enter`, `activation_leave`,
`activation_disable`, `activation_retain`, `activation_release`, `ref_make`,
`ref_read`, `ref_write`, `ref_schedule_nba`, `ref_validate`, `fork_begin`,
`join_wait`, `process_checkpoint`, `diagnostic_emit`, `task_invoke`,
`method_invoke`, `frame_layout_get`, `model_initialize`, `memory_file_read`
and `memory_file_write`. Request/result
schemas carry typed IDs, explicit permissions/capture policy and effect data;
these are kernel services, not a bytecode interpreter behind the C table.
Front supplies semantic descriptors; neither tier rediscovers language policy.

Keep existing 0.3 entry offsets/struct layouts. Compatible new functions append
at the table tail and raise the **minor**. An extended monitor uses a new struct
and entry, leaving old wait entries intact; never append fields to a struct
passed by value under the same ABI. Add a capability/size query as a new tail
entry and require exact major / sufficient minor and capabilities before
accessing later fields. A changed existing prototype, value/reference layout,
calling convention or semantics needs a **major** change. Recommend stabilize
**ABI 1.0** at P2 exit after the layout decision; the exact preceding 0.x minor
is **not confirmed** (depends on intervening P1 changes), so do not claim “0.4”
now. SIR v1 does not numerically imply ABI 1.0; these are separate decisions.

Host-only **H** rows remain in the C++ loader/driver in P3; generated resumes
cannot call them. Inline pure-value **I** exceptions use the same approved public
C++20 helpers under ABI 3. Every exception states why the function table is
unnecessary; re-audit in P3 and before any JIT decision. No executed P1 language
operation can be waved away as “P4” to avoid completing the interface.

**Alternative:** postpone ABI 1.0 and keep a documented 0.x draft through P3.
This allows churn but invalidates compiled models more often; it still requires
complete coverage and explicit compatibility checks. Another alternative is a
single generic runtime request dispatcher; it reduces fields but risks hiding
an interpreter/new instruction contract, so prefer typed service entries.
**Affected:** ABI 2–6/8/9, SIR 5/7/8, SYSTEM_TASKS/WAVEFORM, CODEGEN_SPEC.

### D5. Move decisions, not just switches

Approve the owner assignments in section 2-d mechanism by mechanism. Front
makes explicit semantic IDs, captures, origin, source/scope, bounds and read/
write provenance. Kernel owns runtime activation, effects, subscriptions,
stateful services and errors. Valsem defines value/index/conversion/string rules;
optimized kernel helpers are cross-checked against it. SIR's pure layout/
descriptor utilities may be shared without adding kernel→sir dependency;
front/loader translate descriptors into plain kernel data.

**Alternative:** share a helper library containing today's VM policy unchanged.
That would reduce duplication but preserve misplaced language decisions and
can violate the library dependency graph. Do not make codegen depend on VM or
kernel depend on slang/SIR. No rule is moved just to make this proposal shorter.
**Affected:** CLAUDE one-meaning rule and every spec section named in 2-d.

### D6. Separate analysis tables

Recommend immutable side tables keyed by SIR content hash, formatVersion,
analysis-schema version, target/layout ID and pass/options hash. Dense records
use SlotId or body-kind/body-ID/BlockId/TempId/operand index; IDs are scoped, not
text names. Record result domain (proven-no-X / unknown), static-island membership,
level order and observability/force-preserve reason separately, with evidence
kind and producer version. Missing/stale entries mean **unknown**, never a
license to optimize. Validate ID bounds/hash/type widths and dependency ordering
before consumption. No analysis flags become semantic SIR facts.

P2 fixes schema, invalidation and validation interfaces with empty/unknown
fixtures. P3 selects algorithms and fills tables. **Alternative:** sparse maps
for very large designs; serialize them in sorted key order with the same
identity/unknown rules. Mutating SIR with X-analysis facts is rejected by SIR 1.
**Affected:** SIR 1/5.2, CODEGEN_SPEC static islands, ARCHITECTURE analysis.

### D7. Sequence relative to P1

Option A: start all P2 work only after P1 exits. Advantage: one stable feature
inventory and no repeated ABI/Op edits. Cost: parser/semantic-boundary findings
arrive late and can delay P3 substantially.

Option B (**recommended**): inventory, schema checking, parser prototypes and
shared-service planning may proceed alongside P1; P1 retains priority and
formatVersion stays 0. Reinventory on every new P1 Op/type/kernel operation;
freeze SIR v1/ABI only after P1 completion, human approval of outstanding
semantic amendments and all exit gates. Advantage: gaps are discovered early;
cost: recurring schema churn and extra coordination. Record the commit/hash of
every baseline and never call a partial current inventory “P2 complete”.

Option B was selected by the user on 2026-10-24. The approval covers this
sequence and the P2 freeze deadline; the detailed contracts still identified
as pending require review before their implementation.
**Affected:** SIR 1/9/11 (current P1 grammar deadline), ABI 8, P1_SCOPE, README.

## 4. Milestones and exit criteria

1. **M0 — complete detailed contract review:** the direction and parallel
   preparation/post-P1 freeze sequence are approved on 2026-10-24. Review the
   remaining grammar/schema, API, reference-layout and semantic decisions in
   section 6; reconcile spec-only Ops and pending P1 proposals. Incorporate the
   approved direction into the affected specifications before implementation;
   preserve the distinction between approved direction and pending details.
   M0 is not complete merely because the direction is approved.
2. **M1 — executable Op/schema contract:** complete every P1 Op/type/task/method
   signature and effect rule; CI checks enum/generated metadata/spec set equality
   and stale generation. Negative controls must show missing/extra Ops, wrong
   waitness/type/signature and stale tables are rejected.
3. **M2 — reader and canonical text:** SIR_SPEC 9 grammar fully fixed; parser
   immediately passes the verifier. For **every design in tests/dump_sir and
   tests/gates/p1_population.txt**, print1→parse1→print1 is byte identical and
   parsed Design verifies. Enumerate designs and report attempted/passed/
   unsupported/failed separately: exit requires **zero** unsupported/failed,
   not exclusion of a case. Include new P1 designs added before freeze. Test
   strings/IDs/constants/NaN bits/real signed zero/wide XZ, block args, scopes,
   patterns, containers, dumpvars metadata and invalid/oversized input. When all
   gates pass, set formatVersion to 1; existing v0 goldens stay protected.
4. **M3 — layout and ABI completeness:** frame/reference sizes, alignment,
   root/lifetime and error contracts are specified; C++20 fixture validates offsets
   and C callbacks, rejects incompatible versions, and suspends/resumes a tiny
   model against the real kernel. This is an interface fixture, not the P3 AOT
   generator. Every runtime operation used by VM has a C-table counterpart or
   a reviewed H/I deferral with reason and next-review phase; refresh Appendix B
   at final P1. Add/retain tests that cross-check C and C++ paths.
5. **M4 — shared semantics and no regression:** VM has no independent language
   decisions listed in 2-d; each has one approved owner and entry, mechanism
   matrices and valsem checks. P1 population, sv-tests, CTest and PerfCount
   results remain unchanged (new cases can add coverage; no existing expectation
   is rewritten in an implementation commit). Run full local_ci and external
   gates with pinned input revisions, options and tool versions; report any
   unsupported item without calling it a pass. Today's external gate results
   are **not remeasured by this documentation task**.
6. **M5 — release SIR v1 for P3:** P1 has exited, all above evidence is reviewed,
   outstanding P2 blockers are zero, version/capability checks are active, and
   EN/JA SIR/ABI specifications agree. Hand P3 one Design + layout descriptor +
   separate optional analysis tables + kernel API; not a second IR.

Performance: follow [PERFORMANCE 4/5](../spec/PERFORMANCE.md), not a new threshold.
Retain PerfCount expectations and its current 10% regression check; changed
expected counts need a separate approved change. For wall time/RSS use median
of five runs and last-seven-night baseline; existing 15% time/RSS and 25% build
warnings remain warnings. Record build/run/parse costs separately and target/
compiler/options without machine identifiers. A new parser is not justification
for hiding startup costs; compact-frame alternatives require measured evidence.

## 5. Risks and not confirmed

- P1 is unfinished; opcode count, object/ABI requirements and layouts will grow.
  Mitigation: refresh inventories at P1 exit, keep 0 until final contract gates.
- Omitted dump metadata and inlined constants make “parse the present dump”
  insufficient. A textual fixed point alone does not prove semantic equality;
  verify metadata and execution/VCD as well.
- Current VM ref tags cannot be made public by accident. Activation capture and
  class runtime proposals are still pending; their final semantic/layout choices
  are **not confirmed**. Check human approval and completed P1 implementations
  before sizing tokens or listing them as supported.
- Moving diagnostics/GC/cancellation can change event order, report lifetime,
  seeds, out/ref copy-out or PerfCount. Preserve scheduling matrices and inspect
  any disagreement before changing expectations.
- Real/shortreal bit preservation and Windows ABI execution are **not confirmed**
  until the stated bit-pattern/MSVC fixtures run; this task claims no such test.
- The inventory was extracted lexically, inspected and cross-checked against
  the existing compiler AST, but is not yet a continuing CI gate. Coverage of
  future aliases/macros is **not confirmed**; implement an in-repository checker
  or repeat the AST review using the existing toolchain before freeze. Do not install a new analyzer without approval.
- This task ran no new simulation benchmarks, sv-tests or external population
  pass-rate measurement; do not interpret the proposal as parity evidence.
- New tools/dependencies (including MLIR, LLVM FileCheck, a parser framework or
  a serialization library) require a separate human decision and installation
  approval. None is required by the recommended path or installed here.

## 6. Points to decide (detailed review remains)

1. Finalize the exact format-1 EBNF and field schemas, byte/number rules,
   versioning details and public v0/v1 dump/load CLI spelling. The complete
   record-format direction and post-P1 freeze have been selected.
2. Finalize the Op/task/method schema and generated-consumer interfaces;
   resolve inside/cond.x and incomplete verifier contracts without silently
   weakening approved semantics or existing expectations.
3. Review measured frame-layout results, reference-token size/representation,
   activation/root lifetime and target-specific checks after the relevant P1
   contracts are established. The 16-byte cell / 8-byte alignment choice is an
   evaluation baseline, not a frozen ABI.
4. Review service boundaries, API names/prototypes/count, error and ownership
   rules, and every host/header exception against all inventory rows. The 181
   A rows and additional candidate service names are not an approved function
   list. Define Windows boundary evidence before enabling Windows AOT.
5. Review the exact analysis side-table schema, hashes/IDs and invalidation
   validation. The separate-table and unknown-on-missing/stale principles are
   selected; analysis algorithms remain P3.
6. Resolve inherited Priority P3 headings for assertion/randomize/library-export
   versus their P4/P5 delivery plans, and obtain the separate approvals for
   any still-pending P1 semantic amendments. This approval does not settle them.

## Original proposal verification record

The phase-change full local CI and explicit expected-change result are recorded
in [phase-renumbering](phase-renumbering.md#validation-2026-10-24). For this
proposal, checks compare all 124 Op names with the actual enum, all 274 API
rows/line lists and 181 proposed entry names between EN/JA, both EBNF blocks,
local links, protected skip membership, and private-path exclusion. The existing
compiler parsed vm.cpp with `-fsyntax-only -Xclang -ast-dump=json` using its
build/dev compile command (no new compiler installed); member/free-function
references were cross-checked, including the six ID/status accessor rows.
This verifies the inventory's construction, not the proposed parser or ABI.
Only this English/Japanese note pair is changed in the proposal commit.

## Appendix A. Complete Op inventory

Columns describe the snapshot, not the proposed generated schema. “result only”
allows allocation of kernel-interned strings or immutable values where needed;
read/create/write effects are listed explicitly for stateful instructions.
Verifier line numbers are entry points into checks, not external sources.

| Op / text name | Operands | Current type rule | Terminator | Suspension | Writes / effects | SIR_SPEC section; verifier line |
| --- | --- | --- | --- | --- | --- | --- |
| `Load` / `load` | slot range | result = range value type | no | no | read slot | 5.2; 1280 |
| `Store` / `store` | slot range, value | value fits target | no | no | slot now / queued NBA | 5.2; 1284 |
| `Nba` / `nba` | slot range, value | value fits target | no | no | slot now / queued NBA | 5.2; 1285 |
| `NbaDelay` / `nba.delay` | slot range, value, ticks | value fits target; ticks integral | no | no | queued slot NBA | 5.2; 1293 |
| `DriveDelay` / `drive.delay` | slot range, value, rise, fall, off | cont only; integral target/value; integral ticks | no | no | delayed inertial driver update | 5.4; 1303 |
| `LoadElem` / `load.elem` | slot/frame array, index... | one integral index per dimension; result = element | no | no | read element / default | 5.3; 1314 |
| `StoreElem` / `store.elem` | slot/frame array, index..., value | element-compatible value; integral indices; NBA forbids frame | no | no | element now / queued NBA | 5.3; 1315 |
| `NbaElem` / `nba.elem` | slot/frame array, index..., value | element-compatible value; integral indices; NBA forbids frame | no | no | element now / queued NBA | 5.3; 1316 |
| `NbaElemDelay` / `nba.elem.delay` | slot array, index..., value, ticks | integral indices/ticks; value fits element | no | no | captured element NBA | 5.3; 7.3; 1317 |
| `StoreBits` / `store.bits` | slot/frame, index..., position, value | integral indices/position; same domain; width <= parent; NBA forbids frame | no | no | clipped bits now / NBA | 5.3; 1342 |
| `NbaBits` / `nba.bits` | slot/frame, index..., position, value | integral indices/position; same domain; width <= parent; NBA forbids frame | no | no | clipped bits now / NBA | 5.3; 1343 |
| `NbaBitsDelay` / `nba.bits.delay` | slot, index..., position, value, ticks | as nba.bits; integral ticks | no | no | captured clipped bits NBA | 5.3; 7.3; 1344 |
| `Fload` / `fload` | frame | same result type; scalar or ref | no | no | read frame | 7.4; 1364 |
| `Fstore` / `fstore` | frame, value | value fits scalar frame or ref | no | no | frame value | 7.4; 1370 |
| `RefMake` / `ref.make` | slot/frame, [index...] | ref result; zero or dimension-count integral indices; qualified container target | no | no | reference value | 8; 1376 |
| `LoadRef` / `load.ref` | ref, [constant leaf] | nonvoid/nonref result; qualified ref exact target; leaf forbidden for qualified container | no | no | read referent | 8; 1400 |
| `StoreRef` / `store.ref` | ref, [constant leaf], value | value operand; qualified ref exact target; unqualified target dynamically represented | no | no | referent now / queued NBA | 8; 1401 |
| `NbaRef` / `nba.ref` | ref, [constant leaf], value | value operand; qualified ref exact target; unqualified target dynamically represented | no | no | referent now / queued NBA | 8; 1402 |
| `LoadRefElem` / `load.ref.elem` | ref frame argument, index... | dimension-count integral indices; result = ref target | no | no | read referent element | 5.3; 8; 1418 |
| `StoreRefElem` / `store.ref.elem` | ref frame argument, index..., value | dimension-count integral indices; value operand (target-fit check is incomplete) | no | no | referent element | 5.3; 8; 1419 |
| `StoreRefBits` / `store.ref.bits` | ref frame argument, index..., position, value | integral indices/position/target; valid parent window; value operand (fit check incomplete) | no | no | clipped referent bits | 5.3; 8; 1420 |
| `ContainerNew` / `container.new` | size, initializer | result dynarray<!T>; size s4<64>; initializer equivalent element | no | no | new independent heap cells | 4.2.1; 5.8; 1188 |
| `ContainerCopy` / `container.copy` | container | result container; equivalent source/result elements | no | no | new independent heap cells; destination bounds | 5.8; 1189 |
| `ContainerSnapshot` / `container.snapshot` | container | exact source/result container type | no | no | new independent cells; actual source bounds | 5.8; 1190 |
| `ContainerAssign` / `container.assign` | !T, receiver ref, source | ref<!T>; source equivalent element type | no | no | receiver handle/cells after evaluating source | 5.8; 1230 |
| `ContainerPack` / `container.pack` | element value... (zero allowed) | each value equivalent to result container element | no | no | new heap cells | 5.8; 1191 |
| `ContainerSize` / `container.size` | container | result i2<64> | no | no | read logical size | 5.8; 1215 |
| `ContainerRead` / `container.read` | container, index | integral index; result = element | no | no | read cell/default | 5.8; 1216 |
| `ContainerCell` / `container.cell` | !T, receiver ref, index | ref<!T>; integral index; element-ref result, qualified if nested | no | no | read cell identity | 5.8; 1229 |
| `ContainerWrite` / `container.write` | !T, receiver ref, index, value | ref<!T>; integral index; equivalent element | no | no | receiver cell; change notifications | 5.8; 1231 |
| `ContainerClear` / `container.clear` | !T, receiver ref | ref<!T> | no | no | receiver handle/cells | 5.8; 1232 |
| `QueueInsert` / `queue.insert` | queue !T, receiver ref, index, value | ref<!T>; integral index; equivalent element | no | no | queue structure/cells | 5.8; 1233 |
| `QueuePushFront` / `queue.push.front` | queue !T, receiver ref, value | ref<!T>; equivalent element | no | no | queue structure/cells | 5.8; 1234 |
| `QueuePushBack` / `queue.push.back` | queue !T, receiver ref, value | ref<!T>; equivalent element | no | no | queue structure/cells | 5.8; 1235 |
| `QueuePopFront` / `queue.pop.front` | queue !T, receiver ref | ref<!T>; result = element | no | no | queue structure/cells; result/default | 5.8; 1236 |
| `QueuePopBack` / `queue.pop.back` | queue !T, receiver ref | ref<!T>; result = element | no | no | queue structure/cells; result/default | 5.8; 1237 |
| `QueueErase` / `queue.erase` | queue !T, receiver ref, index | ref<!T>; integral index | no | no | queue structure/cells | 5.8; 1238 |
| `Add` / `add` | x, y | same integral or real type for operands/result | no | no | result only | 5.2; 1450 |
| `Sub` / `sub` | x, y | same integral or real type for operands/result | no | no | result only | 5.2; 1451 |
| `Mul` / `mul` | x, y | same integral or real type for operands/result | no | no | result only | 5.2; 1452 |
| `Div` / `div` | x, y | same integral or real type for operands/result | no | no | result only | 5.2; 1453 |
| `Mod` / `mod` | x, y | same integral type for operands/result | no | no | result only | 5.2; 1466 |
| `Pow` / `pow` | base, exponent | base/result same integral with arbitrary integral exponent; or all same real | no | no | result only | 5.2; 1458 |
| `Neg` / `neg` | x | operand/result same integral or real | no | no | result only | 5.2; 1474 |
| `Lt` / `lt` | x, y | equal integral/real/string/chandle operands; result 1 bit in operand domain | no | no | result only | 5.2; 1483 |
| `Le` / `le` | x, y | equal integral/real/string/chandle operands; result 1 bit in operand domain | no | no | result only | 5.2; 1484 |
| `Gt` / `gt` | x, y | equal integral/real/string/chandle operands; result 1 bit in operand domain | no | no | result only | 5.2; 1485 |
| `Ge` / `ge` | x, y | equal integral/real/string/chandle operands; result 1 bit in operand domain | no | no | result only | 5.2; 1486 |
| `Eq` / `eq` | x, y | equal integral/real/string/chandle operands; result 1 bit in operand domain | no | no | result only | 5.2; 1487 |
| `Ne` / `ne` | x, y | equal integral/real/string/chandle operands; result 1 bit in operand domain | no | no | result only | 5.2; 1488 |
| `Ceq` / `ceq` | x, y | equal integral operands; result 2-state 1 bit | no | no | result only | 5.2; 1519 |
| `Cne` / `cne` | x, y | equal integral operands; result 2-state 1 bit | no | no | result only | 5.2; 1520 |
| `Weq` / `weq` | x, y | equal integral operands; result 1 bit in operand domain | no | no | result only | 5.2; 1499 |
| `Wne` / `wne` | x, y | equal integral operands; result 1 bit in operand domain | no | no | result only | 5.2; 1500 |
| `MatchMask` / `match.mask` | x, y, casez/casex | equal integral operands; result 2-state 1 bit | no | no | result only | 5.2; 1508 |
| `Lnot` / `lnot` | x | integral/real/chandle input; result 1 bit in input domain | no | no | result only | 5.2; 1528 |
| `Land` / `land` | x, y | integral/real operands; result 1 bit; four-state if either input is | no | no | result only | 5.2; 1546 |
| `Lor` / `lor` | x, y | integral/real operands; result 1 bit; four-state if either input is | no | no | result only | 5.2; 1547 |
| `Limp` / `limp` | x, y | integral/real operands; result 1 bit; four-state if either input is | no | no | result only | 5.2; 1548 |
| `Leqv` / `leqv` | x, y | integral/real operands; result 1 bit; four-state if either input is | no | no | result only | 5.2; 1549 |
| `Truth` / `truth` | x | integral/real/chandle input; result 1 bit in input domain | no | no | result only | 5.2; 1529 |
| `And` / `and` | x, y | same integral type for operands/result | no | no | result only | 5.2; 1467 |
| `Or` / `or` | x, y | same integral type for operands/result | no | no | result only | 5.2; 1468 |
| `Xor` / `xor` | x, y | same integral type for operands/result | no | no | result only | 5.2; 1469 |
| `Xnor` / `xnor` | x, y | same integral type for operands/result | no | no | result only | 5.2; 1470 |
| `Not` / `not` | x | operand/result same integral | no | no | result only | 5.2; 1479 |
| `Rand` / `rand` | x | integral input; result 1 bit in input domain | no | no | result only | 5.2; 1536 |
| `Rnand` / `rnand` | x | integral input; result 1 bit in input domain | no | no | result only | 5.2; 1537 |
| `Ror` / `ror` | x | integral input; result 1 bit in input domain | no | no | result only | 5.2; 1538 |
| `Rnor` / `rnor` | x | integral input; result 1 bit in input domain | no | no | result only | 5.2; 1539 |
| `Rxor` / `rxor` | x | integral input; result 1 bit in input domain | no | no | result only | 5.2; 1540 |
| `Rxnor` / `rxnor` | x | integral input; result 1 bit in input domain | no | no | result only | 5.2; 1541 |
| `Shl` / `shl` | x, amount | x/result same integral; integral amount | no | no | result only | 5.2; 1558 |
| `Shr` / `shr` | x, amount | x/result same integral; integral amount | no | no | result only | 5.2; 1559 |
| `Ashr` / `ashr` | x, amount | x/result same integral; integral amount | no | no | result only | 5.2; 1560 |
| `Select` / `select` | condition, x, y | 1-bit condition; equal nonvoid alternatives/result | no | no | result only | 5.2; 1598 |
| `Bitsel` / `bitsel` | x, index | integral operands; unsigned 1-bit result in x domain | no | no | result only | 5.2; 1603 |
| `Partsel` / `partsel` | x, constant position | known integral position <=64 bits; part inside x; unsigned result in x domain | no | no | result only | 5.2; 1608 |
| `IdxPartsel` / `idxpartsel` | x, position | integral operands; unsigned integral result in x domain | no | no | result only | 5.2; 1625 |
| `Concat` / `concat` | x... (nonempty) | all strings -> string; or unsigned integral result = sum widths, domain union | no | no | result only | 5.2; 1630 |
| `Repl` / `repl` | count, x | integral constant count >0; unsigned result = count*width(x); or integral count/string -> string | no | no | result only | 5.2; 1654 |
| `Zext` / `zext` | x | integral same domain; result width >= operand | no | no | result only | 5.2; 1671 |
| `Sext` / `sext` | x | integral same domain; result width >= operand | no | no | result only | 5.2; 1672 |
| `Trunc` / `trunc` | x | integral same domain; result width <= operand | no | no | result only | 5.2; 1676 |
| `To2` / `to2` | x | four-state -> same width/sign two-state | no | no | result only | 5.2; 1680 |
| `To4` / `to4` | x | two-state integral -> same width/sign four-state | no | no | result only | 5.2; 1684 |
| `Itor` / `itor` | x | integral -> real/shortreal | no | no | result only | 5.2; 1689 |
| `Rtoi` / `rtoi` | x | real/shortreal -> integral | no | no | result only | 5.2; 1693 |
| `Rconv` / `rconv` | x | real <-> shortreal; distinct types | no | no | result only | 5.2; 1697 |
| `Itos` / `itos` | x | integral -> string | no | no | result only | 5.2; 1701 |
| `Stoi` / `stoi` | x | string -> integral | no | no | result only | 5.2; 1705 |
| `StrGet` / `str.get` | string, index | integral index; two-state 8-bit result | no | no | result only | 5.2; 1709 |
| `Trigger` / `trigger` | event slot | one event value in slot | no | no | named event generation | 5.2; 7.2; 1714 |
| `TriggerNb` / `trigger.nb` | event slot, [ticks] | event slot; optional integral ticks | no | no | queued named event generation | 5.2; 7.3; 1719 |
| `Disable` / `disable` | process/subroutine/text | identifier kind checked; legacy text allowed by verifier | no | no | process/call cancellation | 5.2; 7.2; 1725 |
| `DisableFork` / `disable.fork` | none | void | no | no | descendant cancellation | 7.2; 7.5; 1730 |
| `ReportDefer` / `report.defer` | construct, content, scope | three constrained text operands | no | no | pending report queue | 5.2; 1844 |
| `ReportCall` / `report.call` | #0/final, function, values... | function; input-only parameters; each value fits | no | no | captured deferred-call queue | 5.7; 1823 |
| `Resolve` / `resolve` | net kind, (value, strength1, strength0, [side])... | four-state result; driver same type; strengths constants 0..7; optional 2-state side width=2*w | no | no | result only | 3.5; 5.2; 1564 |
| `Udp` / `udp` | UDP !T, inputs... | combinational table; one 1-bit value/input; result i4<1> | no | no | result only | 5.5; 1760 |
| `UdpSeq` / `udp.seq` | UDP !T, state, previous..., current... | sequential table; state + 2*n 1-bit values; result i4<1> | no | no | result only (state stored explicitly) | 5.5; 1761 |
| `OverrideStart` / `override.start` | assign/force, override process, targets... | marked integral held ranges; process kind override | no | no | hold registration and process scheduling | 5.6; 1786 |
| `OverrideStop` / `override.stop` | assign/force, targets... | marked integral held ranges | no | no | hold removal and restoration | 5.6; 1787 |
| `StoreOverride` / `store.override` | assign/force, slot range, value | override process; integral value width=range; within writes | no | no | held slot bits | 5.6; 1803 |
| `Strobe` / `strobe` | postponed process, [descriptor] | postponed kind; optional integral descriptor | no | no | postponed run / monitor registration | 7.1; 1747 |
| `Monitor` / `monitor` | postponed process, [descriptor] | postponed kind; optional integral descriptor | no | no | postponed run / monitor registration | 7.1; 1748 |
| `ScopeEnter` / `scope.enter` | scope #N, exit block | scope < numScopes; argument-free block | no | no | activation record | 7.2; 1731 |
| `ScopeLeave` / `scope.leave` | scope #N | scope < numScopes | no | no | activation record | 7.2; 1736 |
| `DisableScope` / `disable.scope` | scope #N | scope < numScopes | no | no | activation unwind and descendant cancellation | 7.2; 1737 |
| `DisableTask` / `disable.task` | task @N | task kind | no | no | task activations and descendants | 7.2; 8; 1741 |
| `Call` / `call` | subroutine, args... | not guard/watch; mayWait=false; arguments/directions and return type match | no | no | callee effects + copy-out + optional result | 8; 1863 |
| `Syscall` / `syscall` | $name, operands... | name starts $; optional result; special $monitor/$dumpvars restrictions; no full per-task signature check | no | no | task-specific I/O, state, scheduler, heap or result | 5.2; SYSTEM_TASKS; 1877 |
| `FormatPattern` / `pattern.format` | !pN, named/positional, values... | valid pattern tree; payload shape matches; result string | no | no | result/string allocation; heap reads | 5.9; 1170 |
| `Builtin` / `builtin` | method name, operands... | nonempty text; optional result; verifier lacks per-method signature | no | no | method-specific result/string/event access | 5.2; 8; 1906 |
| `Br` / `br` | block(args...) | argument number/types match block | yes | no | parallel block-argument binding | 5.1; 7.2; 1910 |
| `BrIf` / `br.if` | condition, true block(args...), false block(args...) | 1-bit condition; matching block arguments | yes | no | selected parallel block binding | 5.1; 7.2; 1913 |
| `Ret` / `ret` | [value] | void process/task -> no value; otherwise fits return | yes | no | return/copy-out | 7.2; 8; 1920 |
| `WaitDelay` / `wait.delay` | ticks, continuation | integral ticks; continuation no arguments | yes | may | wait registration | 7.2; 1929 |
| `WaitEvent` / `wait.event` | slot/watch [iff guard]..., continuation | slot ranges or waiter-frame watches/guards; continuation no arguments | yes | may | mixed wait registration | 7.2; 1933 |
| `WaitCond` / `wait.cond` | guard, continuation | guard reads waiter frame and returns 1 bit; no continuation args | yes | if false | monitor registration if false | 7.2; 1964 |
| `WaitNamed` / `wait.named` | event slot, continuation | event slot; continuation no arguments | yes | may | named wait registration | 7.2; 1973 |
| `Fork` / `fork` | child process..., join kind, continuation | children owned by caller; join/join_any/join_none; no continuation args | yes | except join_none; can complete immediately | children + join state | 7.2; 7.5; 1979 |
| `WaitFork` / `wait.fork` | continuation | no continuation args | yes | while live direct children | join state | 7.2; 7.5; 2001 |
| `CallTask` / `call.task` | task, args..., continuation | task mayWait=true; matching args; no continuation args | yes | may | call frame + callee effects | 7.2; 8; 2005 |

## Appendix B. Complete kernel API inventory and additions

274 distinct API references. A = candidate coverage requiring a reviewed service before P3 (not an approved public-function list); H = host/tier-internal exception reviewed in P3; I = public-inline implementation path reviewed in P3; existing = current C entry. There are 181 A rows (including three partial existing services), 19 H rows, 60 I rows and 14 fully covered rows. H stack/counter access does not authorize generated code to bypass stop/stack checks: D4 process_checkpoint provides those checks. Each listed line is in src/vm/vm.cpp. API wrappers take C-compatible views rather than the original C++ signatures.

| C++ API reference | vm.cpp lines | ABI 0.3 / missing group | Proposed entry or explicit deferral |
| --- | --- | --- | --- |
| `counters_.add` | 887 | none; infrastructure (ABI 3/5/9, outside 4.2) | H: P3 host loader/driver or tier-local accounting; not called by generated resumes |
| `counters_.increment` | 888, 913, 922, 939, 952, 965 | none; infrastructure (ABI 3/5/9, outside 4.2) | H: P3 host loader/driver or tier-local accounting; not called by generated resumes |
| `ctx.counters` | 623 | none; infrastructure (ABI 3/5/9, outside 4.2) | H: P3 host loader/driver or tier-local accounting; not called by generated resumes |
| `ctx.dump` | 622 | none; infrastructure (ABI 3/5/9, outside 4.2) | H: P3 host loader/driver or tier-local accounting; not called by generated resumes |
| `ctx.files` | 622 | none; infrastructure (ABI 3/5/9, outside 4.2) | H: P3 host loader/driver or tier-local accounting; not called by generated resumes |
| `ctx.heap` | 623 | none; infrastructure (ABI 3/5/9, outside 4.2) | H: P3 host loader/driver or tier-local accounting; not called by generated resumes |
| `ctx.output` | 621 | none; infrastructure (ABI 3/5/9, outside 4.2) | H: P3 host loader/driver or tier-local accounting; not called by generated resumes |
| `ctx.randomSeed` | 622 | none; infrastructure (ABI 3/5/9, outside 4.2) | H: P3 host loader/driver or tier-local accounting; not called by generated resumes |
| `ctx.scheduler` | 621 | none; infrastructure (ABI 3/5/9, outside 4.2) | H: P3 host loader/driver or tier-local accounting; not called by generated resumes |
| `ctx.stack` | 624 | none; infrastructure (ABI 3/5/9, outside 4.2) | H: P3 host loader/driver or tier-local accounting; not called by generated resumes |
| `ctx.store` | 621 | none; infrastructure (ABI 3/5/9, outside 4.2) | H: P3 host loader/driver or tier-local accounting; not called by generated resumes |
| `ctx.strings` | 623 | none; infrastructure (ABI 3/5/9, outside 4.2) | H: P3 host loader/driver or tier-local accounting; not called by generated resumes |
| `dump_.all` | 5493 | none; System tasks | A: `dump_all` |
| `dump_.file` | 5472 | none; System tasks | A: `dump_file` |
| `dump_.finish` | 901 | none; System tasks | A: `dump_finish` |
| `dump_.flush` | 5495 | none; System tasks | A: `dump_flush` |
| `dump_.limit` | 5494 | none; System tasks | A: `dump_limit` |
| `dump_.off` | 5491 | none; System tasks | A: `dump_off` |
| `dump_.on` | 5492 | none; System tasks | A: `dump_on` |
| `dump_.setup` | 885 | none; System tasks | A: `dump_setup` |
| `dump_.vars` | 5488 | none; System tasks | A: `dump_vars` |
| `files_.atEnd` | 5701 | none; System tasks | A: `files_at_end` |
| `files_.close` | 5766 | none; System tasks | A: `files_close` |
| `files_.error` | 5711 | none; System tasks | A: `files_error` |
| `files_.flush` | 5772 | none; System tasks | A: `files_flush` |
| `files_.flushAll` | 5771 | none; System tasks | A: `files_flush_all` |
| `files_.getChar` | 5612 | none; System tasks | A: `files_get_char` |
| `files_.openFd` | 5579 | none; System tasks | A: `files_open_fd` |
| `files_.openMcd` | 5584 | none; System tasks | A: `files_open_mcd` |
| `files_.readBytes` | 5660 | none; System tasks | A: `files_read_bytes` |
| `files_.readLine` | 5599 | none; System tasks | A: `files_read_line` |
| `files_.scan` | 5644 | none; System tasks | A: `files_scan` |
| `files_.seek` | 5694 | none; System tasks | A: `files_seek` |
| `files_.targets` | 5279 | none; System tasks | A: `files_targets` |
| `files_.tell` | 5674 | none; System tasks | A: `files_tell` |
| `files_.ungetChar` | 5619 | none; System tasks | A: `files_unget_char` |
| `files_.write` | 5557, 5569 | none; System tasks | A: `files_write` |
| `heap_.actualType` | 2335 | none; Objects | A: `heap_actual_type` |
| `heap_.addLayout` | 2290 | none; Objects | A: `heap_add_layout` |
| `heap_.addType` | 2292 | none; Objects | A: `heap_add_type` |
| `heap_.cell` | 2418 | none; Objects | A: `heap_cell` |
| `heap_.clear` | 2437 | none; Objects | A: `heap_clear` |
| `heap_.collect` | 2231 | none; Objects | A: `heap_collect` |
| `heap_.construct` | 2376 | none; Objects | A: `heap_construct` |
| `heap_.copy` | 2377, 2431 | none; Objects | A: `heap_copy` |
| `heap_.describeType` | 2413 | none; Objects | A: `heap_describe_type` |
| `heap_.describeWord` | 2327 | none; Objects | A: `heap_describe_word` |
| `heap_.erase` | 2439 | none; Objects | A: `heap_erase` |
| `heap_.fromValues` | 2392 | none; Objects | A: `heap_from_values` |
| `heap_.insert` | 2448 | none; Objects | A: `heap_insert` |
| `heap_.memoryResource` | 4888 | none; Objects | A: `heap_memory_resource` |
| `heap_.needsCollection` | 2230 | none; Objects | A: `heap_needs_collection` |
| `heap_.pop` | 2441 | none; Objects | A: `heap_pop` |
| `heap_.push` | 2450 | none; Objects | A: `heap_push` |
| `heap_.reaches` | 3034 | none; Objects | A: `heap_reaches` |
| `heap_.read` | 2407, 5018 | none; Objects | A: `heap_read` |
| `heap_.readCell` | 2343, 2480 | none; Objects | A: `heap_read_cell` |
| `heap_.size` | 2401, 4948 | none; Objects | A: `heap_size` |
| `heap_.snapshot` | 2378 | none; Objects | A: `heap_snapshot` |
| `heap_.storeNested` | 2354, 2520 | none; Objects | A: `heap_store_nested` |
| `heap_.words` | 2479 | none; Objects | A: `heap_words` |
| `heap_.write` | 2446 | none; Objects | A: `heap_write` |
| `heap_.writeCell` | 2524 | none; Objects | A: `heap_write_cell` |
| `kernel::HeapLayoutId::index` | 2422 | none; pure values (ABI 3; not a 4.2 table operation) | I: P3 public inline C++20 helper; same implementation, no dynamic kernel symbol |
| `kernel::HeapResult<kernel::HeapTypeId>::operator bool` | 2336 | none; pure values (ABI 3; not a 4.2 table operation) | I: P3 public inline C++20 helper; same implementation, no dynamic kernel symbol |
| `kernel::HeapRoot::cell` | 2120 | none; Objects | A: `heap_root_cell` |
| `kernel::HeapRoot::container` | 2108, 3032 | none; Objects | A: `heap_root_container` |
| `kernel::HeapStatus::operator bool` | 2236, 2253 | none; pure values (ABI 3; not a 4.2 table operation) | I: P3 public inline C++20 helper; same implementation, no dynamic kernel symbol |
| `kernel::HeapTypeId::index` | 2318 | none; pure values (ABI 3; not a 4.2 table operation) | I: P3 public inline C++20 helper; same implementation, no dynamic kernel symbol |
| `kernel::PatternFormatResult::operator bool` | 5027 | none; pure values (ABI 3; not a 4.2 table operation) | I: P3 public inline C++20 helper; same implementation, no dynamic kernel symbol |
| `kernel::Rng::child` | 803, 1402 | none; System tasks | A: `rng_child` |
| `kernel::Rng::draw` | 5180 | none; System tasks | A: `rng_draw` |
| `kernel::SlotId::index` | 741, 742, 743, 2134 | none; pure values (ABI 3; not a 4.2 table operation) | I: P3 public inline C++20 helper; same implementation, no dynamic kernel symbol |
| `kernel::StackRange::ofThisThread` | 723 | none; infrastructure (ABI 3/5/9, outside 4.2) | H: P3 host loader/driver or tier-local accounting; not called by generated resumes |
| `kernel::add4` | 4565 | none; pure values (ABI 3; not a 4.2 table operation) | I: P3 public inline C++20 helper; same implementation, no dynamic kernel symbol |
| `kernel::allX` | 3247, 4630, 5918 | none; pure values (ABI 3; not a 4.2 table operation) | I: P3 public inline C++20 helper; same implementation, no dynamic kernel symbol |
| `kernel::and4` | 4599 | none; pure values (ABI 3; not a 4.2 table operation) | I: P3 public inline C++20 helper; same implementation, no dynamic kernel symbol |
| `kernel::asSigned` | 413 | none; pure values (ABI 3; not a 4.2 table operation) | I: P3 public inline C++20 helper; same implementation, no dynamic kernel symbol |
| `kernel::ashr4` | 4612 | none; pure values (ABI 3; not a 4.2 table operation) | I: P3 public inline C++20 helper; same implementation, no dynamic kernel symbol |
| `kernel::bits4` | 4620, 4625, 4630 | none; pure values (ABI 3; not a 4.2 table operation) | I: P3 public inline C++20 helper; same implementation, no dynamic kernel symbol |
| `kernel::caseMatch4` | 4586 | none; pure values (ABI 3; not a 4.2 table operation) | I: P3 public inline C++20 helper; same implementation, no dynamic kernel symbol |
| `kernel::ceq4` | 4581 | none; pure values (ABI 3; not a 4.2 table operation) | I: P3 public inline C++20 helper; same implementation, no dynamic kernel symbol |
| `kernel::cne4` | 4582 | none; pure values (ABI 3; not a 4.2 table operation) | I: P3 public inline C++20 helper; same implementation, no dynamic kernel symbol |
| `kernel::compileUdpRow` | 652 | none; Writing values / pure helper facade (ABI 3) | A: `value_compile_udp_row` |
| `kernel::concat4` | 4636 | none; pure values (ABI 3; not a 4.2 table operation) | I: P3 public inline C++20 helper; same implementation, no dynamic kernel symbol |
| `kernel::countBits4` | 5430 | none; pure values (ABI 3; not a 4.2 table operation) | I: P3 public inline C++20 helper; same implementation, no dynamic kernel symbol |
| `kernel::distChiSquare` | 5746 | none; System tasks | A: `system_dist_chi_square` |
| `kernel::distErlang` | 5750 | none; System tasks | A: `system_dist_erlang` |
| `kernel::distExponential` | 5742 | none; System tasks | A: `system_dist_exponential` |
| `kernel::distNormal` | 5740 | none; System tasks | A: `system_dist_normal` |
| `kernel::distPoisson` | 5744 | none; System tasks | A: `system_dist_poisson` |
| `kernel::distT` | 5748 | none; System tasks | A: `system_dist_t` |
| `kernel::distUniform` | 5738 | none; System tasks | A: `system_dist_uniform` |
| `kernel::div4` | 4568 | none; pure values (ABI 3; not a 4.2 table operation) | I: P3 public inline C++20 helper; same implementation, no dynamic kernel symbol |
| `kernel::dumpOrder` | 5331 | none; System tasks | A: `system_dump_order` |
| `kernel::eq4` | 4579 | none; pure values (ABI 3; not a 4.2 table operation) | I: P3 public inline C++20 helper; same implementation, no dynamic kernel symbol |
| `kernel::eventHandle` | 773 | none; Writing values / pure helper facade (ABI 3) | A: `value_event_handle` |
| `kernel::eventOfHandle` | 3091, 4745, 4755 | none; Writing values / pure helper facade (ABI 3) | A: `value_event_of_handle` |
| `kernel::formatInt` | 4851, 5086, 5342 | none; System tasks | A: `system_format_int` |
| `kernel::formatPattern` | 5024 | none; System tasks | A: `system_format_pattern` |
| `kernel::formatReal` | 4855, 5080 | none; System tasks | A: `system_format_real` |
| `kernel::formatString` | 5072 | none; System tasks | A: `system_format_string` |
| `kernel::formatStringCall` | 5529 | none; System tasks | A: `system_format_string_call` |
| `kernel::formatTime` | 5085 | none; System tasks | A: `system_format_time` |
| `kernel::formatTimeReal` | 5074 | none; System tasks | A: `system_format_time_real` |
| `kernel::formattedString` | 5092 | none; System tasks | A: `system_formatted_string` |
| `kernel::fullyInside` | 4033, 4059 | none; pure values (ABI 3; not a 4.2 table operation) | I: P3 public inline C++20 helper; same implementation, no dynamic kernel symbol |
| `kernel::ge4` | 4578 | none; pure values (ABI 3; not a 4.2 table operation) | I: P3 public inline C++20 helper; same implementation, no dynamic kernel symbol |
| `kernel::gt4` | 4577 | none; pure values (ABI 3; not a 4.2 table operation) | I: P3 public inline C++20 helper; same implementation, no dynamic kernel symbol |
| `kernel::initialState` | 799 | none; System tasks | A: `system_initial_state` |
| `kernel::inside` | 1719, 2511, 3616, 3939 | none; pure values (ABI 3; not a 4.2 table operation) | I: P3 public inline C++20 helper; same implementation, no dynamic kernel symbol |
| `kernel::land4` | 3721, 3725, 3726, 4254, 4590 | none; pure values (ABI 3; not a 4.2 table operation) | I: P3 public inline C++20 helper; same implementation, no dynamic kernel symbol |
| `kernel::le4` | 4576 | none; pure values (ABI 3; not a 4.2 table operation) | I: P3 public inline C++20 helper; same implementation, no dynamic kernel symbol |
| `kernel::lnot4` | 3709, 3723, 3726, 4222, 4257, 4588, 4592 | none; pure values (ABI 3; not a 4.2 table operation) | I: P3 public inline C++20 helper; same implementation, no dynamic kernel symbol |
| `kernel::loadMemory` | 5366 | none; System tasks | A: `system_load_memory` |
| `kernel::lor4` | 3722, 3723, 3727, 4255, 4257, 4591, 4592 | none; pure values (ABI 3; not a 4.2 table operation) | I: P3 public inline C++20 helper; same implementation, no dynamic kernel symbol |
| `kernel::lt4` | 4575 | none; pure values (ABI 3; not a 4.2 table operation) | I: P3 public inline C++20 helper; same implementation, no dynamic kernel symbol |
| `kernel::mask` | 759, 760, 1722, 1723, 2283, 3428, 4722, 4723, 4737, 4765 | none; pure values (ABI 3; not a 4.2 table operation) | I: P3 public inline C++20 helper; same implementation, no dynamic kernel symbol |
| `kernel::mod4` | 4569 | none; pure values (ABI 3; not a 4.2 table operation) | I: P3 public inline C++20 helper; same implementation, no dynamic kernel symbol |
| `kernel::mul4` | 4567 | none; pure values (ABI 3; not a 4.2 table operation) | I: P3 public inline C++20 helper; same implementation, no dynamic kernel symbol |
| `kernel::ne4` | 4580 | none; pure values (ABI 3; not a 4.2 table operation) | I: P3 public inline C++20 helper; same implementation, no dynamic kernel symbol |
| `kernel::neg4` | 4574 | none; pure values (ABI 3; not a 4.2 table operation) | I: P3 public inline C++20 helper; same implementation, no dynamic kernel symbol |
| `kernel::negate1` | 3744, 3746, 3748 | none; pure values (ABI 3; not a 4.2 table operation) | I: P3 public inline C++20 helper; same implementation, no dynamic kernel symbol |
| `kernel::not4` | 4603 | none; pure values (ABI 3; not a 4.2 table operation) | I: P3 public inline C++20 helper; same implementation, no dynamic kernel symbol |
| `kernel::or4` | 4600 | none; pure values (ABI 3; not a 4.2 table operation) | I: P3 public inline C++20 helper; same implementation, no dynamic kernel symbol |
| `kernel::parseFormat` | 1236 | none; System tasks | A: `system_parse_format` |
| `kernel::plusargValue` | 5866 | none; System tasks | A: `system_plusarg_value` |
| `kernel::position4` | 3487 | none; pure values (ABI 3; not a 4.2 table operation) | I: P3 public inline C++20 helper; same implementation, no dynamic kernel symbol |
| `kernel::pow4` | 4571 | none; pure values (ABI 3; not a 4.2 table operation) | I: P3 public inline C++20 helper; same implementation, no dynamic kernel symbol |
| `kernel::preparePlusargFormat` | 5855 | none; System tasks | A: `system_prepare_plusarg_format` |
| `kernel::prepareScanFormat` | 4679 | none; System tasks | A: `system_prepare_scan_format` |
| `kernel::randomInt` | 5736 | none; System tasks | A: `system_random_int` |
| `kernel::readField` | 1558, 1559, 1592, 1593 | none; pure values (ABI 3; not a 4.2 table operation) | I: P3 public inline C++20 helper; same implementation, no dynamic kernel symbol |
| `kernel::readFile` | 5355 | none; System tasks | A: `system_read_file` |
| `kernel::redAnd4` | 4604 | none; pure values (ABI 3; not a 4.2 table operation) | I: P3 public inline C++20 helper; same implementation, no dynamic kernel symbol |
| `kernel::redNand4` | 4605 | none; pure values (ABI 3; not a 4.2 table operation) | I: P3 public inline C++20 helper; same implementation, no dynamic kernel symbol |
| `kernel::redNor4` | 4607 | none; pure values (ABI 3; not a 4.2 table operation) | I: P3 public inline C++20 helper; same implementation, no dynamic kernel symbol |
| `kernel::redOr4` | 4606 | none; pure values (ABI 3; not a 4.2 table operation) | I: P3 public inline C++20 helper; same implementation, no dynamic kernel symbol |
| `kernel::redXnor4` | 4609 | none; pure values (ABI 3; not a 4.2 table operation) | I: P3 public inline C++20 helper; same implementation, no dynamic kernel symbol |
| `kernel::redXor4` | 4608 | none; pure values (ABI 3; not a 4.2 table operation) | I: P3 public inline C++20 helper; same implementation, no dynamic kernel symbol |
| `kernel::repl4` | 4640 | none; pure values (ABI 3; not a 4.2 table operation) | I: P3 public inline C++20 helper; same implementation, no dynamic kernel symbol |
| `kernel::resolve4` | 3412, 3433 | none; Writing values / pure helper facade (ABI 3) | A: `value_resolve4` |
| `kernel::scanString` | 5838 | none; System tasks | A: `system_scan_string` |
| `kernel::select4` | 4613 | none; pure values (ABI 3; not a 4.2 table operation) | I: P3 public inline C++20 helper; same implementation, no dynamic kernel symbol |
| `kernel::sext4` | 4644 | none; pure values (ABI 3; not a 4.2 table operation) | I: P3 public inline C++20 helper; same implementation, no dynamic kernel symbol |
| `kernel::shl4` | 4610 | none; pure values (ABI 3; not a 4.2 table operation) | I: P3 public inline C++20 helper; same implementation, no dynamic kernel symbol |
| `kernel::shr4` | 4611 | none; pure values (ABI 3; not a 4.2 table operation) | I: P3 public inline C++20 helper; same implementation, no dynamic kernel symbol |
| `kernel::sub4` | 4566 | none; pure values (ABI 3; not a 4.2 table operation) | I: P3 public inline C++20 helper; same implementation, no dynamic kernel symbol |
| `kernel::to2` | 3784, 4083, 4738, 5941 | none; pure values (ABI 3; not a 4.2 table operation) | I: P3 public inline C++20 helper; same implementation, no dynamic kernel symbol |
| `kernel::trunc4` | 4645 | none; pure values (ABI 3; not a 4.2 table operation) | I: P3 public inline C++20 helper; same implementation, no dynamic kernel symbol |
| `kernel::truth4` | 420, 4160, 4235, 4589, 4594, 4595 | none; pure values (ABI 3; not a 4.2 table operation) | I: P3 public inline C++20 helper; same implementation, no dynamic kernel symbol |
| `kernel::udpOutput` | 4376 | none; Writing values / pure helper facade (ABI 3) | A: `value_udp_output` |
| `kernel::udpOutputRead` | 4368 | none; Writing values / pure helper facade (ABI 3) | A: `value_udp_output_read` |
| `kernel::udpStep` | 4382 | none; Writing values / pure helper facade (ABI 3) | A: `value_udp_step` |
| `kernel::udpStepRead` | 4370 | none; Writing values / pure helper facade (ABI 3) | A: `value_udp_step_read` |
| `kernel::urandomRange` | 5176 | none; System tasks | A: `system_urandom_range` |
| `kernel::weq4` | 4583 | none; pure values (ABI 3; not a 4.2 table operation) | I: P3 public inline C++20 helper; same implementation, no dynamic kernel symbol |
| `kernel::wide::add` | 3632 | none; Writing values / pure helper facade (ABI 3) | A: `value_wide_add` |
| `kernel::wide::allX` | 3256, 3795 | none; Writing values / pure helper facade (ABI 3) | A: `value_wide_all_x` |
| `kernel::wide::ashr` | 3761 | none; Writing values / pure helper facade (ABI 3) | A: `value_wide_ashr` |
| `kernel::wide::bitAnd` | 3659 | none; Writing values / pure helper facade (ABI 3) | A: `value_wide_bit_and` |
| `kernel::wide::bitNot` | 3665 | none; Writing values / pure helper facade (ABI 3) | A: `value_wide_bit_not` |
| `kernel::wide::bitOr` | 3660 | none; Writing values / pure helper facade (ABI 3) | A: `value_wide_bit_or` |
| `kernel::wide::bitXnor` | 3662 | none; Writing values / pure helper facade (ABI 3) | A: `value_wide_bit_xnor` |
| `kernel::wide::bitXor` | 3661 | none; Writing values / pure helper facade (ABI 3) | A: `value_wide_bit_xor` |
| `kernel::wide::bits` | 2515, 3620, 3783, 3798, 3943, 5010 | none; Writing values / pure helper facade (ABI 3) | A: `value_wide_bits` |
| `kernel::wide::caseMatch` | 3698 | none; Writing values / pure helper facade (ABI 3) | A: `value_wide_case_match` |
| `kernel::wide::ceq` | 3691 | none; Writing values / pure helper facade (ABI 3) | A: `value_wide_ceq` |
| `kernel::wide::chunks` | 2514, 3464, 3619, 3942 | none; Writing values / pure helper facade (ABI 3) | A: `value_wide_chunks` |
| `kernel::wide::cne` | 3692 | none; Writing values / pure helper facade (ABI 3) | A: `value_wide_cne` |
| `kernel::wide::countBits` | 5428 | none; Writing values / pure helper facade (ABI 3) | A: `value_wide_count_bits` |
| `kernel::wide::div` | 3637 | none; Writing values / pure helper facade (ABI 3) | A: `value_wide_div` |
| `kernel::wide::eq` | 3689 | none; Writing values / pure helper facade (ABI 3) | A: `value_wide_eq` |
| `kernel::wide::fromReal` | 3845, 4228, 5902 | none; Writing values / pure helper facade (ABI 3) | A: `value_wide_from_real` |
| `kernel::wide::fromText` | 4121, 4125, 4709, 4735, 5096, 5605, 5716 | none; Writing values / pure helper facade (ABI 3) | A: `value_wide_from_text` |
| `kernel::wide::ge` | 3686 | none; Writing values / pure helper facade (ABI 3) | A: `value_wide_ge` |
| `kernel::wide::gt` | 3681 | none; Writing values / pure helper facade (ABI 3) | A: `value_wide_gt` |
| `kernel::wide::hasUnknown` | 1538, 4660, 5829, 5917 | none; Writing values / pure helper facade (ABI 3) | A: `value_wide_has_unknown` |
| `kernel::wide::le` | 3676 | none; Writing values / pure helper facade (ABI 3) | A: `value_wide_le` |
| `kernel::wide::lt` | 3671 | none; Writing values / pure helper facade (ABI 3) | A: `value_wide_lt` |
| `kernel::wide::mod` | 3642 | none; Writing values / pure helper facade (ABI 3) | A: `value_wide_mod` |
| `kernel::wide::mul` | 3634 | none; Writing values / pure helper facade (ABI 3) | A: `value_wide_mul` |
| `kernel::wide::ne` | 3690 | none; Writing values / pure helper facade (ABI 3) | A: `value_wide_ne` |
| `kernel::wide::neg` | 3655 | none; Writing values / pure helper facade (ABI 3) | A: `value_wide_neg` |
| `kernel::wide::position` | 3489 | none; Writing values / pure helper facade (ABI 3) | A: `value_wide_position` |
| `kernel::wide::pow` | 3648 | none; Writing values / pure helper facade (ABI 3) | A: `value_wide_pow` |
| `kernel::wide::put` | 2516, 3624, 3810, 3820, 3945, 5667 | none; Writing values / pure helper facade (ABI 3) | A: `value_wide_put` |
| `kernel::wide::redAnd` | 3743, 3744 | none; Writing values / pure helper facade (ABI 3) | A: `value_wide_red_and` |
| `kernel::wide::redOr` | 3745, 3746 | none; Writing values / pure helper facade (ABI 3) | A: `value_wide_red_or` |
| `kernel::wide::redXor` | 3747, 3748 | none; Writing values / pure helper facade (ABI 3) | A: `value_wide_red_xor` |
| `kernel::wide::resize` | 3465, 3830 | none; Writing values / pure helper facade (ABI 3) | A: `value_wide_resize` |
| `kernel::wide::select` | 3769 | none; Writing values / pure helper facade (ABI 3) | A: `value_wide_select` |
| `kernel::wide::shl` | 3759 | none; Writing values / pure helper facade (ABI 3) | A: `value_wide_shl` |
| `kernel::wide::sub` | 3633, 4718 | none; Writing values / pure helper facade (ABI 3) | A: `value_wide_sub` |
| `kernel::wide::to2` | 3476, 3831, 4726, 5013 | none; Writing values / pure helper facade (ABI 3) | A: `value_wide_to2` |
| `kernel::wide::toReal` | 3841, 5065 | none; Writing values / pure helper facade (ABI 3) | A: `value_wide_to_real` |
| `kernel::wide::toText` | 4116, 4661 | none; Writing values / pure helper facade (ABI 3) | A: `value_wide_to_text` |
| `kernel::wide::truth` | 3704, 3709, 3718, 3719 | none; Writing values / pure helper facade (ABI 3) | A: `value_wide_truth` |
| `kernel::wide::weq` | 3693 | none; Writing values / pure helper facade (ABI 3) | A: `value_wide_weq` |
| `kernel::wide::wne` | 3694 | none; Writing values / pure helper facade (ABI 3) | A: `value_wide_wne` |
| `kernel::wne4` | 4584 | none; pure values (ABI 3; not a 4.2 table operation) | I: P3 public inline C++20 helper; same implementation, no dynamic kernel symbol |
| `kernel::writeBits4` | 4346, 4515 | none; pure values (ABI 3; not a 4.2 table operation) | I: P3 public inline C++20 helper; same implementation, no dynamic kernel symbol |
| `kernel::writeField` | 759, 760 | none; pure values (ABI 3; not a 4.2 table operation) | I: P3 public inline C++20 helper; same implementation, no dynamic kernel symbol |
| `kernel::xnor4` | 4602 | none; pure values (ABI 3; not a 4.2 table operation) | I: P3 public inline C++20 helper; same implementation, no dynamic kernel symbol |
| `kernel::xor4` | 4601 | none; pure values (ABI 3; not a 4.2 table operation) | I: P3 public inline C++20 helper; same implementation, no dynamic kernel symbol |
| `output_.message` | 897, 1087, 1441, 4149, 5141, 5407, 5788 | none; System tasks | A: `output_message` |
| `output_.write` | 5214, 5559 | none; System tasks | A: `output_write` |
| `sched_.addChildProcess` | 1408 | none; Waits and scheduling | A: `scheduler_add_child_process` |
| `sched_.addDelayedDriver` | 693 | none; Waits and scheduling | A: `scheduler_add_delayed_driver` |
| `sched_.addEvent` | 773, 1376 | none; Waits and scheduling | A: `scheduler_add_event` |
| `sched_.addFileMonitor` | 2789 | none; Waits and scheduling | A: `scheduler_add_file_monitor` |
| `sched_.addProcess` | 810, 816, 852, 859, 866, 871, 876, 2787 | yes: `add_process` | existing |
| `sched_.addProgram` | 828 | none; Waits and scheduling | A: `scheduler_add_program` |
| `sched_.addProgramInitial` | 850 | none; Waits and scheduling | A: `scheduler_add_program_initial` |
| `sched_.cancelWait` | 3047, 3240 | none; Waits and scheduling | A: `scheduler_cancel_wait` |
| `sched_.deferCall` | 4542 | none; Reports | A: `scheduler_defer_call` |
| `sched_.deferReport` | 4560 | none; Reports | A: `scheduler_defer_report` |
| `sched_.detachDisplayMonitor` | 3046 | none; Waits and scheduling | A: `scheduler_detach_display_monitor` |
| `sched_.driveDelayed` | 2742 | none; Writing values | A: `scheduler_drive_delayed` |
| `sched_.ended` | 823 | none; Waits and scheduling | A: `scheduler_ended` |
| `sched_.ending` | 813, 2598, 2658, 2692, 2762, 2769, 2830, 4089 | yes: `ending` | existing |
| `sched_.error` | 821, 903 | none; Time and ending | A: `scheduler_error` |
| `sched_.exitProgram` | 5775 | none; Waits and scheduling | A: `scheduler_exit_program` |
| `sched_.fail` | 1429, 2177, 2182, 2192, 2211, 2237 | yes: `fail` | existing |
| `sched_.failedHeapMonitorProcess` | 3054 | none; Waits and scheduling | A: `scheduler_failed_heap_monitor_process` |
| `sched_.finished` | 2175 | none; Time and ending | A: `scheduler_finished` |
| `sched_.flushHeapChanges` | 4087 | none; Waits and scheduling | A: `scheduler_flush_heap_changes` |
| `sched_.hasPending` | 1285 | none; Waits and scheduling | A: `scheduler_has_pending` |
| `sched_.heapMonitorLimited` | 3052 | none; Waits and scheduling | A: `scheduler_heap_monitor_limited` |
| `sched_.hold` | 2866 | none; Writing values | A: `scheduler_hold` |
| `sched_.holdsAny` | 3238 | none; Writing values | A: `scheduler_holds_any` |
| `sched_.inPostponed` | 2560 | none; Waits and scheduling | A: `scheduler_in_postponed` |
| `sched_.kill` | 1421, 3153 | none; Waits and scheduling | A: `scheduler_kill` |
| `sched_.markHeld` | 785 | none; Writing values | A: `scheduler_mark_held` |
| `sched_.printingTo` | 5556 | none; Waits and scheduling | A: `scheduler_printing_to` |
| `sched_.programStopped` | 2537, 2599, 2763, 2770, 2831 | none; Time and ending | A: `scheduler_program_stopped` |
| `sched_.reason` | 820, 902 | none; Time and ending | A: `scheduler_reason` |
| `sched_.release` | 2813 | none; Writing values | A: `scheduler_release` |
| `sched_.releaseProcess` | 1294 | none; Waits and scheduling | A: `scheduler_release_process` |
| `sched_.requestEnd` | 2555, 5409, 5790 | yes: `request_end` | existing |
| `sched_.resourceLimit` | 1088, 2151 | none; Time and ending | A: `scheduler_resource_limit` |
| `sched_.run` | 891 | none; infrastructure (ABI 3/5/9, outside 4.2) | H: P3 host loader/driver or tier-local accounting; not called by generated resumes |
| `sched_.runFinal` | 892 | none; infrastructure (ABI 3/5/9, outside 4.2) | H: P3 host loader/driver or tier-local accounting; not called by generated resumes |
| `sched_.runtimeError` | 1442 | none; Time and ending | A: `scheduler_runtime_error` |
| `sched_.scheduleNba` | 1571, 1646 | yes: `schedule_nba` | existing |
| `sched_.setEndNote` | 2554 | none; Time and ending | A: `scheduler_set_end_note` |
| `sched_.setMonitor` | 2804 | none; Waits and scheduling | A: `scheduler_set_monitor` |
| `sched_.setMonitoring` | 5465, 5467 | none; Waits and scheduling | A: `scheduler_set_monitoring` |
| `sched_.setSafePointHook` | 731, 733, 2300 | none; Waits and scheduling | A: `scheduler_set_safe_point_hook` |
| `sched_.start` | 853, 890, 1411, 3228 | yes: `start` | existing |
| `sched_.strobe` | 2775, 2777 | none; Waits and scheduling | A: `scheduler_strobe` |
| `sched_.takeEndedFileMonitor` | 1322 | none; Waits and scheduling | A: `scheduler_take_ended_file_monitor` |
| `sched_.takeEndedProgramProcess` | 1314 | none; Waits and scheduling | A: `scheduler_take_ended_program_process` |
| `sched_.takeInterrupt` | 2571, 2577 | none; Time and ending | A: `scheduler_take_interrupt` |
| `sched_.takeOverflow` | 2635, 2836 | none; Time and ending | A: `scheduler_take_overflow` |
| `sched_.time` | 1453, 4558, 5406, 5488, 5787, 5799, 5884 | yes: `time` | existing |
| `sched_.trigger` | 1273, 2719 | yes: `trigger` | existing |
| `sched_.triggerNb` | 2728 | yes: `trigger_nb` | existing |
| `sched_.triggered` | 4755 | yes: `triggered` | existing |
| `sched_.visitPendingValues` | 2221 | none; Waits and scheduling | A: `scheduler_visit_pending_values` |
| `sched_.waitAny` | 2684 | yes: `wait_mixed (wait_any for no named items)` | A: `scheduler_wait_mixed_ex` for heap-aware descriptors |
| `sched_.waitDelay` | 2634 | yes: `wait_delay` | existing |
| `sched_.waitEvent` | 1754 | yes: `wait_event` | existing |
| `sched_.waitMonitor` | 3112 | yes: `wait_monitor (slot-only descriptor; see partial note)` | A: `scheduler_wait_monitor_ex` for heap dependencies |
| `sched_.waitNamed` | 1383, 2707 | yes: `wait_named (flushPoint=true only)` | A: `scheduler_wait_named_ex` for explicit flush policy |
| `sched_.write` | 1565, 1618 | yes: `write` | existing |
| `sched_.writeHeld` | 2819 | none; Writing values | A: `scheduler_write_held` |
| `stack_.nearlyUsedUp` | 1074, 2897, 3315 | none; infrastructure (ABI 3/5/9, outside 4.2) | H: P3 host loader/driver or tier-local accounting; not called by generated resumes |
| `store_.add` | 737 | none; infrastructure (ABI 3/5/9, outside 4.2) | H: P3 host loader/driver or tier-local accounting; not called by generated resumes |
| `store_.element` | 756, 773, 1556, 1584, 2172, 3033, 3090 | yes: `slot_words` | existing (validated read view; writes use write/NBA) |
| `store_.layout` | 751, 1554, 1582, 1610, 1624, 1638 | none; infrastructure (ABI 3/5/9, outside 4.2) | H: P3 host loader/driver or tier-local accounting; not called by generated resumes |
| `store_.size` | 724 | none; infrastructure (ABI 3/5/9, outside 4.2) | H: P3 host loader/driver or tier-local accounting; not called by generated resumes |
| `strings_.get` | 4109, 4658, 4759, 4801, 4918, 5072, 5297, 5451, 5472, 5516, 5574, 5577, 5828, 5875 | none; System tasks | A: `strings_get` |
| `strings_.intern` | 634, 4111, 4732, 4762, 5092, 5211, 5603, 5714 | none; System tasks | A: `strings_intern` |
