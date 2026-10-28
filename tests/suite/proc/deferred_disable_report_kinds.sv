/*
:name: deferred_disable_report_kinds
:description: disabling an outermost scope discards its deferred assertions but preserves pending unique and priority violations in their original order
:tags: proc assertion disable unique priority
:type: simulation
:lrm: 1800-2023 12.4.2.1 12.5.3 16.4.4
:oracle: spec
:spec_check: ai
:fourstate: no
:nondeterministic: no
:issue: 62
:top_module: top
:expected_stdout: @deferred_disable_report_kinds.expected
:expected_stderr: @deferred_disable_report_kinds.stderr
*/
// Copyright (c) 2026 The kirimasim Authors
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
// SCHEDULER_SPEC 15.3/15.4 limits this flush point to deferred assertions.
`timescale 1ns/1ns
module top;
  bit go = 0;
  int selector = 0;

  always @(go) begin : outer
    unique case (selector)
      1: ;
    endcase
    priority if (selector == 1) begin end
    assert #0 (0) else $display("unexpected observed assertion");
    assert final (0) else $display("unexpected final assertion");
  end

  initial begin
    #1 go = 1;
    #0 disable outer;
    #1 $display("done");
  end
endmodule
