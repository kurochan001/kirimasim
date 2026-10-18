// A design for the command-line tests of $finish and $stop and their
// arguments (SYSTEM_TASKS.md 10.2, SCHEDULER_SPEC.md 16.2): a plusarg picks
// the call, made at 5ns. final runs whichever ends the run, and nothing
// scheduled later runs.
`timescale 1ns/1ns
module top;
  initial begin
    #5;
    if ($test$plusargs("finish0")) $finish(0);
    else if ($test$plusargs("finish1")) $finish(1);
    else if ($test$plusargs("finish2")) $finish(2);
    else if ($test$plusargs("finish")) $finish;
    else if ($test$plusargs("stop0")) $stop(0);
    else if ($test$plusargs("stop1")) $stop(1);
    else if ($test$plusargs("stop2")) $stop(2);
    else if ($test$plusargs("stop")) $stop;
  end
  initial #9 $display("later");
  final $display("final %0t", $time);
endmodule
