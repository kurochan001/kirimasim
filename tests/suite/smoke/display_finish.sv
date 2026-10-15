/*
:name: display_finish
:description: $display and $write of string literals in an initial block, and $finish ending the run before the next statement
:tags: system_tasks display finish
:type: simulation
:lrm: 1800-2023 21.2.1 20.2 9.2.1
:oracle: spec iverilog@13.0
:spec_check: ai
:fourstate: no
:nondeterministic: no
:expected_stdout: @display_finish.expected
*/
// $display ends the line and $write does not (21.2.1); %% prints a single %.
// $finish ends the simulation, so the last $display never runs (20.2).
// Verilator 5.052 (--binary) still prints "never": it ends the run only
// after the process finishes its time step.
module top;
  initial begin
    $display("hello");
    $write("a");
    $display("b 100%%");
    $finish;
    $display("never");
  end
endmodule
