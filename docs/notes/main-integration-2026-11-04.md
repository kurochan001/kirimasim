# Published main integration and documentation checkpoint

[日本語](main-integration-2026-11-04.ja.md) · 2026-11-04.

Published main `9113955914d878437074f0e3b86a8dca38438080` integrates 25 commits
after `d73d8922`. Full local CI and the matching GitHub CI workflow passed.
P1 remains in progress: finite examples do not establish the complete
[Icarus-required coverage target](../spec/P1_SCOPE.md). This record updates
documentation, without changing implementation, approved contracts or expectations.

## Implemented subsets and remaining boundaries

| Area | Integrated behavior | Still outside this subset |
| --- | --- | --- |
| [State-dependent specify](specify-state-execution.md) | Simple scalar `if`/`ifnone`, condition capture at functional arrival, existing physical-driver timing | Vector/conditional edge-sensitive/passive paths, explicit pulse controls, SDF and timing checks |
| [VPI whole-net writes](vpi-net-write-execution.md) | Immediate deposits on point nets; force/release on eligible registered nets, shared HDL ownership and callback reentry | Selected/delayed net writes, passive deposits, additional APIs and cocotb integration |
| [Explicitly clocked class bodies](class-clocked-execution.md) | Supported property operations, calls and allocation under ordinary signal triggers | Object-dependent trigger/guard/watch evaluation, implicit object sensitivity and existing property target restrictions |
| [Native FST](fst-execution.md) and [strength formatting](strength-format-execution.md) | Binary waveform output and captured-strength `%v` through existing shared state | Unlisted source/object forms; viewer interoperability and waveform performance remain unconfirmed |

The first three rows are the newly integrated implementation groups. FST and
strength formatting were already implemented; older guide statements calling
them unsupported have been corrected. Exact type, lifetime and selection limits
remain in [P1 Support](../P1_SUPPORT.md). Existing expectation migrations retain
their separate commits and approval records.

## Validation of the published implementation

[Machine-readable metadata](../measurements/2026-11-04/main-integration-validation.json)
records the source revision, local CI steps, tool versions, captured GitHub result
and documentation command checks.

- The default `python3 tools/local_ci.py` passed **all 21 steps** on `91139559`.
  Dev, ASan and UBSan each passed **12,012/12,012** CTests; release passed
  **12,013/12,013**, with no failures or skips. `cli/slot_event_default` is
  release-only.
- Release flags, source policies, fixture pairing, generated counts, formatting,
  clang-tidy on **194 translation units**, and expectation checks for all
  **25** integrated commits passed. Private untracked files caused the log's
  dirty-worktree annotation; tracked files were clean and stayed unchanged
  during the run.
- [GitHub CI run 41725997482](https://github.com/kurochan001/kirimasim/actions/runs/41725997482)
  reports `completed` / `success` for the same full commit. The result was checked
  on 2026-11-04; its last update is 2026-11-03 23:21:34 UTC. This is a workflow
  conclusion, not a claim that a new Nightly or every optional job passed.

This closes the earlier class note's distinction between its full-suite
checkpoint and final fixture follow-up: both are included in the published
commit's full local CI. Earlier raw records retain their original revisions.

## Feature probes and older external measurements

The [180-probe record](../measurements/2026-11-04/icarus13-class-clocked-after.json)
has **179 P1-evaluated cases and one excluded case**, `real_net`, whose source
depends on Icarus's optional `wire real` extension. Its rejection remains
unsupported, not PASS. Standard real variables, arithmetic, legal single
continuous assignments and `nettype real` retain their existing P1 scope.

The required-scope gap is **`dense_udp_coverage`**. It remains an independent
frontend time/output-limit failure; it is neither excused nor counted as normal
rejection. The raw passing-reference gaps are two, including `real_net`.
The 25 reference mismatches stay separate. The previous 176 inputs/oracles and
native outcomes are unchanged; three of the four added class-clocked probes
close new gaps and the fourth is a waiting-method control.

No external population was rerun for this documentation update. Retain the dates
in [recorded counts](../VALIDATION_COUNTS.md): the fixed population is
1,456/1,466 passing, zero failures and ten unsupported (2026-10-27); sv-tests is
252/344 on each OS versus Icarus 211 on Linux and 210 on macOS (2026-11-01);
the seven-night reference-fuzz record runs through 2026-10-27. Performance and
other historical observations also keep their measured versions and dates.

## Documentation examples

Eighteen smoke checks used `kirimasim 0.0.0 (commit 9113955914d8)`: the default
and width-8 counter, bit-query parse/SIR/run, the old object example's three
stages, property-ref parse/rejection, X-condition error, zero-time limit,
VCD/FST output and FST decoding, memory loading, strength force/release output,
and bit-query execution with statistics and waveform controls.
Expected errors and limits retain their nonzero exit codes.

The old `unsupported_object.sv` now executes successfully and is retained
unchanged. [Debugging](../DEBUGGING.md) instead demonstrates a current lowering
boundary with [a new property-ref example](../examples/debugging/unsupported_property_ref.sv):
parse succeeds, then SIR lowering returns exit 2 with a positioned unsupported
diagnostic. Documentation examples do not add suite tests or change CTest counts.

For this documentation refresh, all **201 Tools CTests** pass. The **442 public
Markdown files (221 language pairs)** have reciprocal language links and no
broken local links; changed tables and Mermaid structures agree between
translations. Generated README counts, SPDX and whitespace checks pass.
`counts.json`, earlier raw measurements and the five private untracked files
are unchanged. Full local CI above is the published implementation's earlier
run, rather than a second full run for these prose changes.

Use [Validation](../VALIDATION.md) for reproduction recipes and
[status history](../STATUS_HISTORY.md#guide-checkpoints-before-2026-11-04) for
superseded guide checkpoints. Slang posting remains on hold.
