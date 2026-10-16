// Built-in gates (VALUE_SEMANTICS.md 5.8, SIR_SPEC.md 3.5): cont processes;
// a three-state gate on a net that resolves also writes a side slot, read
// by resolve, and on one that does not, writes L and H as x.
module top;
  logic a, b, en;
  wire o, alone, shared;
  nand (o, a, b);
  bufif1 (alone, a, en);
  bufif0 (shared, a, en);
  pullup (shared);
endmodule
