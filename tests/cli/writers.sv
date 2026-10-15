// A design for the command-line tests: always_comb writes a[1:0] and an
// initial also writes a[0], which 9.2.2.2 forbids (exit code 2). Writing
// the other bits, a[3:2], is allowed.
module top;
  logic [3:0] a, b;
  always_comb a[1:0] = b[1:0];
  initial a[3:2] = 2'b01;
  initial a[0] = 1'b1;
endmodule
