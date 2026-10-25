// Arithmetic on values wider than 64 bits: a 256-bit state is mixed with
// additions, a 128-bit multiplication, shifts, xor and comparisons, in one
// loop of one process.
// +n=<iterations> sets how many times the loop runs.
// bench-n: 1500000
// bench-expect: wide_arith n=1500000 c=3c456e9041369e70dd23de7f53da97b515a152693c1092d571a77a8f656bab94
module wide_arith;
  int n;
  logic [255:0] a, b, c;
  logic [127:0] m;

  initial begin
    if (!$value$plusargs("n=%d", n)) n = 1000;
    a = {64'h0123456789abcdef, 64'hfedcba9876543210, 64'h0f1e2d3c4b5a6978, 64'h8796a5b4c3d2e1f0};
    b = ~a;
    c = '0;
    for (int i = 0; i < n; i++) begin
      a = a + b + 256'(i);
      m = a[127:0] * b[191:64];
      b = (b << 7) ^ (b >> 13) ^ {m, m};
      if (a > b) c = c + (a - b);
      else c = c ^ (b - a);
    end
    $display("wide_arith n=%0d c=%h", n, c);
    $finish;
  end
endmodule
