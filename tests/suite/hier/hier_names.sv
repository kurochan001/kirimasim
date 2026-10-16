/*
:name: hier_names
:description: A parent writes and reads a child's variable by a hierarchical name, and the child reads the parent's by an upward name
:tags: hierarchy names
:type: simulation
:lrm: 1800-2023 23.6 23.8
:oracle: spec iverilog@13.0
:spec_check: ai
:fourstate: no
:nondeterministic: no
:expected_stdout: @hier_names.expected
*/
// The writes from top wake the child's event control on cnt. The child's
// final read happens at time 4, after top has finished at time 3, so the
// order of the lines does not depend on the order of the processes.
module counter;
  int cnt;
  always @(cnt) $display("child sees %0d", cnt);
  initial #4 $display("child reads top.flag=%0d", top.flag);
endmodule

module top;
  int flag = 7;
  counter u ();
  initial begin
    #1 u.cnt = 5;
    #1 u.cnt = u.cnt + 1;
    #1 $display("top reads %0d", u.cnt);
  end
endmodule
