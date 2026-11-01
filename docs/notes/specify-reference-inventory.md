# Specify reference inventory

[日本語](specify-reference-inventory.ja.md). 2026-11-01.

This finite inventory prepares the [first module-path contract](specify-p1-proposal.md).
It does not implement timing or change P1 criteria. The 176-probe inventory,
its two required-scope gaps, all existing tests and counts remain unchanged.
The existing `specify_path` source is reused byte-for-byte (SHA-256
`e31603a4f262a52b5c942a3bfef807f20b15aa20a5f98ed4abf4517712e21669`).

## Measured scope

The [machine record](../measurements/2026-11-01/specify-reference-inventory.json)
contains 102 original sources, written draft oracles where applicable, hashes,
compiler flags, observations and versions. Explicit installed compiler/runtime
binaries both identify Icarus Verilog 13.0 stable `v13_0`. `-gspecify` is always
passed; omitting it would measure parsing with timing disabled. No reference
implementation source or generated simulation bytecode was read.

There are **87 matching written traces, six reference differences and nine
observation-only cases**. The unchanged native binary identifies `80859f34f0aa`:
all 102 sources parse, then compile-and-run rejects the unsupported specify
block with exit 2. Native timing execution and native passes are both zero.
These 102 attempts are not added to the fixed P1 population or gate denominator.

The inputs cover all twelve distinct four-state transitions with 1/2/3/6/12
delay expressions, the existing frozen probe, min/typ/max, scalar full paths,
latest-input and simultaneous-input choice, mixed distributed/path delay,
positive/negative edges, polarity/data annotations, default pulses, zero/negative
and real delay normalization, state-dependent conditions, vectors and selected
pulse/force/visibility/timing-check observations.

## Differences and limits

The six reference differences are retained, without calling them native faults:

- `vector_parallel`, `vector_full`: the runtime explicitly diagnoses incomplete
  multibit path support and use of the LSB delay for all bits. Observed output
  misses the independently timed intermediate vector value.
- `negative_path_delay`: instead of the draft's zero-clamped transition, the
  observed event is at tick 4,294,967,293 after the test's end sample.
- `condition_1_bx`, `condition_1_bz`, `condition_2_b1x`: the observed path uses the
  fallback delay. The reviewed 2017 rule enables an X/Z condition LSB.

The latter rules and delay expansion were checked against primary IEEE
1800-2017 text. Direct confirmation against 2023 remains unconfirmed because
the official document endpoint was unavailable. These are differences from the
explicitly recorded draft/2017 oracle, not a claim that all are proven 2023
reference defects. The [proposal's edition note](specify-p1-proposal.md#9-evidence-and-edition-limitation)
identifies the source and how to complete the check.

The nine observation-only cases have no portable stdout oracle and are not
passes. Four explicit pulse-style/cancellation declarations and `$setup` receive
unsupported timing-check diagnostics. A deliberate setup-window violation leaves
the notifier at zero; compilation alone does not prove timing checks execute.
The PATHPULSE stimulus distinguishes a short filtered default pulse from one
that would pass a reduced threshold; the raw observation is retained without
claiming pulse-control support.

Force/release across a pending path deadline observes `forced 0`, `released 1`.
Internal and external reads observe 0 before the path deadline and 1 afterward.
Those are evidence for the proposed integration choices, not permission to
copy arbitrary reference ordering into a standard oracle.

## Oracle correction and checker validation

The initial uncommitted table draft incorrectly used ordinary gate turn-off
selection for two expressions, and minimum selection for transitions out of X.
Six initial differences were draft-oracle errors. Inspection of 2017 chapter 30
corrected them before this record: 0-to-Z uses R, 1-to-Z uses F, and omitted
transitions out of X use the latest possible delay. All 60 transition cases,
the frozen probe and three min/typ/max cases then match. Sources were preserved;
these were not migrations of existing repository test expectations.

[Six external checker controls](../measurements/2026-11-01/specify-checker-controls.json)
change one original source in an ignored copy, compile and execute it, and
require the written oracle to reject it. They change a path deadline, two-delay
turn-off, X-to-1, distinct-source delay, distributed delay and pulse width.
Every mutant compiles/runs and is caught; the original six pass before and
after, and the runner hash is unchanged. These are reference-checker controls,
not native simulation mutations, and do not prove the future native engine.

## Reproduction and exit status

Run the tool with explicit executables and an ignored output destination:

```sh
python3 tools/specify_feature_probe.py \
  --iverilog <icarus-13-compiler> --vvp <icarus-13-runtime> \
  --kirimasim build/dev/src/driver/kirimasim \
  --export build/specify-inputs --output build/specify-inventory.json
```

`--case` selects original cases; `--source-dir` uses explicit source copies
while retaining canonical written oracles, making the controls reproducible.
Default exit 0 means the inventory was recorded, not that all cases passed.
Independent execution failures still produce exit 1. Add
`--fail-on-reference-mismatch` when checking an oracle or fault-injected input;
that mode returns 1 for unmatched reference cases, including this full inventory's
six retained differences. No result is rewritten to PASS or skipped.

Production source, build dependencies, CI registration, existing expectations
and measured gate counts are unchanged. A native contract must be approved
before runtime implementation; its existing rejection migration is a later,
separate review.
