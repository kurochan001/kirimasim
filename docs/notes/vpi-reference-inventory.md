# Original VPI reference inventory

[日本語](vpi-reference-inventory.ja.md)

For current native support, see the [first execution subset](vpi-execution.md).
The evidence below describes the reference and old native build at measurement time.

Measured 2026-11-01: **23/23 reference cases pass** on the explicitly selected
Icarus Verilog 13.0 stable compiler and runtime (`v13_0`). The
[record](../measurements/2026-11-01/vpi-reference-inventory.json) includes
original SV sources, their hashes and expected/actual output, the original C
fixture hash, runner hash, tool hashes and native loader observation.

These are availability probes for the [proposed first VPI contract](vpi-p1-proposal.md),
not native implementation tests or complete IEEE/cocotb coverage. Native VPI
is still unimplemented: the tested binary rejects `--vpi` with input error 2.
Native per-API cases were not executed; `native_evaluated` is zero and
`native_passed` is null. Do not treat this as either 0/23 native test failures
or 23 native successes. Existing P1 inventories and percentages are unchanged.

## Cases and observations

The [original fixture](../../tests/probes/vpi/inventory.c) exercises standard
registration/call argument handling, compiletf, integer and 129-bit sized
functions, hierarchy/iterator discovery, 129-bit X/Z get/put, fixed memory
words, real and packed-string conversions, high/low time rollover, argv,
finish/error queries and variable force/release. Callback cases cover start,
end, delayed and next-time work, read-write/read-only synchronization,
value changes, removal, cancellation and native C reentry.

The read-write case sees the NBA result 1 and writes 2; read-only and the next
HDL observation both see 2. The dedicated reentry case observes `before put`,
`change 1`, `after put`. That ordering is a reference observation used to
propose a specific native contract, not a portable output oracle. Callback
regions already specified by SCHEDULER_SPEC remain authoritative.

Lookup and lifecycle targets are explicitly referenced by the HDL/VPI actuals
to retain them in the reference compilation. An earlier exploratory input
without those references did not expose those objects. Unreferenced/optimized
object visibility remains unconfirmed and is not silently inferred from a
passing retained-object probe. Non-callback API subsection numbers also
remain unconfirmed; the dataset marks this and points to chapter 38.

## Reproduction and checker controls

Set these three variables to the explicit compiler/runtime/VPI-wrapper paths
from the same installed 13.0 toolchain, rather than whichever versions PATH
happens to select. The tool reads only our sources and subprocess outputs;
it never reads simulator implementation or generated simulation bytecode.
The C compiler includes the installed public VPI header; it is not copied
into the repository. The external startup-table declaration follows that ABI.

```sh
python3 tools/vpi_feature_probe.py \
  --iverilog "$VPI_PROBE_IVERILOG" \
  --vvp "$VPI_PROBE_VVP" \
  --iverilog-vpi "$VPI_PROBE_COMPILER" \
  --kirimasim build/dev/src/driver/kirimasim \
  --output build/vpi-reference-inventory.json
```

Six actual compiled fixture controls were all detected by the checker:
wrong function width, discarded X/Z, lost high time half, omitted callback
cancellation, wrong synchronized value and deposit substituted for force.
Their [record](../measurements/2026-11-01/vpi-probe-checker-controls.json)
identifies the failing cases and confirms the fixture bytes were unchanged.
The unchanged positive fixture passes again afterward. `--plugin-source`
allows reproduction with an explicit fault-injected fixture copy; no tracked
source needs modification. These controls validate the reference checker,
**not an unimplemented native VPI host**.

No dependency/tool was installed, no existing source test or expected result
was changed, and no production VPI implementation or new CI count is claimed.
Slang posting remains on hold. Detailed execution review precedes host code,
as VPI_SPEC and the priority decision require.
