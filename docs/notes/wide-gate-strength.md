# Wide three-state gate strength masks

[日本語](wide-gate-strength.ja.md). Implementation `37a2726` and boundary
correction `57a64ef`, verified 2026-10-24.

## Implemented behavior and boundary

`bufif0`, `bufif1`, `notif0` and `notif1` now retain their L/H side masks on
resolved nets wider than 32 bits. This implements the existing
VALUE_SEMANTICS 5.4/5.8 and SIR_SPEC 3.5 contract; it adds no semantic policy,
opcode, dependency or kernel API. The shared kernel's tested interval resolver
still determines the value. The VM only transports its driver values and masks.

The side value has twice the net's width. Its zero-side half starts at the
full net width, including between 64-bit words. A 33–64-bit result can therefore
need a multiword side operand even though the result itself is one word.
The VM now parses masked and unmasked drivers together and extracts each half
at its absolute bit position for each result chunk. The front removes its
32-bit restriction and retains the actual block containing side computations;
short-circuit gate controls can finish in a block other than the entry block.

This does not enable delayed three-state gates on resolved nets. The
[delayed-side amendment](delayed-gate-side-proposal.md) is pending because
same-projection L/H timing needs an explicit contract. Its exploratory path
was removed, and the native source retains the existing unsupported diagnostic.
MOS/switch execution, strength-preserving partial port connections and `%v`
also remain unfinished. Arbitrary resolved strength intervals are not yet
transported through MOS/switch networks by these static-strength side masks.

```systemverilog
module top;
  logic [64:0] data = '0;
  logic control = 'x;
  wire [64:0] q0, q1;
  bufif1 g0[64:0](q0, data, control);
  bufif1 g1[64:0](q1, data, control);
  assign (weak1, weak0) q0 = '0;
  assign (weak1, weak0) q1 = '1;
  initial begin
    #1; $display("%0d %0d", q0 === '0, q1 === 'x);
    data = '1;
    #1; $display("%0d %0d", q0 === 'x, q1 === '1);
  end
endmodule
```

Output: `1 1`, then `1 1`. Keeping L/H as a plain full-strength X would
lose the known 0 or 1 in the corresponding weak-background case.

## Original evidence

[`gen_wide_gate_side_matrix.py`](../../tools/gen_wide_gate_side_matrix.py)
generates **73 original source cases**. Seventy-two span four gate kinds,
widths 33/63/64/65/129/257 and module/generate/interface-owned signals.
They exercise every data/control value, weak 0/1 backgrounds, varying bits
across words and ascending/descending net bounds. Primitive instances stay in
modules; interface cases connect directly to interface-owned signals.
One more case uses short-circuit function calls in gate controls.

[Exact source/expected/output hashes and per-engine results](../measurements/2026-10-24/wide-gate-source-validation.json)
record all 73 passing on the current native binary and on matching installed
Icarus 13.0 stable and 14.0 development binaries. All 73 are unsupported on the
retained old binary, which reports `396131e016eb`. No existing source body or
expected output was changed for this work.

An independent [SIR test](../../tests/unit/wide_strength_sir_test.cpp) checks
eight widths from 1 to 257, both weak background values and both driver orders.
Its period-three/period-five masks distinguish the two halves and distant
words independently of the source matrix's bit patterns. Four deliberate
compiled mutations are caught by actual new stdout expectations: a fixed
zero-half offset, ignored one-side mask, repeated first mask word and wrong
control-flow block. The first three also fail the independent SIR assertions.
Anchor/judging self-checks are excluded as evidence. Sources are restored and
rebuilt after the controls.

The boundary correction checks every control-flow block for delayed writes.
An independent CLI test checks six positioned delayed rejections and six
undelayed acceptances without resolution, including function controls and
widths 1/65/257. This also exercises removal of unused side computations in
their actual blocks. One more compiled mutation restores the entry-block-only
check and fails that real CLI boundary test. The pending timing interpretation
therefore cannot be enabled by a short-circuit control accidentally.

The [current progress](p1-icarus-progress.md) records full local CI and the
versioned finite inventory. The new comparison item measures this mechanism;
it does not prove complete P1 coverage. Performance has not been remeasured.

```sh
python3 tools/gen_wide_gate_side_matrix.py --check
ctest --preset dev -R 'wide_gate_side|StrengthTransport' --output-on-failure
python3 tools/mutants.py --only 'wide gate strength:'
```
