/*
:name: fork_join_any
:description: fork with join, join_any and join_none; join_none children start only after the parent blocks
:tags: process fork
:type: simulation
:lrm: 1800-2023 9.3.2
:oracle: spec iverilog@13.0 verilator@5.052
:spec_check: ai
:fourstate: no
:nondeterministic: no
:expected_stdout: @fork_join_any.expected
*/
// The default time unit and precision are implementation-specific (3.14.2.3),
// and the test prints nonzero times with %0t, so it sets them.
`timescale 1ns/1ns
// join waits for every child, join_any for the first one to finish, and
// join_none for none. With every kind of join, the children start only
// after the parent blocks or finishes (9.3.2), so "parent first" is printed
// before "join_none child".
module top;
  initial begin
    fork
      #2 $display("[%0t] join child 2", $time);
      #1 $display("[%0t] join child 1", $time);
    join
    $display("[%0t] after join", $time);

    fork
      #30 $display("[%0t] any child 30", $time);
      #10 $display("[%0t] any child 10", $time);
      #20 $display("[%0t] any child 20", $time);
    join_any
    $display("[%0t] after join_any", $time);

    #40;
    fork
      $display("[%0t] join_none child", $time);
    join_none
    $display("[%0t] parent first", $time);
    #1 $display("[%0t] end", $time);
  end
endmodule
