# Main publication of automatic events and call transfers

[日本語](automatic-event-main-integration.ja.md). Status: **published on main,
2026-10-26**, implementation checkpoint `304e1aec`.
The remote main ref was checked against the full commit ID in the
[publication snapshot](../measurements/2026-10-26/automatic-event-main-publication.json).

## Published scope

Main fast-forwarded from `1c970f5d` through six existing commits, preserving
the implementation and approved-expectation histories separately:

- `1d2b7e6e` / `ee71fdd0`: event argument/copyback snapshots and return pins,
  with the component evidence and limitations.
- `e909f022` / `954b5350`: native/SSA/transfer root publication, collection
  before physical event-row growth, automatic scalar/fixed-array declaration
  defaults and the original 176-source/32-SIR-design evidence.
- `4ba6b63d`: the separately approved one-case historical rejection migration.
  Every source input and four strict ref rejection boundaries are preserved.
- `304e1aec`: successful post-approval CI, generated counts and bilingual
  validation/status documentation.

[Current execution and limits](instruction-event-execution.md) describe the
mechanism. Automatic event value formals, initializer aliases, automatic
structure event fields and broader event/object interfaces remain partial.
The implementation adds no dependency, SIR version or C function-table ABI
version and changes no performance budget.

## Validation and publication status

[Post-approval local CI](../measurements/2026-10-26/automatic-event-approved-validation.json)
passes **21/21** steps on `4ba6b63d`. The published `304e1aec` changes only
documentation after that tested checkpoint; runtime and test bytes match.

- dev/ASan/UBSan: each 5,741 passed out of 5,743.
- release: 5,742 passed out of 5,744.
- Every preset has zero failures and two known skips:
  `suite/cb/input_sample_1step` and `suite/nets/net_port_drivers_part`.
- Full format/tidy, policy, expected-change and generated-count checks pass.

The [hosted CI run](https://github.com/kurochan001/kirimasim/actions/runs/40319556353) for
`304e1aec` was **completed / success** at **2026-10-26 12:52:13 UTC**:
**all 14 jobs succeeded**. They include Linux dev/release, GCC ASan/UBSan,
Linux TSan, minimum compilers, lint and all four macOS presets. This is a timestamped observation; use the run link for
later results. Regular CI does not establish seven nightly fuzz passes or
refresh the external comparison and speed measurements.

## Reading older records

Earlier notes and measurement JSON retain their original local/unmerged,
unapproved and failing states. They are checkpoint evidence, not current
approval requests. The old tran rejection migration was separately applied
in `77c9d188`; the automatic-event metadata rejection was resolved in
`4ba6b63d`. This publication supersedes earlier descriptions of automatic
scalar/fixed-array construction or instruction collection as disabled.

The [87-probe comparison](../measurements/2026-10-26/instruction-events-comparison.json)
still has seven Icarus-passing gaps against each reference and four/seven
reference mismatches. The fixed population remains 1,456/1,466 with zero
failures and ten unsupported; native sv-tests remains 244/344. These are the
previous measurements of the unchanged execution implementation, not new
measurements made by this documentation update.

P1 remains incomplete. [P1_SUPPORT](../P1_SUPPORT.md) lists implemented forms
and remaining mechanisms; finite probes and gate percentages do not prove
complete Icarus Verilog 13.0 coverage.
