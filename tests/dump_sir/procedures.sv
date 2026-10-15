// The kinds of procedures (SIR_SPEC.md 7.1) and their triggers: an
// always_comb gets the variables it reads but does not write (9.2.2.2.1),
// an always with only a leading edge control is ff-plain, and any other
// always is a general process that loops. Also a bit write with a constant
// index, an NBA, a delay and a casez.
`timescale 1ns/100ps
module top;
  logic clk;
  logic [3:0] a, b, c;
  logic [7:0] mem [0:3];
  int i;
  always_comb begin
    c = b;
    b = a ^ mem[1][3:0];
  end
  always @(posedge clk) a[2] <= ~a[2];
  always begin
    #2.5 clk = 0;
    @(negedge clk or posedge a[0]);
  end
  initial begin
    casez (a)
      4'b1??0: i = 1;
      default: i = 2;
    endcase
  end
endmodule
