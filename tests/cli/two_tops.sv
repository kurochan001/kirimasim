// A design for the command-line tests of --top (CLI.md 2.1): two modules
// that nothing instantiates, so without --top both are tops.
`timescale 1ns/1ns
module first_top;
  initial $display("first");
endmodule

module second_top;
  initial #1 $display("second");
endmodule
