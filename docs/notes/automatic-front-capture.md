# Lowering shared lexical automatic storage from source

[日本語](automatic-front-capture.ja.md). Status: source implementation committed as `ebb0e14d`, 2026-10-25;
historical expectation migration awaits approval.

## Contract and implementation

This implements the source connection for sections 1–4/6–8 of the
[approved automatic-capture contract](automatic-capture-proposal.md), mapped to
SIR_SPEC 7/8 and RUNTIME_ABI 4/6 by the
[approval record](p1-execution-contracts-2026-10-25.md). Language grounds are
IEEE 1800-2023 6.21, 9.3.2 and 13.5.2. Implementation and tests are original;
no other simulator implementation or test code was read. There is no new
dependency, runtime switch, IR or public ABI.

`Lowerer` records lexical entries/exits and the ownership of source fields.
`promoteLexical` promotes the complete active automatic chain when a fork
actually creates children. `ownFrame` handles later declarations in the same
scope too. `finishLexical` inserts entry/exit instructions at the recorded
positions, including before initializers already lowered before discovery of
a fork. Ordinary bodies and declaration-only forks need no promoted storage.
Compiler temporaries stay ordinary; every source field of an extended scope
keeps its declared type and layout. Static declarations still use slots.

`captureForkEnvironment` imports all active value fields in parent row order,
including unread typed roots and empty lexical extents. Descendants forward
inherited extents and immutable original bindings. References are imported
when used by the child or its descendants; an unused normal ref formal is not
turned into an illegal asynchronous capture. Formal const/ref-static qualifiers
are retained. Strict slang diagnostics keep normal ref restricted to join and
ref static restricted to static actuals; a fork-declaration value initializer
may copy from a normal ref before spawning.

Normal block/fork exits and break/continue/disable/return use the same lexical
ownership bookkeeping. Automatic loop control variables remain shared while
body-local/fork-local declarations receive fresh storage per entry. Returned
expressions are evaluated before scope exit. The
[Interpreter adapter](automatic-vm-execution.md) provides root-before-copy-in,
once-only copyback, transactional group publication, callback pins, cleanup and
typed roots; front records meaning rather than duplicating it in VM branches.

## Source and fault checks

The [new generator](../../tools/gen_automatic_capture_matrix.py) creates 186
original designs: six source containers × three joins × 2/4-state widths
1/7/65/129 (144), eight function-return cases, 24 typed-domain cases and ten
lifetime/reference/control cases. The typed cases include real, shortreal,
string, null chandle, wide fixed arrays, flattened structures, queues and
dynamic arrays. Additional mechanisms cover parent/sibling sharing, distinct
calls, loop reentry/break/continue, fork initializer snapshots, nested forwarding,
output copyback, normal ref with join, ref static, guards/container replacement,
named disable and return from a nested scope.

The [metadata checker](../../tests/tools/check_automatic_capture_metadata.py)
checks every unread typed field and empty extent through two child generations,
then executes the leaf after the original owner has returned. Five negative
boundaries remain strict: normal ref in join_any/join_none, an automatic actual
for ref static, a write through const ref static, and an unread automatic event.
All 188 focused tests pass on the restored production source. Five deliberate
compiled faults are detected: dropped empty extents, omitted unread fields,
lost inherited ownership, lost capture origins and omitted lexical entry.
Compile failure is not counted as detection.

## Finite comparisons

The fresh installed Icarus 14.0 devel (s20260301-411-gca6b8d376) comparison keeps
the same 87 original probes. Only native `automatic_fork_capture` changes, from
run_failed to passed; other native statuses and passing bytes are unchanged.
Native gaps against the historical clean 13.0 record fall from 22 to 21.
This is not a fresh 13.0 rerun or a complete parity claim.

The new 186-design matrix also runs on installed Icarus: 16 pass, 132 have
nonzero compile exit, and 38 have nonzero runtime exit. There is no successful
run with differing stdout in this measurement. Nonpassing diagnostics/output
are omitted from public data. These reference failures do not independently
validate all new oracles; their grounds remain the clauses and the approved
contract. No reference failure is adopted as language behavior.

The exact 1,466-design P1 population remains 1,451 pass, zero fail and 15
unsupported. The pinned 344-design sv-tests set remains 244 pass. Both use
archived exact revisions, not the current external checkout. Linux and the
standard speed comparison have not been remeasured in this checkpoint.

## Pending review and remaining work

The [separate expectation migration](automatic-capture-expectation-migration.md)
retains the old source inputs and proposes successful output/exit expectations
in three historical rejection checks. It is not yet approved or applied.
The initial dev run identifies those three failures plus the stale README count;
after count regeneration, dev has only those three failures. The [complete local-CI record](../measurements/2026-10-25/front-capture-validation.json)
reports 17 successful checks and four failed test steps: dev/ASan/UBSan pass
4,865/4,870 and release 4,866/4,871, with the same three failures and two known
skips each. Builds, static checks, PerfCount and all 29 existing SIR dumps pass.
The committed implementation also passes its separate expected-change check.
This checkpoint must not be described as an all-green local CI result.

Machine-readable records retain [compiled faults](../measurements/2026-10-25/front-capture-negative-controls.json),
[87 probes](../measurements/2026-10-25/icarus14-front-capture-probes.json),
[the source reference matrix](../measurements/2026-10-25/front-capture-reference-matrix.json),
[population](../measurements/2026-10-25/front-capture-population.json) and
[sv-tests](../measurements/2026-10-25/front-capture-svtests.json).
Recorded source hashes match `ebb0e14d`; runs took place before that commit.

Automatic event construction and owned event/ref/object providers are still
unsupported. Null chandle is tested; nonnull source chandle transport is not
confirmed here and needs an original permitted producer. Full source allocation
failure injection, complete SSA ref provenance and exhaustive descriptor
replacement/generation paths remain work. The frozen probe inventory leaves
21 historical Icarus-13-passing gaps and does not cover the entire language.
P1 is not complete.
