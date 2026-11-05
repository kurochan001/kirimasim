# Unsigned VPI packed-bit views

[日本語](vpi-packed-bit-values.ja.md) · 2026-11-05.

A selected packed bit has its own unsigned view even when the parent is signed; the [VPI
bit-selection FAQ](../FAQ.md#does-a-vpi-bit-selection-inherit-its-parents-sign) explains
the user-visible result.

Implementation `64b31665` fixes an Icarus-13.0-supported P1 value boundary:
`vpi_handle_by_index` previously copied the signedness of a packed parent into
its selected bit. A one bit read from a signed vector therefore returned −1
through `vpiIntVal`, `vpiRealVal` and `vpiDecStrVal`. It now returns 1; the
parent retains its declared signedness and value.

This follows the existing unsigned-selection rule in
[VALUE_SEMANTICS 2](../spec/VALUE_SEMANTICS.md#2-types-and-widths-of-expressions)
(IEEE 1800-2023 11.8.1) and the approved
[VPI object/value contract](vpi-p1-proposal.md#3-functions-objects-and-values).
No specification amendment or existing expectation migration is needed.

## Scope and checks

The fix normalizes the type of the selected view before existing metadata,
conversion and callback code uses it. It preserves the parent declaration,
physical binding, declared index, neighboring bits and all existing write
capabilities. It adds no ABI, dependency, scheduling or force policy.

The original [generator](../../tools/gen_vpi_packed_bit_matrix.py) supplies
**432 source inputs**: module/child/generate/interface/program/static-task
contexts, `logic`/`bit`/`wire`, signed/unsigned parents, ascending/descending
ranges including negative indices, and widths 1/32/33/65/129/257. Each checks
argument and name handles, boundary/middle indices, numeric and vector reads,
the approved automatic scalar format, parent preservation, selected changes,
unchanged neighbors and callback payloads across 0/1/X/Z where applicable.

The [matrix record](../measurements/2026-11-05/vpi-packed-bit-matrix.json)
keeps source hashes, written oracles and both native outcomes. Identical
inputs improve from **216/432 to 432/432**. Of the 216 corrected cases,
144 also match the reference's numeric-read companion. The other 72 corrected
cases use two-state parents whose bit handles the reference does not supply.

Three new unit tests cover conversions without modifying parents, packed
memory words with both range directions and retained element/index bindings,
and unsigned callback values with distinct X/Z transitions. One compiled
omission control restores the old signedness behavior; all four new CTest
entries detect it. The original source SHA and all four successful baseline
tests are restored. [Validation metadata](../measurements/2026-11-05/vpi-packed-bit-validation.json)
records the compiled control and build/test results.

Unfiltered dev passes **12,144/12,144**, with no failures or skips. Release,
ASan and UBSan each pass all **52 related VPI CTests**. A final test-only
offset-type cleanup is rechecked with the three new unit tests in all four
profiles; clang-tidy reports no diagnostics in the two affected translation
units. Full local CI remains required before a later authorized push.

## Reference limits and remaining P1 work

Explicitly selected Icarus compiler/runtime **13.0 stable (`v13_0`)** match
numeric reads in **288/432** companion cases. The remaining 144 two-state
cases return null bit handles and remain non-passing reference observations.
The companion omits callback and `vpiObjTypeVal` assertions: Icarus rejects
these bit callbacks and chooses integer for a bit's automatic output, whereas
the already approved host policy chooses scalar. The stronger native tests
retain both checks. These reference limits create no P1 exclusion or native
PASS substitution.

The earlier 184-probe inventory and its one `wire real` exception retain
their scope and date; this VPI matrix is a separate mechanism inventory.
P1 is unfinished. A separate
[two-case follow-up](../measurements/2026-11-05/container-implicit-followup.json)
confirms Icarus-passing queue-size implicit sensitivity in `always_comb` and
`always @*`, both still rejected natively. These are further required-scope
gaps outside those 184 probes, not optional extensions. Object-dependent
observation, further specify and VPI forms also remain work. Upstream slang
posting is still on hold; private drafts are unchanged.

```sh
python3 tests/cli/run_vpi_packed_bit_tests.py --kirimasim build/dev/src/driver/kirimasim
ctest --preset dev -R '^(Vpi.Packed|cli/vpi_packed_bit$)' --output-on-failure
python3 tools/mutants.py --check
```

For the numeric-read companion, also pass the explicitly selected Icarus 13.0
`--iverilog` and `--vvp` executables. The command's success denotes native
oracle matches; inspect the separate `reference_matches` fields in `--report`.
The 432 source cases are one CLI CTest, not 432 added CTests. External
population, sv-tests, fuzz and performance evidence is not remeasured here.
