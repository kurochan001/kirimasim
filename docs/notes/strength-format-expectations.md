# Strength formatting: existing expectation migration

[日本語](strength-format-expectations.ja.md). 2026-11-01. **Approved by the maintainer on 2026-11-01.**

The implementation follows the MOS/switch contract approved on 2026-10-25,
sections 5 (registered-net force/release) and 8 (complete strength formatting).
IEEE 1800-2023 references are 10.6.2 and 21.2.1.4. This does not request new
semantics. It requests the separate review of existing expectations required
by AGENTS.md; the approved contract explicitly keeps that review separate.

The [concrete two-file patch](strength-format-expectation-migration.patch)
preserves every source input and changes these five expectations:

- `tests/tools/check_source_mos_boundaries.py`: the original direct and alias
  MOS-net force inputs now require successful execution with empty stdout.
  The original undriven nmos `%v` input now requires exactly `HiZ\n`.
  The accepted tran input and both positioned frontend rejection checks stay.
  No unsupported result is recategorized as success without running it.
- `StrengthNetworkSir.MalformedStaticRowsAndForeignOwnersAreRejectedBeforeVmConstruction`:
  mode 14 (inactive force-target flag) now requires verified, quiescent execution,
  one physical slot, `[6,6]` readout and no active force. All other 26 malformed
  variants retain their original rejection, empty store and unconfigured bank.
- `StrengthNetworkSir.ImplicitOriginsStayFixedAndForceThroughAliasesIsRejected`:
  after the unchanged alias input gains its inactive force flag, require the
  same 27 exact pull/supply intervals, no force on any node, one physical slot,
  zero created processes and a quiescent result with no error.

The test bodies are not reduced and no file is removed or renamed. New tests
also exercise actual overrides, overlap, release and strength formatting.
The approved migration is applied in a separate implementation-free commit;
counts are updated only after an exclusion-free successful regression run.
