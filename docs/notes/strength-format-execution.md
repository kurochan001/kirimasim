# Captured strength formatting and registered-net overrides

[日本語](strength-format-execution.ja.md). 2026-11-01.

This implementation connects sections 5, 7 and 8 of the
[MOS/switch contract](mos-switch-proposal.md), approved 2026-10-25.
It does not establish complete P1 parity. The existing
[expectation migration](strength-format-expectations.md) has a separate review.

## Supported behavior

`%v` consumes the complete captured signed interval for an eligible four-state
integral net reference, including packed selections, fixed-array elements and
physical aliases. Non-net integral expressions use Strong fallback. The
[formal formatting rules](../spec/SYSTEM_TASKS.md#310-v) define point, known
multi-rank, L/H, X and HiZ tokens. Vectors join tokens most significant first
with `_`; widths and alignment apply to the whole string.

Literal and runtime string/packed formats, display/write variants, string
formatting, file output, strobe and monitor use the same renderer. A runtime
format retains provenance for every net-reference actual, even if that format
currently selects `%b` or `%p`. Raw values, signedness and pattern layouts stay
separate; `%b`/`%p` do not use the metadata as a substitute value. Selectors are
evaluated once, then value and interval are captured together before the next
argument. Explicit casts and computed expressions use the ordinary value path.

Pinned slang rejects literal `%0v` and other literal `%v` field widths before
lowering. Runtime formats support the approved whole-field widths. This frontend
restriction is preserved; it is not a native success or a skipped test. Input
scan `%v`, unsupported net domains and unrelated formatting limits are unchanged.

Force/release on a registered strength net now bridges existing per-bit
procedural owners to the native node-level force API. Force has Strong rank 6;
local roots are suppressed, remote roots can transit, and forced Z clamps only
that node's own readout. Release synchronously reads current underlying roots.
Overlapping owners, aliases and partial targets use the existing override
selection rules. This does not widen the existing port identity/collapse
admission rules or the VPI interface.

## Representation and lifetime

`StrengthLoad` captures the interval from the same typed selected place as its
ordinary load; invalid selections retain Strong X padding. Strength SSA values
remain forbidden in frames and ordinary slots. The private typed
`$strength.format` call returns an owned String. Runtime arguments with net
provenance use `$sformatf.runtime.strength` and five fields: pattern layout,
raw value/empty marker, named/positional strings, strength/empty marker. The
older four-field private call remains supported and is emitted for arguments
without net provenance. The verifier checks shape and matching widths before
VM construction. No SIR opcode or public C ABI changes.

Postponed monitor evaluation records exact selected strength reads and replaces
subscriptions only after capture. Interval-only updates mark the corresponding
monitor due; they create no ordinary signal event. Replacement, detachment and
file closure release owned subscriptions before callback frames disappear.
Strength metadata is Context-owned and does not introduce name lookup or GC roots.

## Reproducible checks

- `tools/gen_strength_format_matrix.py --output build/strength-format-inputs`
  exports original source programs and exact text oracles for review.
- `ctest --preset dev -R '^(StrengthFormat|StrengthDisplay|StrengthNetworkVm.InactiveForce|cli/strength_format)' --output-on-failure`
  checks formatting, source contexts/APIs, typed metadata and lifetimes.
- `tests/unit/strength_format_test.cpp` contains a literal 120-interval table,
  whole-field checks and 1/8/9/65/129-bit packed boundaries.
- `tests/unit/strength_display_test.cpp` checks selected subscriptions,
  Postponed execution, replacement/detachment, per-bit ownership and synchronous
  release. Existing physical-force failure/rollback tests continue to apply.

[Final validation](strength-format-validation.md) records 11,364 passing dev tests,
63 related tests in each other profile and eight caught compiled faults. External
reference equivalence for the complete new matrix is unconfirmed. The native
expectations follow the approved project contract and do not infer semantics
from simulator implementation sources.
