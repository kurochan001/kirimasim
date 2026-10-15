/*
:name: zero_delay_forever
:description: forever #0 never advances time; haremasim stops at its zero-time limit and still runs final
:tags: scheduling limits
:type: simulation
:lrm: 1800-2023 12.7.6 9.2.3
:oracle: spec
:spec_check: ai
:fourstate: no
:nondeterministic: no
:expected_stdout: @zero_delay_forever.expected
*/
// The standard warns that a forever loop without a timing control that
// advances time can hang the scheduler (12.7.6) but sets no limit. The limit
// and the exit are haremasim rules (SCHEDULER_SPEC.md 16.2): the run stops
// with the reason zero_time_limit, exit code 5, a reason line on stderr, and
// final procedures run. Only stdout is compared for now; checking the exit
// code and the stderr line needs a header field that TEST_SUITE.md adds in P1.
// Icarus Verilog and Verilator loop without end on this test.
module top;
  initial begin
    $display("start");
    forever #0;
  end
  final $display("final ran");
endmodule
