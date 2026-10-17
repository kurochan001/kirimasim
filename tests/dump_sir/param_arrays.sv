// Unpacked array parameters (SIR_SPEC.md 6): an element at a constant
// index is slang's constant; one at a run-time index is read from a slot
// that the static-init of the parameter's scope fills.
module top;
  localparam int P[3] = '{4, 5, 6};
  int k;
  initial $display("%0d %0d", P[1], P[k]);
endmodule
