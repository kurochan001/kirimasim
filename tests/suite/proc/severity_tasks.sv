/*
:name: severity_tasks
:description: $error and $warning write a message with the calling scope and the time; $fatal writes its message and ends the run with exit code 1, running final blocks
:tags: proc severity
:type: simulation
:lrm: 1800-2023 20.10 20.2 9.2.3 9.3.5
:oracle: spec iverilog@13.0
:spec_check: ai
:fourstate: yes
:nondeterministic: no
:expected_stdout: @severity_tasks.expected
:expected_stderr: @severity_tasks.stderr
:expected_exit: 1
*/
// The form of the messages is kirimasim's (SYSTEM_TASKS.md 10.1): the
// scope is the named block, fork or task that calls the task, and the
// time is in the global precision. $info and $fatal print too, but the
// suite compares only the warning and error lines (TEST_SUITE.md).
// Icarus 13.0 prints the same messages, scopes and times in its own form,
// the same standard output, and exits with 1.
`timescale 1ns/1ps
module child;
  task automatic t(input int v);
    $warning("in task %0d", v);
  endtask
endmodule

module top;
  child u ();
  int n = 4;
  initial begin : main
    $info("info %0d", n);
    #2 $error("error %0d %s", n, "text");
    $warning;
    u.t(7);
    fork : branch
      $warning("from fork");
    join
    begin : inner
      $error("x=%b", 2'bx1);
    end
    $display("before fatal");
    #3 $fatal(1, "fatal %0d", n);
    $display("after fatal");
  end
  final $display("final at %0d", $time);
endmodule
