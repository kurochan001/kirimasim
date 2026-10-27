# Conditional dynamic-array and queue values

[日本語](container-conditional-execution.ja.md). Status: integrated into main in `a8bb17cc`,
2026-10-27. P1 remains incomplete.

## Rule and implementation

Grounds: VALUE_SEMANTICS 3.1, 3.9, 3.14 and 1.4; IEEE 1800-2023 11.4.11,
11.4.5, 6.8, 7.5, 7.6 and 7.10. These are existing approved contracts, not
a specification amendment. No external implementation, issue reproducer or
standard example was used. The sources, expected values and faults are original;
new source oracles are marked `spec_check: ai`.

`Lowerer::lowerContainerConditional` is the single source entry for conditional
expressions whose type is a supported dynamic array or queue. It normalizes the
condition using the existing typed truth operation. Known true/false conditions
evaluate only the selected expression; X/Z truth evaluates both, left first.
A wide condition containing a known 1 is true even with other X/Z bits. Each
executed operand is captured independently before a later operand can change it.
The relative operand order is approved project policy; slang's strict read/write
warnings are preserved, not suppressed.

`Lowerer::mergeContainerConditional` implements the unknown-condition rule:
different lengths give the default whole value (empty); equal lengths retain
each complete element only when its `==` is definitely true, otherwise replacing
that element with its type's default. Integral elements are not merged bit by
bit as though the whole array were a packed scalar. Two equal Z-containing
elements do not have definitely true logical equality. Defaults are all X for
four-state integral elements, zero for two-state integrals/reals/shortreals,
empty for strings/nested arrays/queues and null for chandles. This uses the
existing project's scalar typing and default-value contract.

`Lowerer::definitelyEqualValues` compares scalar leaves through `compareValues`
and existing shared value operations. Nested container elements compare lengths
and contents rather than allocation handles; a nested element that is unequal
or has unknown equality becomes an empty container. This is comparison of the
whole element, not recursive conditional merging of its differing leaves.
The result is independent of both source values, including nested storage.
Expression buffers have no queue bound, including nested bounds; ordinary
assignment/copyback applies the actual destination's bounds afterward.

Front emits only existing typed branches, block arguments, container snapshots,
reads and queue operations. VM/kernel behavior, SIR formatVersion 0 and C ABI
0.3 are unchanged. Automatic/static tasks, fork branches, interfaces, packages,
programs, generate scopes, class static methods, function results, structure
members, refs and delayed whole-value NBAs use the same expression entry.
Explicit heap-dependent waits use the existing dependency capture and collection
paths. No new dependency, mutable global state or performance budget is added.

## Verification

The [generator](../../tools/gen_container_conditional_matrix.py) adds **351 original
source designs**: nine execution homes; dynamic arrays/queues; bit/logic widths
1, 4, 32, 64, 65 and 129; X/Z and wide truth; equal/unequal/empty lengths;
complete-element defaults; nested contents and copy independence; effectful
operands and target selectors; actual queue bounds; string, real/NaN, shortreal,
chandle and packed-record elements; functions, delayed NBA snapshots, waits on
size/value changes and 100 rounds across collection boundaries. The automatic
NBA example explicitly uses a static destination, retaining strict legality.
A dedicated Context fixture also runs sequentially, concurrently and with churn.
No existing source body, expected output, skip or PerfCount item is changed.

[Eight compiled faults](../measurements/2026-10-27/container-conditional-controls.json)
are detected by actual source-output assertions: wrong known branch, unknown
truth treated as false, lost left snapshot, ignored length mismatch, packed-style
bit merging, unknown equality accepted, copied nested values treated as unequal
and premature formal-bound truncation. The original source bytes are restored,
the restored binary rebuilds and all selected tests pass. Registry matching is
not counted as a detecting test.

The finite comparison inventory grows from 87 to **95 original probes**.
All eight added conditional probes pass natively; both installed Icarus versions
reject them during compilation. Their failure is recorded, not adopted as an
oracle and not counted as an Icarus-passing gap. Within the earlier 87 probes, `queue_conditional` improves from unsupported
to passed; the other 86 native results retain their status and stdout. The same seven reference-passing gaps remain.
See [Icarus 13 observations](../measurements/2026-10-27/icarus13-container-conditional-probes.json)
and [Icarus 14 observations](../measurements/2026-10-27/icarus14-container-conditional-probes.json).
These are finite original observations, not a proof of whole-language parity.

## Limits and next work

Only already supported element/storage representations are enabled. Class/event
objects, fixed aggregate elements, associative arrays, conditional patterns and
mixed fixed/dynamic operand representations remain unimplemented or unconfirmed;
check each representation with original executable tests before claiming it.
This does not change the separate lowering of conditional fixed aggregates.
Implicit heap sensitivity remains explicitly unsupported. Queue-slice assignment,
additional array methods and the other P1 mechanisms in README remain work.
Performance equivalence is not established by these functional checks; use
PERFORMANCE.md's measurement procedure for any speed claim.

## Local checkpoint validation

[Validation hashes and results](../measurements/2026-10-27/container-conditional-validation.json)
record all 21 local-CI steps passing: dev/ASan/UBSan 6,989/6,991 and release
6,990/6,992, with zero failures and the same two known skips. C++20 public
headers, PerfCount, format/tidy and all 820 mutation anchors pass. The pinned
P1 population remains 1,456 passed, zero failed and ten unsupported out of
1,466. Native sv-tests remains 244/344, 62 unsupported, with no previous pass
lost. The [comparison record](../measurements/2026-10-27/container-conditional-comparison.json)
records the one existing probe improvement and all eight added probes.
The staged-change expectation guard passed at this measurement-time checkpoint.
Main integration is recorded separately in the
[integration record](../measurements/2026-10-27/conditional-nba-main-integration.json).

After full CI, the generator removes trailing whitespace from 136 new source
files. Tokens, line numbers, expected outputs and implementation bytes are
unchanged; all 356 focused checks pass again. The validation record preserves
both full-CI and final hashes and the per-file whitespace-only comparison.
