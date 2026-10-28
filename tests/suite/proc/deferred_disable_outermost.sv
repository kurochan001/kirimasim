/*
:name: deferred_disable_outermost
:description: disabling the outermost scope drops pending observed and final assertions after active and completed activations, while inner scopes, tasks, other processes and confirmed calls keep their reports
:tags: proc assertion disable
:type: simulation
:lrm: 1800-2023 16.4.1 16.4.2 16.4.4
:oracle: spec
:spec_check: ai
:fourstate: no
:nondeterministic: no
:issue: 62
:top_module: top
:expected_stdout: @deferred_disable_outermost.expected
*/
// Copyright (c) 2026 The kirimasim Authors
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
// SCHEDULER_SPEC 15.3/15.4: the queue belongs to the process, and can
// outlive its block activation. A disable of its outermost scope clears
// only unconfirmed deferred calls; a task or inner block is not that scope.
`timescale 1ns/1ns
module top;
  bit inactive_trigger = 0, active_trigger = 0, confirmed_trigger = 0;

  task automatic note(string message);
    $display("%0t %s", $time, message);
  endtask

  always @(inactive_trigger) begin : inactive_outer
    assert #0 (0) else note("unexpected inactive observed");
    assert final (0) else note("unexpected inactive final");
  end

  always @(inactive_trigger) begin
    assert #0 (1) note("other observed retained");
    assert final (1) note("other final retained");
  end

  always @(active_trigger) begin : active_outer
    assert #0 (0) else note("unexpected active observed");
    assert final (0) else note("unexpected active final");
    #100;
  end

  initial begin : self_outer
    #5;
    assert #0 (0) else note("unexpected self observed");
    assert final (0) else note("unexpected self final");
    disable self_outer;
    note("unexpected self continuation");
  end

  initial #7 begin : leading_outer
    assert #0 (0) else note("unexpected leading observed");
    assert final (0) else note("unexpected leading final");
    #100;
  end

  initial begin : completed_outer
    #9;
    assert #0 (0) else note("unexpected completed observed");
    assert final (0) else note("unexpected completed final");
  end

  initial begin
    #11;
    begin : inner
      assert #0 (0) else note("inner observed retained");
      assert final (0) else note("inner final retained");
      disable inner;
      note("unexpected inner continuation");
    end
  end

  task automatic task_disable();
    assert #0 (0) else note("task observed retained");
    assert final (0) else note("task final retained");
    disable task_disable;
  endtask

  initial begin
    #13 task_disable();
  end

  task automatic confirmed_first();
    disable confirmed_outer;
    note("confirmed first retained");
  endtask

  always @(confirmed_trigger) begin : confirmed_outer
    assert #0 (1) confirmed_first();
    assert #0 (1) note("confirmed second retained");
    assert final (0) else note("unexpected unconfirmed final");
  end

  initial begin
    #17;
    fork
      begin : fork_outer
        assert #0 (0) else note("unexpected fork observed");
        assert final (0) else note("unexpected fork final");
      end
    join
    #0 disable fork_outer;
  end

  initial begin
    #1 inactive_trigger = 1;
    #0 disable inactive_outer;
    #2 active_trigger = 1;
    #0 disable active_outer;
    #4;
    #0 disable leading_outer;
    #2;
    #0 disable completed_outer;
    #6 confirmed_trigger = 1;
    #3 note("done");
  end
endmodule
