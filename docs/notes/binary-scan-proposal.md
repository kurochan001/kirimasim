# Binary scan proposal

[日本語](binary-scan-proposal.ja.md). Status: **approved (2026-10-23)**.
Incorporated into [SYSTEM_TASKS 6.3](../spec/SYSTEM_TASKS.md#63-input) and
[SIR_SPEC 8](../spec/SIR_SPEC.md#8-subroutines).
Implementation now uses the shared kernel scanner. The matrix checks
40 programs / 17,120 conversions plus continuation and rejection cases.

P1's SYSTEM_TASKS.md 6.3 includes file input; SIR_SPEC.md 8 previously
explicitly rejected `%u` and `%z`. This approved amendment removes that
specification restriction. IEEE 1800-2023 21.3.4.3 and Table 21-7 describe binary transfer;
they do not explicitly settle every interaction with the general field
rules. The following choices were reviewed and approved before code.
The standard was checked locally; no text or examples are reproduced here.

## Approved amendment to SYSTEM_TASKS.md 6.3 and SIR_SPEC.md 8

- Accept `%u`, `%U`, `%z` and `%Z` in `$fscanf` and `$sscanf` for integral,
  enumerated and packed-aggregate destinations already represented by SIR.
  They are not accepted by `$value$plusargs`, whose conversion set stays
  that of 21.6. Unpacked aggregates remain unsupported.
- Without a field width, transfer the destination's bit width, rounded
  up to complete 32-bit words. With an explicit positive field width,
  treat it as the transfer's bit width; mask bits above it, then assign
  to the destination by ordinary zero extension/truncation and 4-to-2
  conversion. Width zero has the omitted-width meaning, as in the
  existing scanner. These width choices are rules of this document,
  informed by the reference measurements rather than explicitly stated
  by Table 21-7.
- Suppressed `%*u` and `%*z` consume a field without taking a destination
  argument. Their omitted width is 32 bits; an explicit width determines
  their transfer size. This is a rule of this document.
- Words arrive least significant first, each in the host's native byte
  order. `%u` reads one 32-bit value per word; `%z` reads a value/mask pair
  in the VPI representation. This inverts the approved output encoding
  (SYSTEM_TASKS.md 3.11; 21.2.1.1, 21.3.4.3, 38.15).
- **Approved interpretation:** binary conversions do not skip
  leading whitespace or NUL. Such bytes are payload, including inside
  packed `$sscanf` input. Whitespace written in the format still performs
  its ordinary skip before the binary directive. This gives binary
  transfer precedence over the general leading-whitespace rule, which
  previously named only `%c` as an exception in SYSTEM_TASKS.md 6.3.
- Assign only a complete binary field. On a short field, keep that
  destination unchanged, consume the bytes actually available, mark EOF
  for a file, and stop scanning. Return the number of earlier assigned
  fields, or `-1` when no earlier directive converted. A successfully
  suppressed conversion counts as a conversion for this EOF decision
  but does not increase the assignment count. Partial-field rejection
  is a rule of this document, preserving whole-field assignment.
- Read only the field's bytes, without consuming the next byte or
  looking ahead to test EOF after a complete field. A literal or another
  directive continues at that next byte. Existing file pushback applies.
- Constant and runtime formats use the same encoding and conversion
  rules in the kernel. Front supplies destination widths and performs
  copy-back only for successfully assigned fields, as for other scans.
  No source rewriting, simulator-specific branch, or default language
  relaxation is introduced.
- Do not weaken existing expected simulation output. The existing unit
  check that `%u` is unsupported is replaced by positive parser, type,
  byte-consumption and value checks. Existing suite expectations remain
  unchanged. Add tests for boundaries, X/Z, suppressed fields, EOF at
  each byte, continuation, input homes and containers.

The new transfer-width and EOF rules above are intentionally identified
as project choices, not presented as settled IEEE wording. Both this
semantic amendment and the P3/P4 scope amendment were approved on 2026-10-23.

## Reference observations

Only original probes and installed executable behavior were examined.
The comparison compiler/runtime identify as
`14.0 (devel) (s20260301-411-gca6b8d376)`; exact 13.0 behavior is unconfirmed.

- A 33-bit destination consumes 8 bytes for `%u`, 16 for `%z`.
- `%4u` into 32- or 65-bit destinations consumes 4 bytes and keeps only
  the low four value bits. `%4z` consumes 8 bytes and keeps four value
  and mask bits. `%*u` consumes 4 bytes and `%*z` consumes 8 bytes.
- Leading `20`, `09`, `0a`, `0d`, and `00` bytes were retained as payload.
- With a 32-bit destination, zero through three available bytes for `%u`,
  or zero through seven for `%z`, return `-1`, preserve an initial
  `deadbeef`, consume the available bytes and set EOF. A full field
  returns 1; EOF is not set until another read meets it.
- **Known difference from the approved width-zero rule:** Icarus `%0u`
  and `%0z` return `-1`, keep the destination and consume no payload,
  without setting EOF. The approved project rule deliberately retains
  omitted-width behavior for width zero, including this measured difference.

Reproduce the complete observations and the comparison with the approved
project rules (not a conformance assertion):

```sh
python3 tools/binary_scan_probe.py \
  --kirimasim build/dev/src/driver/kirimasim \
  --output build/binary-scan-observations.json
```

The 448 original cases cover six destination widths, both binary codes,
omitted/zero/explicit widths, raw NUL/whitespace, explicit whitespace in
the format, suppression and every byte of selected short fields. A
complete run produced 448 tagged observations: 374 matched the candidate,
and all 74 differences were width-zero directives. Subsequent grouped
runs also crashed before producing traces: SIGTRAP with the installed
`s20260301-411-gca6b8d376` runtime and SIGSEGV with the existing `f190dbb`
runtime. The cause is unconfirmed; no Icarus source was read to investigate.
The tool records native failures and incomplete comparison explicitly.

The nonzero/omitted-width subset was then measured separately:

```sh
python3 tools/binary_scan_probe.py \
  --kirimasim build/dev/src/driver/kirimasim --exclude-zero-width \
  --output build/binary-scan-nonzero-observations.json
```

All 374 selected cases produced complete observations with no differences
from the candidate in that run; the report records the 74 excluded cases.
Before implementation, kirimasim rejected the binary format and produced zero
observations. The implemented scanner now matches all 448 project-rule cases;
the nonzero-width comparison gives 374 observations with no differences for
both kirimasim and the reference tool. No failure or warning line is mistaken for another case's
trace, and zero observations are never reported as a passing comparison.

The [official standard page](https://standards.ieee.org/ieee/1800/7743/)
identifies the edition used. Reference behavior is evidence about the
reference tool, not authority for a standards interpretation.
