// Net aliases (SIR_SPEC.md 3.5): the nets of an alias share one slot, and
// their drivers resolve on it.
module top;
  logic p, q;
  wire a, b, c;
  alias a = b = c;
  assign a = p;
  assign c = q;
endmodule
