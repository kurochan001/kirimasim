/*
:name: specparam_delays
:description: specparams as the delays of a procedural delay control, a continuous assignment, a gate and a net declaration assignment, where only a constant may stand for the last three
:tags: values
:type: simulation
:lrm: 1800-2023 6.20.5 10.3.3 28.16 9.4.1
:oracle: spec iverilog@13.0
:spec_check: ai
:fourstate: yes
:nondeterministic: no
:expected_stdout: @specparam_delays.expected
*/
// A specparam is a constant (6.20.5), so it may be the delay of a
// continuous assignment, a gate and a net declaration assignment, whose
// delays are constants; a real one is rounded to the precision.
`timescale 1ns/100ps
module top;
  logic a = 0;
  wire c1, g1;
  specify
    specparam tcont = 3, tgate = 4, tnet = 5, treal = 1.5;
  endspecify
  assign #tcont c1 = a;
  buf #(tgate) b1 (g1, a);
  wire #tnet n2 = a;
  initial begin
    #(treal) $display("%t real", $realtime);
    a = 1;
    #10 $display("%t c1=%b g1=%b n2=%b", $realtime, c1, g1, n2);
  end
  always @(c1) $display("%t c1 -> %b", $realtime, c1);
  always @(g1) $display("%t g1 -> %b", $realtime, g1);
  always @(n2) $display("%t n2 -> %b", $realtime, n2);
endmodule
