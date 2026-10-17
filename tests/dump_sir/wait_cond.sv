// wait (expr) (SIR_SPEC.md 7.2): the condition is a guard that reads the
// frame of the process or task that waits; wait.cond goes on to its block.
module top;
  logic [3:0] a;
  task automatic t(int k);
    wait (a == k[3:0]);
  endtask
  initial begin
    wait (a[1]) $display("a[1]");
    t(3);
  end
endmodule
