/*
:name: unique_case_violation
:description: A unique case violation seen only during a zero-delay glitch is not reported; a violation that remains at the end of the time step is
:tags: case unique deferred
:type: simulation
:lrm: 1800-2023 12.5.3 12.5.3.1 12.4.2 12.4.2.1
:oracle: spec
:fourstate: no
:nondeterministic: no
:expected_stdout: @unique_case_violation.expected
*/
// The default time unit and precision are implementation-specific (3.14.2.3),
// and the test prints nonzero times with %0t, so it sets them.
`timescale 1ns/1ns
// At time 1, sel is 2'b10 (no item matches: a violation) and then, after #0,
// 2'b01. The second run of the always_comb is a flush point, so the pending
// violation is discarded and nothing is reported (12.4.2.1). At time 2, sel
// stays 2'b11, so the violation matures in the Observed region and is
// reported once. With no matching item no statement runs, so hit keeps 1.
// Violation reports go to stderr (SYSTEM_TASKS.md 2), so only stdout is
// compared for now: checking that exactly one report, for time 2, appears
// needs a header field that TEST_SUITE.md adds in P1.
// Icarus Verilog 13.0 and Verilator 5.052 (--assert) both report the glitch
// at time 1, which 12.4.2.1 rules out.
module top;
  logic [1:0] sel;
  int hit;

  always_comb begin
    unique case (sel)
      2'b00: hit = 0;
      2'b01: hit = 1;
    endcase
  end

  initial begin
    sel = 2'b00;
    #1 sel = 2'b10;
    #0 sel = 2'b01;
    #0 $display("[%0t] hit=%0d", $time, hit);
    #1 sel = 2'b11;
    #0 $display("[%0t] hit=%0d", $time, hit);
  end
endmodule
