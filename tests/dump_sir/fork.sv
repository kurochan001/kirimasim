// fork: each statement is a child process whose parent is the forking
// process (SIR_SPEC.md 7.5); wait fork and disable fork.
`timescale 1ns/1ns
module top;
  int n;
  initial begin
    fork
      #1 n = 1;
      begin
        #2 n = 2;
      end
    join_any
    wait fork;
    disable fork;
  end
endmodule
