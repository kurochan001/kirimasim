/*
:name: input_blocks_cancel
:description: Independent block events, value changes, atomic publication and canceled event waiters
:tags: clocking sampling events
:type: simulation
:lrm: 1800-2023 9.6 14.10 14.13
:oracle: spec
:spec_check: ai
:fourstate: yes
:nondeterministic: no
:expected_stdout: @input_blocks_cancel.expected
*/
interface pins;
  bit clk = 0;
  int a = 11, b = 22;
  clocking cb @(posedge clk);
    input a, b;
  endclocking
  clocking any_cb @(clk);
    input a;
  endclocking
endinterface
module top;
  pins p();
  int any_count = 0;
  initial begin
    fork
      begin @(p.cb); $fatal(1, "canceled waiter ran"); end
      begin #1; end
    join_any
    disable fork;
    #9 p.a = 33; p.b = 44; p.clk = 1;
    #5 p.clk = 0;
    #5;
    if (any_count != 2 || p.cb.a != 11 || p.cb.b != 22)
      $fatal(1, "unrelated event republished");
    $display("PASS");
    $finish;
  end
  initial begin
    @(p.cb);
    if (p.cb.a != 11 || p.cb.b != 22 || p.a != 33 || p.b != 44)
      $fatal(1, "atomic publication");
  end
  initial begin
    @(p.cb.a);
    if (p.cb.b != 22) $fatal(1, "partial clockvar state became visible");
  end
  initial begin
    @(p.any_cb);
    if (p.any_cb.a != 11) $fatal(1, "first value-change sample");
    any_count++;
    @(p.any_cb);
    if (p.any_cb.a != 33) $fatal(1, "second value-change sample");
    any_count++;
  end
endmodule
