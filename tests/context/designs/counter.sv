// A clock, a register and always_comb, with formatted output at the end.
`timescale 1ns/1ps
module counter;
  logic clk = 0;
  logic [7:0] q = 0;
  logic [7:0] d;
  string name = "counter";
  always #5 clk = ~clk;
  always_comb d = q + 8'd3;
  always_ff @(posedge clk) q <= d;
  initial begin
    repeat (20) @(posedge clk);
    $display("%s q=%0d %h %b at %0t", name, q, q, q[3:0], $time);
    $display("%s", $sformatf("%0d-%0d", q, d));
    $finish;
  end
endmodule
