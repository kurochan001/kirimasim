# Proposal: end P1 at a closed list

[日本語](p1-closed-exit-proposal.ja.md). Status: **approved on 2026-11-07 and in effect. The rules are in [P1_SCOPE](../spec/P1_SCOPE.md#closed-exit-list-approved-2026-11-07) and [TEST_SUITE](../spec/TEST_SUITE.md); this note keeps the reasoning as it was proposed.** English is authoritative.

## The problem

P1_SCOPE aims at execution coverage at least as broad as Icarus Verilog 13.0, and says in the same place that exact coverage and complete parity remain unconfirmed. [P1 Support](../P1_SUPPORT.md#remaining-work-and-scope) and the probe records repeat that a finite list does not establish the target. Both statements are correct, and together they mean that no measurement can ever say that P1 has ended.

This holds up the phases after it:

- The final SIR v1 grammar and kernel boundary freeze [after P1 completes](sir-v1-proposal.md).
- `src/analysis/` and `src/codegen/` hold a 10-line placeholder each; tier 2 starts in P3, after P2.

## Where P1 stands

Figures carry their dates; nothing here was remeasured for this note.

| Gate | Result | Date and source |
| --- | --- | --- |
| sv-tests, at least the pinned Icarus's count | Linux 252 against 211, macOS 252 against 210, of 344 | 2026-11-01, [validation counts](../VALIDATION_COUNTS.md) |
| Fixed population, 90% (1,320) | 1,456 of 1,466; 0 failures, 10 unsupported | [Integration record, 2026-11-06](main-integration-2026-11-06.md) |
| Mandatory test list | Passes in CI | CI of 2026-11-06 |
| `valsem` cross-check, 7 consecutive nightly runs | **Not confirmed for the current code.** The nightly workflow failed on six nights from 2026-10-29 to 2026-11-03; whether the cross-check was among the causes was not checked here | Nightly history |
| Human review record (M8) | Not done; it is the maintainer's | TEST_SUITE |

The 188 probes against Icarus 13.0 ([record](../measurements/2026-11-06/icarus13-vpi-argument-checkpoint.json), commit `7bcb52ce`):

| kirimasim | Icarus 13.0 | Probes | Meaning |
| --- | --- | --- | --- |
| passes | passes | 104 | Agreement |
| passes | fails to compile or run | 40 | kirimasim is ahead |
| passes | runs, other output | 25 | kirimasim gives the expected value; the expected values come from the standard or an approved contract |
| fails | fails too | 17 | Neither runs them; not a gap by the current criterion |
| fails | passes | 1 | `real_net`, the approved `wire real` exclusion |
| time limit | passes | 1 | `dense_udp_coverage`, a 33-input UDP; slang's exhaustive coverage check does not end |

So of 187 probes in scope, **one** is a gap, and kirimasim cannot close it while slang stays unmodified. No upstream report has been filed for it.

What keeps P1 open is the other row of P1 Support: "the Icarus-supported subset of" dynamic containers, classes, VPI, switch-level modeling, specify and so on. Which forms that subset holds is exactly what is not known, and finding out has no end.

## The proposal

**P1 ends when these four hold.** A form outside them no longer holds P1 open.

1. **The existing gates**, unchanged: sv-tests, the fixed population, the mandatory list, the `valsem` cross-check, and the M8 report with its human review records.
2. **A frozen probe list.** The 188 probes go in `tests/gates/p1_probes.txt` by name and source hash, as `tests/gates/p1_population.txt` does for the population. `tools/icarus_feature_probe.py --extended --require-parity` passes on that list against Icarus 13.0. Changing the list takes a person's approval. Its exclusions are `real_net` (approved) and `dense_udp_coverage`, by source hash (chosen 2026-11-07). The second takes more than a new row: TEST_SUITE and `independent_failure` in the tool say that an exclusion never excuses a time limit. The amendment must allow a front-end time limit for this one hash, and keep crashes and internal errors as failures.
3. **The named mechanisms, as implemented at the freeze.** Each passes its mechanism matrix and is described in P1 Support: the first VPI subset; scalar and state-dependent specify paths; classes under the approved contract; dynamic containers; ports and strengths; programs; FST and VCD. This asks for no new feature. Its purpose is to name what P1 delivers.
4. **Every probe where Icarus's output differs (25) has a stated ground** in the standard or an approved contract. P1_SCOPE already asks to document such differences and not to reproduce another simulator's defects. Whether all 25 are written down yet is **not confirmed**.

**What moves.** Forms in the "Required P1 execution gaps" row of P1 Support that the list does not name:

| Area | Moves to |
| --- | --- |
| Class forms and dynamic-container forms beyond the matrices | P4 |
| VPI beyond the first subset | P4 |
| `%p` handle and storage forms, `$fread` shapes | P4 |
| Switch-level modeling beyond the matrices | P5 |
| Remaining specify paths, pulse controls and timing checks | P5 (chosen 2026-11-07; the phase table names neither) |

**What does not move.** A wrong result is a bug in any phase. IEEE value and scheduling semantics, four-state defaults, strict slang diagnostics and existing expected values stay as they are. The comparison tool keeps running as information; a form it finds later goes on the P4/P5 lists.

**What it costs.**

- kirimasim gives up the claim of being at least as broad as Icarus 13.0, and states what it covers instead. README, P1_SCOPE and TEST_SUITE say this differently today and must change together.
- A design that uses a form Icarus runs and the list leaves out is rejected until P4 or P5. It gets exit code 2 and a positioned "unsupported" diagnostic, not a wrong result.

**What it gains.** P1 can be judged by running tools, and P2 and P3 get a start.

## Alternatives

- **Keep the current target.** Honest about the aim, but with no end; P2 and P3 keep waiting.
- **One more inventory round, then freeze (not chosen).** A fixed time for VPI, specify, process control and class forms, then the freeze at whatever list results. It finds more of the gap and costs that time; the areas above were not fully mapped when the 188 were written. Reasonable if the maintainer wants a stronger claim. It is the same proposal with a later freeze date.

## If it is approved

In separate commits, each approved: the amendments to P1_SCOPE and TEST_SUITE (both languages); the gate file and its check; the `dense_udp_coverage` exclusion; the rewording of P1 Support and README; then the M8 report and the judgment. P2 then starts from [its proposal](sir-v1-proposal.md), whose open items (the Op inventory, exact grammar, API prototypes) still need their own review.

## The maintainer's choices (2026-11-07)

1. Freeze now, at 188.
2. Exclude `dense_udp_coverage` by source hash.
3. Specify's remaining forms go to P5.
4. README describes P1 as running the listed scope and comparing with Icarus 13.0 on 188 probes, instead of "at least as broad as Icarus 13.0".

These set the direction. The amendments listed above were shown and approved on 2026-11-07.
