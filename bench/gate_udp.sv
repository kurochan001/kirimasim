// Gate primitives and a combinational UDP: 32 xor gates and 32 instances of a
// majority UDP evaluate again after each change of their 32 inputs.
// +n=<input changes> sets how many times the inputs change.
// bench-n: 400000
// bench-expect: gate_udp n=400000 acc=8bcfd518
primitive maj3 (out, a, b, c);
  output out;
  input a, b, c;
  table
    1 1 ? : 1;
    1 ? 1 : 1;
    ? 1 1 : 1;
    0 0 ? : 0;
    0 ? 0 : 0;
    ? 0 0 : 0;
  endtable
endprimitive

module gate_udp;
  int n;
  logic [31:0] x;
  wire [31:0] y, z;

  for (genvar g = 0; g < 32; g++) begin : cells
    xor (y[g], x[g], x[(g + 1) % 32]);
    maj3 (z[g], x[g], y[g], x[(g + 5) % 32]);
  end

  initial begin
    int unsigned acc;
    if (!$value$plusargs("n=%d", n)) n = 1000;
    x = 32'h1;
    acc = 0;
    for (int i = 0; i < n; i++) begin
      x = x * 32'd1103515245 + 32'd12345;
      #1;
      acc = (acc ^ z) + 32'(i);
    end
    $display("gate_udp n=%0d acc=%h", n, acc);
    $finish;
  end
endmodule
