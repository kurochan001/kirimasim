/*
:name: program_registered_strength_regions
:description: Program driver updates settle a registered MOS component before Re-Inactive readers
:tags: proc program scheduling nets strength
:type: simulation
:lrm: 1800-2023 4.5 24.3.1 24.3.2 28.12
:oracle: spec
:spec_check: ai
:fourstate: yes
:nondeterministic: no
:issue: 63
:top_module: top
:expected_stdout: @program_registered_strength_regions.expected
*/
// Copyright (c) 2026 The kirimasim Authors
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
`timescale 1ns/1ns
program drive(output wire driven, input wire resolved);
  logic data = 1'bz;
  assign driven = data;
  initial begin
    #1 $display("initial=%b", resolved);
    #1 top.marker = 10;
    data = 0;
    #0 $display("driven=%b", resolved);
    top.marker = 11;
    #1 top.marker = 20;
    data = 1'bz;
    #0 $display("released=%b", resolved);
    top.marker = 21;
    #1 top.marker = 30;
    data <= 0;
    @(data);
    #0 $display("re-nba=%b", resolved);
    top.marker = 31;
    #1 top.marker = 40;
    data = 1'bz;
    #0 $display("re-nba release=%b", resolved);
    top.marker = 41;
    #1 $display("active=%b", resolved);
    #1;
  end
endprogram

module top;
  wire resolved;
  logic source = 1;
  int marker = 0;
  nmos device(resolved, source, 1'b1);
  drive worker(resolved, resolved);
  initial #6 source = 0;
  always @(resolved)
    if ($time >= 2)
      $display("%0t module=%b marker=%0d", $time, resolved, marker);
endmodule
