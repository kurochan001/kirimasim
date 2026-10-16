// A task that may wait is called with call.task, which ends the block; the
// caller goes on in the next block (SIR_SPEC.md 7.2, 8). A task that only
// calls a function does not wait, and an always that calls it stays
// ff-plain.
`timescale 1ns/1ns
module top;
  logic clk;
  int n, m;
  function int inc(int a);
    return a + 1;
  endfunction
  task automatic delay_inc(inout int v);
    #2 v = inc(v);
  endtask
  task automatic quick_inc(inout int v);
    v = inc(v);
  endtask
  always @(posedge clk) quick_inc(m);
  initial begin
    n = 1;
    delay_inc(n);
    n = n + 1;
  end
endmodule
