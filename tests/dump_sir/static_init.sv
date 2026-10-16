// Initial values of static variables: one static-init process per instance,
// in declaration order, run before every other process (SIR_SPEC.md 7.1).
// The initial value of q is no other writer of what always_ff writes (6.8).
module top;
  logic clk;
  int a = 5;
  int b = a + 2;
  logic [3:0] q = 4'h3;
  always_ff @(posedge clk) q <= q + 4'h1;
  initial begin
    static int k = 10;
    k = k + b;
  end
endmodule
