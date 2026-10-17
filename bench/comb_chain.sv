// A chain of always_comb stages of 32-bit arithmetic, shifts and
// comparisons, fed by a xorshift generator that changes its inputs once per
// time step. The initial process folds the last stage into a sum.
// +n=<steps> sets the number of time steps.
// bench-n: 3000000
// bench-expect: comb_chain n=3000000 sum=d3369c10
module comb_chain;
  logic [31:0] a, b;
  logic [31:0] s1, s2, s3, s4, s5, s6, s7, s8;
  logic [31:0] sum;
  int n;

  always_comb s1 = a + b;
  always_comb s2 = (s1 ^ (a >> 3)) - b;
  always_comb s3 = (s2 > a) ? s2 - a : a - s2;
  always_comb s4 = s3 * 32'd40503;
  always_comb s5 = {s4[15:0], s4[31:16]} + 32'(s3 == b);
  always_comb s6 = (s5 & ~s1) | (s2 << 1);
  always_comb s7 = (s6 < s4) ? s6 + 32'd1 : s6 - 32'd1;
  always_comb s8 = s7 ^ (s7 >> 7);

  initial begin
    if (!$value$plusargs("n=%d", n)) n = 1000;
    a = 32'd1;
    b = 32'h1234_5678;
    sum = '0;
    for (int i = 0; i < n; i++) begin
      #1;
      sum = sum + s8;
      a = a ^ (a << 13);
      a = a ^ (a >> 17);
      a = a ^ (a << 5);
      b = b + 32'h9e37_79b9;
    end
    #1;
    sum = sum + s8;
    $display("comb_chain n=%0d sum=%h", n, sum);
    $finish;
  end
endmodule
