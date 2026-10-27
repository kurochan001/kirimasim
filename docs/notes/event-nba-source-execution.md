# Source event-NBA lowering

[日本語](event-nba-source-execution.ja.md). Status: validated local source checkpoint, 2026-10-27, on main `81799982`. The historical rejection
expectations were [approved and migrated](event-nba-expectation-migration.md)
in separate expectation-only commit `16c033a4`.
This is not a complete event-NBA or P1 parity claim.

## Rule and implementation boundary

The basis is the approved [event-NBA contract](event-nba-proposal.md), its
[specification mapping](p1-execution-contracts-2026-10-25.md), and the main
[typed SIR/VM checkpoint](event-nba-sir-execution.md). Background clauses are
IEEE 1800-2023 9.4.2, 9.4.5, 10.4.2, 11.4.14, 13.4.4, 15.5 and 18.14.
No standard text/examples or external simulator code/tests were copied.
No new dependency, ABI version, SIR parser or detailed-spec change is added.

`Lowerer::lowerEventNba` captures the RHS, then the full typed repeat count,
then target selectors. `captureNbaWrite` translates the existing typed `Place`
into layout targets. Aggregate assignment and streaming use the same snapshot,
coercion and flattening paths as ordinary assignment. `finishEventNba` publishes
one immutable layout for the complete statement, with conservative write
footprints from `sir::nbaWrites`. It creates no fork or per-leaf countdown.
Named-event binding is skipped for nonpositive/X/Z counts. The ordinary
four-state relational operation and conditional branch decide that path;
the original count still reaches shared-kernel classification unchanged.

Event guards and watches use `waiterOf`'s detached mode: ordinary automatic
nonwaiting SIR functions, with their lexical source scope and no borrowed
issuer-frame variables. Fresh private work, RNG, source depth, notifications,
countdown, payload roots and retention use the existing VM/kernel boundary.
The VM has no new source-shape or source-container cases in this change.
Module, task, function, fork, interface, package, program, generate and class
static-method bodies all reach these same lowering functions.

Strict slang diagnostics and existing condition-purity checks stay in force.
In particular, automatic-variable references in the event control remain
illegal. The [repeat-count example using an automatic input](../measurements/2026-10-27/nba-front-automatic-count-boundary.json) is rejected by
pinned slang and installed Icarus 13.0; installed Icarus 14.0 accepts it.
This observation does not authorize relaxing the default language diagnostics.

## Original verification and measurable limits

The generator `tools/gen_event_nba_matrix.py` adds **1,018** source designs:
880 combinations of 10 containers × 11 write shapes × eight controls; 80
count/effect cases; four captured-event-selection/lifetime cases; eight queue,
region and completion cases; eight wide-count cases; eight RNG/depth cases;
20 aggregate capture-order cases; six disable cases; three ordered-guard and
empty-destination cases; and one independent-count case. Every source has its
own new stdout/stderr oracle marked `spec_check: ai`. Existing source fixtures
and oracles are unchanged. No new skip or PerfCount budget is added.

All 1,018 pass direct execution. The first full dev run includes all 1,018 and
the two new tool tests; its only failures are the old rejection oracle and
stale README counts. The latter is regenerated from real JUnit. The separate
contract tool verifies one control/three writes, detached callbacks, no event
wait/fork expansion, and execution under `--max-procs=1`. The full native
unit suite and 29 existing dump expectations pass unchanged.

[Six compiled faults](../measurements/2026-10-27/nba-front-controls.json) are
all detected: count narrowing, evaluating skipped named selectors, dropping
earlier aggregate leaves, moving count after LHS effects, losing automatic
callback work, and ignoring guards. All sources are restored byte-for-byte,
rebuilt, and all 1,018 cases pass again. This is not a complete mutation run.

The [fixed comparison](../measurements/2026-10-27/nba-front-comparison.json)
retains population **1,456/1,466**, with zero failures/10 unsupported, and
sv-tests **244/344**, with no previously passing case lost. In each versioned
95-probe inventory, only `event_controlled_nba` changes from run failure to
pass. Six gaps remain in that finite inventory; it is not a P1 remainder total.

The [source comparison](../measurements/2026-10-27/nba-front-source-comparison.json)
runs the exact 1,018 original inputs with both installed Icarus versions.
Icarus 13.0 compiles 132, matching 44 expected outputs; 14.0 compiles 138,
matching 47. Compilation failures and output disagreements remain recorded.
The approved project's OR coalescing, capture/pending order, RNG and program
lifetime rules are not inferred from Icarus output.

[60 independent isolation probes](../measurements/2026-10-27/nba-front-isolation.json)
pass native execution and retain the reference observations. For these inputs,
both references assign after the first change for XOR/argument-call event
expressions, retain zero for no-argument function watches, and lose a slot
control when an automatic issuing task returns; keeping that task alive changes
the slot result to the expected two-notification result. The two installed
versions agree on these observations. Their internal causes and the complete
standard interpretation are **not confirmed**: check the cited clauses and
extend original notification/dependency probes. No external implementation
source was read, and no unexplained disagreement is counted as parity.

## Remaining work

The historical CLI rejection migration is approved and applied as `16c033a4`.
[Initial full validation](../measurements/2026-10-27/nba-front-initial-validation.json)
retains the obsolete-oracle failures plus four optional-access lint findings
and one vector-reservation finding. Explicit optional checks and selector
reservation address those findings; the final compiled controls and full CI
are rerun on the corrected source. [Final validation](../measurements/2026-10-27/nba-front-validation.json)
passes all 21 local CI checks: dev/ASan/UBSan pass 8,129/8,131, release passes
8,130/8,132, with zero failures and the same two known skips. All 773 unit tests
and 29 existing dump tests pass. Final source comparisons use the exact final
dev binary hash. The implementation/tests/documents remain local and uncommitted. No source
event-NBA performance claim is made from the earlier dormant native benchmark.
Eligible static scalar/container targets, fixed aggregates, packed selections,
concatenation and streaming forms are exercised; this does not cover all
combinations. Array elements through a `ref`, nested packed windows through a
`ref`, general event-value rebinding/return expressions, managed-event payload
substitution and complete lexical activation/collection matrices remain
unsupported or not confirmed. General event rebinding was already unsupported;
new fixtures use supported runtime event-array selection rather than enabling it.
[Two exploratory string-character NBA observations](../measurements/2026-10-27/nba-front-string-observations.json)
also remain unresolved: native captures a whole string while both reference
runtimes abort. String-character NBA legality and target granularity are not
confirmed; review 6.16/10.4.2/9.4.5 before selecting a correction. These are not
conformance oracles or passing matrix cases. The ordinary-event direct/called-private dependency and source-purity gaps from
the preceding monitor note remain. Linux, default C++ allocator fault injection,
quiet-host performance, the other P1 mechanisms and complete Icarus parity
still require evidence. P1 stays in progress.
