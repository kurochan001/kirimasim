// A design for the command-line tests: a loop of #0, which the
// --max-zero-delay limit stops (CLI.md 2.6).
module top;
  initial forever #0;
  final $display("final ran");
endmodule
