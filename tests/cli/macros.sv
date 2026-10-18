// A design for the command-line tests of +incdir+ and +define+ (CLI.md
// 2.1): COUNT comes from include/macros_inc.svh, GREETING from the
// command line.
`include "macros_inc.svh"
module top;
  initial begin
`ifdef GREETING
    $display("%s %0d", `GREETING, `COUNT);
`else
    $display("no greeting %0d", `COUNT);
`endif
  end
endmodule
