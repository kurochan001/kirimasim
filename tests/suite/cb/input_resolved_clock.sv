/*
:name: input_resolved_clock
:description: A resolved strength-net clock publishes sampled inputs without ordinary clock waiters
:tags: clocking nets strength
:type: simulation
:lrm: 1800-2023 14.10 14.13 28.12
:oracle: spec
:spec_check: ai
:fourstate: yes
:nondeterministic: no
:expected_stdout: @input_resolved_clock.expected
*/
module top;
  logic drive = 0;
  wire clk;
  int d = 3;
  assign (weak1, weak0) clk = drive;
  assign (weak1, weak0) clk = 1'bz;
  clocking cb @(posedge clk);
    input d;
  endclocking
  initial begin
    #10 drive = 1; d = 7;
    #10 drive = 0;
    #10 drive = 1; d = 9;
  end
  initial begin
    @(cb);
    if (cb.d != 3 || d != 7) $fatal(1, "resolved first edge");
    @(cb);
    if (cb.d != 7 || d != 9) $fatal(1, "resolved second edge");
    $display("PASS");
    $finish;
  end
endmodule
