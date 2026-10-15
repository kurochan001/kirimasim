/*
:name: empty_forever
:description: An empty forever loop never advances time; haremasim stops at its zero-time limit and still runs final
:tags: scheduling limits
:type: simulation
:lrm: 1800-2023 12.7.6 9.2.3
:oracle: spec
:spec_check: ai
:fourstate: no
:nondeterministic: no
:expected_stdout: @empty_forever.expected
:expected_exit: 5
:expected_stderr: @empty_forever.stderr
*/
// Unlike zero_delay_forever, the loop body has no timing control at all, so
// the process never suspends. haremasim stops it at the zero-time limit
// (SCHEDULER_SPEC.md 16.2: reason zero_time_limit, exit code 5, a reason
// line on stderr, final procedures run). :expected_exit: and :expected_stderr:
// check the code and the line naming the limit (CLI.md 2.6, default --max-steps).
// Icarus Verilog and Verilator loop without end on this test.
module top;
  initial begin
    $display("start");
    forever begin
    end
  end
  final $display("final ran");
endmodule
