/*
:name: comb_at_packed_identifier_sensitivity
:description: always at-star keeps identifier sensitivity for a constant packed selection
:tags: proc
:type: simulation
:lrm: 1800-2023 9.4.2.2
:oracle: spec
:spec_check: ai
:fourstate: no
:nondeterministic: no
:top_module: top
:expected_stdout: @comb_at_packed_identifier_sensitivity.expected
*/
// Copyright (c) 2026 The kirimasim Authors
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
`timescale 1ns/1ns
module top;
  logic [7:0] vector = 0;
  logic selected;
  int runs = 0;

  task count_run();
    runs++;
  endtask

  always @* begin
    count_run();
    selected = vector[2];
  end

  initial begin
    #1 vector[2] = 1;
    #1 $display("selected bit runs=%0d value=%b", runs, selected);
    vector[0] = 1;
    #1 $display("other bit runs=%0d value=%b", runs, selected);
  end
endmodule
