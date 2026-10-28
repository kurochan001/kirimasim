/*
:name: program_reactive_drain_order
:description: A program awakened by Re-NBA completes its reactive work before a module awakened by the same update
:tags: proc program scheduling
:type: simulation
:lrm: 1800-2023 4.5 24.3.1
:oracle: spec
:spec_check: ai
:fourstate: no
:nondeterministic: no
:top_module: top
:expected_stdout: @program_reactive_drain_order.expected
*/
// Copyright (c) 2026 The kirimasim Authors
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
`timescale 1ns/1ns
module top;
  int value = 0;
  int marker = 0;
  int module_runs = 0;
  always @(value) begin
    module_runs++;
    $display("module value=%0d marker=%0d", value, marker);
  end
  program worker;
    initial begin
      #1;
      value <= 1;
      #0;
      $display("before Re-NBA value=%0d", value);
      @(value);
      marker = 1;
      $display("program value=%0d", value);
      // Keep the program alive so its completion does not finish the run
      // before the pending Active module process can execute.
      #1;
      $display("module runs=%0d", module_runs);
    end
  endprogram
endmodule
