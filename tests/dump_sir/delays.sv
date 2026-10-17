// Delays of continuous assignments, net declaration assignments and gates
// (SIR_SPEC.md 5.4): drive.delay with the rise, fall and turn-off delays.
`timescale 1ns/1ns
module top;
  logic a, b;
  wire y, n, d;
  wire [3:0] w;
  assign #(2, 3, 1) y = a;
  assign #4 w[1:0] = {a, b};
  wire #5 dw = a;
  nand #(1, 2) g (n, a, b);
endmodule
