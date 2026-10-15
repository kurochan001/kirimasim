// A design for the command-line tests: output at 1.5 ns and 3.5 ns, then
// $finish. The precision is 1 ps, so --max-time and the end-reason line
// count in picoseconds (CLI.md 2.5, 4).
`timescale 1ns/1ps
module top;
  initial begin
    #1.5 $display("a");
    #2 $display("b");
    $finish;
  end
endmodule
