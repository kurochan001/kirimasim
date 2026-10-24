# Typed assignment-pattern execution checkpoint

[日本語](pattern-sir-execution.ja.md). 2026-10-24. This extends the
[kernel renderer](pattern-format-foundation.md) with typed SIR and execution.
Source-language `%p` is still unsupported: front and both literal/runtime
format-argument bridges remain unfinished. P1/Icarus parity is not established.

## Implemented representation

`Design::patterns` stores immutable presentation layouts with distinct
PatternIds. `pattern.format !pN, "named"|"positional", values...` returns a
string using SYSTEM_TASKS 3.9's existing forms. Layouts describe scalars,
real/string values, enum names, records, fixed arrays, first-member unions,
selected tagged payloads, packed field windows and typed dynamic arrays/queues.
These are explicit presentation descriptions, not runtime source-type guesses.
They do not enable an otherwise unsupported source type or class/event identity.

The verifier checks references, scalar types, names/escape metadata, enum
constants, packed windows, operand count/types, mode, cycles and size overflow.
Graph validation and argument matching are iterative; huge zero-leaf repeated
shapes do not make the verifier enumerate their count. Text writes layout
references, modes, members and offsets; old designs emit no new table.

The VM builds borrowed kernel views with explicit work frames. Packed fields
use declared offsets, signedness and state conversion. Container views use
the actual validated heap type and runtime size, including nested/empty values.
Scratch memory uses the Context heap's resource, and failures end as resource
limits. The existing shared renderer determines output. The kernel still has
no dependency on SIR or slang; the public scheduler header remains C++20.

## Heap-backed display monitors

An original source with `$monitor("%0d", values[0])`, followed by changing
that queue element from 0 to 9, printed only 0 before this change. It now
prints 0 and 9. Both measured Icarus versions refuse that monitor expression
at runtime despite exit code 0; their diagnostic is not a passing oracle.

The scheduler now captures typed heap reads during the actual Postponed
monitor body, without an extra evaluation. Slot bindings and captured
container/cell dependencies schedule subsequent displays. Read footprints are
weak, replaced after each display and detached on replacement, file closure,
callback-process release and VM exit. Change hooks share the existing wait
monitor ownership and restore the previous hook after the last owner leaves.
The display path carries the existing private-call-local read filter.
Registration serials prevent a callback's replacement/reallocation from
receiving an older capture. Capture failures identify the callback process
for the existing resource-limit diagnostic.

The same API is wired to file monitors. New combinations of heap-backed file
monitors, closure/replacement, allocation failures and callback lifetime still
need a dedicated source/kernel matrix; passing old file tests alone does not
prove those combinations. Full source-context coverage is unconfirmed.

## Checked evidence

Eleven original hand-built SIR tests cover scalar/enum/string/real values,
named/positional nested shapes, packed state/sign/offset handling, nested
containers, empty values, 4,000-deep layouts, text, malformed graphs/windows,
operand types/counts/modes, huge zero-leaf shapes and resource failure.
A four-case guard/display-monitor test changes a cell while preserving its
container handle, both directly and inside a record. All 122 selected
PatternSir/Pattern/Format/Scheduler/VmContainer tests pass.

An additional original mixed-state packed-field probe gives X/Z in both
references where native converts a declared two-state member to 0. Clause
7.2.1 requires an implicit four-to-two-state conversion when reading that
member. This discrepancy is not adopted as a new oracle or merged into the
87-probe inventory. SIR metadata retains the explicitly selected field type;
front must preserve slang's typing when source formatting is connected.

The two old wait-monitor mutation anchors are scoped to the wait evaluator
after the read-capture helper was shared. All 513 registry entries apply.
No new compiled mutation campaign is claimed for this checkpoint. Existing
suite bodies, expected output and performance-count expectations remain intact.

## Resume here

First finish the dedicated display-monitor lifetime/file/resource matrix.
Then connect front's type/field/enum metadata and value snapshots to
`pattern.format`, and support both constant and runtime formats through the
same kernel. Preserve argument order, postponed evaluation, container roots
and copyback. Retire old `%p` rejection expectations on a separate review
branch before enabling source support. The pending class/event contracts
remain pending and are not approved by this implementation.

## Checkpoint validation

[Code hashes, CI and original source observations](../measurements/2026-10-24/pattern-sir-validation.json)
are retained. All 21 local-CI checks pass. Dev/ASan/UBSan each have 3,340
passes out of 3,342; release has 3,341 out of 3,343. The same two known
skips remain. LLVM 22.1.8 format/tidy, expectation protection, counts and
other required checks pass. Source `%p`, a dedicated heap file-monitor
matrix and complete functional comparison remain unfinished.
