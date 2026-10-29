/*
:name: input_same_slot
:description: Time-zero sampling and repeated Active/Observed re-entry retain a single Preponed snapshot
:tags: clocking sampling
:type: simulation
:lrm: 1800-2023 4.4.2.1 14.10 14.13
:oracle: spec
:spec_check: ai
:fourstate: no
:nondeterministic: no
:expected_stdout: @input_same_slot.expected
*/
module top;
  bit clk = 0;
  int d = 42;
  clocking cb @(posedge clk);
    input #1step d;
  endclocking
  initial begin
    @(cb);
    if ($time != 0 || cb.d != 42 || d != 99) $fatal(1, "time-zero sample");
    d = 100; clk = 0; clk = 1;
    @(cb);
    if (cb.d != 42 || d != 100) $fatal(1, "Active re-entry resampled");
    d <= 101; clk <= 0;
    #0 clk <= 1;
    @(cb);
    if (cb.d != 42 || d != 101) $fatal(1, "NBA re-entry resampled");
    #20 d = 200;
    #10 clk = 0; clk <= 1; d <= 300;
    @(cb);
    if (cb.d != 200 || d != 300) $fatal(1, "new slot snapshot");
    $display("PASS");
    $finish;
  end
  initial begin
    d = 99;
    clk = 1;
  end
endmodule
