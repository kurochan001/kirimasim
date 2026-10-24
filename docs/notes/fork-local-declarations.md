# Fork-local declarations and parent initialization

[日本語](fork-local-declarations.ja.md). Implemented at `c6d9c3d`, verified
2026-10-24. Full P1 and Icarus coverage remain unfinished.

## Implemented behavior

Fork block declarations are lowered in their parent's execution scope, before
the executable children are created. They do not become children or satisfy
`join_any`. Static variables retain their existing shared slots and initialize
once before time zero; automatic declarations initialize on each entry in
the parent's frame. A declaration-only fork creates no child and needs no
wait, for all three join kinds. Named-fork initializers retain their lexical
scope, including `%m`.

This uses the existing SIR_SPEC 3.3/7.4/7.5 and SCHEDULER_SPEC lifetime/fork
rules under the approved P1 scope. No kernel API, SIR instruction, dependency
or new scheduling policy is added. Initializers can use existing expressions,
calls and typed storage; they do not get an independent value or copy rule.
Existing slang warnings for implicit static initialization remain checked.

```systemverilog
module top;
  initial fork
    int value;
    begin #1; value = 9; end
    begin #2; $display("%0d", value); end
  join
endmodule
```

Output: `9`. Both children use the same static variable. The declaration is
not an extra process that can complete before the two timed statements.

**Automatic capture is still unsupported.** A fork child cannot yet read or
write an automatic variable of its parallel block or enclosing activation.
Shared identity, parent-frame retention and escaping lifetimes need the
detailed contract still missing from SIR_SPEC 7.5. This change does not copy
such variables into separate children and call that equivalent. Existing
automatic event/aggregate declaration restrictions also remain where those
types' frame representations are unimplemented.

## Original execution evidence

The [generator](../../tools/gen_fork_declaration_matrix.py) adds **72 original
source cases**. Sixty-four cross eight homes, four widths (1/32/65/129) and
two-/four-state types. Each checks shared static storage, repeated entry,
initialization once, all three joins and exact completion times. Eight more
cover ordered/concurrent automatic initialization without capture, empty
child lists, lexical scope, short-circuit control flow, typed arrays/reals/
strings, queues/events and an initializer calling a function that starts work.

[Source/output hashes and four-engine results](../measurements/2026-10-24/fork-declaration-source-validation.json)
record all 72 passing on the current native binary and all 72 unsupported
on the retained previous binary (`e4376f1d029d`, native source `57a64ef`).
Both Icarus 13.0 stable and 14.0 development match 58 sources, reject nine
at compilation and differ on five at runtime/output. Eight compile rejections
are explicit lifetime modifiers; the other is the existing function/fork
scheduling mechanism. The five runtime/output differences remain observations,
not replacement expectations; they include two signal-6 terminations.
No other simulator source or tests were read.

[Four compiled negative controls](../measurements/2026-10-24/fork-declaration-negative-controls.json)
are detected by the new source tests. Declaration-as-child, omitted automatic
initialization and repeated static-initializer effects each fail an actual
new stdout expectation. A fourth emits an empty fork and is caught by the
real source's SIR verification failure. Anchor/judging self-checks are not
evidence, and source is restored and rebuilt after the controls.

An independent CLI boundary test checks three positioned automatic-capture
rejections and three uncaptured-parent-initialization acceptances, across all
join kinds. Full local CI passes all four configure/build/test configurations
and static checks; the [current progress](p1-icarus-progress.md) has the counts.
All 29 existing SIR dump expectations and old suite/performance expectations
remain unchanged. Existing slang warning lines are preserved in new fixtures.

## Review boundary and finite comparison

The historical CLI input with six declaration-only contexts is byte-identical.
Its obsolete rejection expectations are retired separately at `a431909` on
`codex/fork-local-declaration-expectations`, with 6.21/9.3.2 as the reason.
Implementation review is based on that expectation branch; this is a local
review stack, not a claim that separate remote PRs have been created.

The [comparison inventory](icarus-13-comparison.md) now has 81 original probes.
On that same inventory the previous native binary has 22/28 Icarus-passing
gaps (13.0/development), and the current one has 21/27. Static fork declarations
now pass. The newly inventoried automatic shared-capture case still fails
natively and passes both references. All earlier 79 statuses and their four/
seven reference-mismatch lists are unchanged. The earlier 20/26 gap counts
belong to that smaller inventory; this is no full-coverage claim. Performance
and Linux results have not been remeasured.

```sh
python3 tools/gen_fork_declaration_matrix.py --check
ctest --preset dev -R 'fork_declaration|gen_unsupported_matrix' --output-on-failure
python3 tools/mutants.py --only 'fork declaration:'
python3 tools/check_expected_change.py --base codex/fork-local-declaration-expectations --head HEAD
```
