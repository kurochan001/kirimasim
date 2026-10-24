# Icarus 13.0 and development-version comparison

[日本語](icarus-13-comparison.ja.md). Measured 2026-10-24 at kirimasim
source `c6d9c3d` (fork declaration lowering), dev build on macOS/arm64. No installation was added and
no other simulator source or tests were read.

## Verified versions and results

Existing binaries report `13.0 (stable) (v13_0)` for the compiler driver,
preprocessor, parser/elaborator and runtime. The additional baseline's
compiler/runtime report `14.0 (devel) (s20260301-411-gca6b8d376)`.
Both ran the same 82 original probes from
[`icarus_feature_probe.py`](../../tools/icarus_feature_probe.py):

- **13.0 stable:** 22 Icarus-passing gaps and four reference result
  mismatches. [Portable per-probe results](../measurements/2026-10-24/icarus13-automatic-capture-probes.json).
- **14.0 devel:** 28 Icarus-passing gaps and seven reference result
  mismatches. [Portable per-probe results](../measurements/2026-10-24/icarus14-automatic-capture-probes.json).

The six additional development-version gaps are array sum, queue sorting
and the unique/empty min/max probes. Version 13.0 does not compile those
forms. Its duplicate/with-key extrema cases also do not run, so they do not
produce the three extra reference-result mismatches observed in development.
The different totals are different reference baselines, not a reduction in
kirimasim's missing functionality. The broader cases and their pending
semantic amendments remain tracked.

The common four reference mismatches are packed parent-bound selection,
virtual dispatch, dynamic-array queries and one-bit X/Z repeat-event counts.
A reference mismatch can coexist with missing kirimasim support: class
virtual dispatch is still unsupported. The JSON records each engine's
status independently; these mechanisms must not disappear from P1 work.

The inventory has 79 standard-mechanism probes, two `project_policy`
RHS-order probes and one `icarus_extension` (`wire real`). IEEE permits
either side's evaluation first in the policy probes; their oracle tests
the existing approved kirimasim order. Extensions and project choices are
identified rather than presented as universal IEEE requirements.

## Automatic events added to the inventory

`automatic_event_capture` passes both references and reports a positioned
unsupported automatic-event diagnostic in native execution. The earlier 81
statuses and passing judged outputs are unchanged; no production code changed
for this measurement. Adding this mechanism increases gaps from 21/27 to
22/28 while leaving the four/seven reference-mismatch lists unchanged.

The [shared automatic-storage and event proposal](automatic-capture-proposal.md)
records 32 original design programs separately. It remains pending review;
its proposed outputs do not authorize new oracles or implemented support.
The earlier 81-item [13.0](../measurements/2026-10-24/icarus13-fork-declaration-probes.json)
and [development](../measurements/2026-10-24/icarus14-fork-declaration-probes.json)
records remain unchanged, including their previous-native comparisons.

## Fork declarations and the newly measured capture gap

At the fork-declaration step, the same 81 probes give 22/28 gaps on the retained previous native binary and
21/27 on the current one. `fork_local_declarations` now passes both references
and native execution. `automatic_fork_capture` verifies shared mutation of an
automatic fork variable; it passes both references and remains unsupported
natively. Its required shared-frame/lifetime contract is now proposed for review.

All previous 79 statuses and passing judged outputs are unchanged. Their
20/26 gap counts remain valid for that smaller cohort; the additional capture
case increases the expanded list's remaining count by one. The four/seven
reference-mismatch lists are unchanged. [Seventy-two source cases and compiled controls](fork-local-declarations.md)
provide broader mechanism evidence, without establishing complete fork support.
The portable records include the previous native results and the current
probe-tool hash. Historical 79-item records at `57a64ef` remain available for
[13.0](../measurements/2026-10-24/icarus13-wide-gate-probes.json) and
[development](../measurements/2026-10-24/icarus14-wide-gate-probes.json).

## Wide-mask result on the same inventory

At the earlier wide-mask step (`57a64ef`), on 79 probes and the same reference versions, the retained old native binary
had 21 gaps against 13.0 and 27 against development. That step had
20 and 26: `wide_tristate_masks` now passes both engines. All other 78 engine
statuses and reference-mismatch lists are unchanged. The portable records
include each previous native result and its version; the
[73-source matrix](wide-gate-strength.md) also matches both references.
This closes a newly inventoried mechanism, not an earlier counted gap or
the full P1 target.

The earlier [13.0](../measurements/2026-10-24/icarus13-blocking-aggregate-probes.json)
and [development](../measurements/2026-10-24/icarus14-blocking-aggregate-probes.json)
78-probe records at `396131e` are retained without modification.

## Evidence limits

The previous 75-probe records at `917c715` remain available for
[13.0](../measurements/2026-10-24/icarus13-feature-probes.json) and
[development](../measurements/2026-10-24/icarus14-feature-probes.json).
All their engine statuses and gap/mismatch lists are unchanged on the current
source. The three added standard probes cover whole-array blocking delay,
repeated-event capture and differing-bound structure-array copies. Kirimasim
passes all three; both references reject them at compilation. See
[implementation and expectation review](blocking-aggregate-timing.md).

This is a finite local inventory. It does not establish full 13.0 coverage,
all interactions, Linux results or the full extension/external-interface
inventory. Classes, VPI, switches, specify/timing and other required work
remain. The P1 target and existing gates are unchanged; zero passing gaps
in this list would still not prove completion.

The portable JSON preserves versions, categories, expected outputs,
per-engine results, gaps and disagreements. It keeps the first diagnostic
line and normalizes nonportable backend paths in failing stdout; passed
stdout is unchanged. The original tool outputs, including full diagnostics,
can be regenerated locally. Historical external-suite counts and performance
measurements retain their previous dates and revisions.

## Reproduce

Point `IVERILOG_13` and `VVP_13` at matching existing 13.0 binaries and check
their `-V` output. `iverilog -V` reports its helper components too.

```sh
python3 tools/icarus_feature_probe.py --kirimasim build/dev/src/driver/kirimasim \
  --iverilog "$IVERILOG_13" --vvp "$VVP_13" --extended \
  --output build/icarus13-automatic-capture-probes.json --require-parity
python3 tools/icarus_feature_probe.py --kirimasim build/dev/src/driver/kirimasim \
  --extended --output build/icarus14-automatic-capture-probes.json --require-parity
```

The second command uses PATH, so verify its compiler/runtime versions rather
than assuming a development build. Both measured comparisons exit 1 for
remaining gaps; the test-suite CI is a separate check.
