// A design for the command-line tests: a loop that never waits, which the
// --max-steps limit stops (CLI.md 2.6).
module top;
  initial forever begin
  end
  final $display("final ran");
endmodule
