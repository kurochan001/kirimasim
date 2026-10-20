// A design that runs until the time limit the test gives it (50 ns).
`timescale 1ns/1ns
module time_limit;
  logic x = 0;
  int edges = 0;
  always #1 x = ~x;
  always @(posedge x) edges++;
  final $display("edges %0d", edges);
endmodule
