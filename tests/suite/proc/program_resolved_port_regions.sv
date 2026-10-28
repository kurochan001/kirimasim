/*
:name: program_resolved_port_regions
:description: A shared resolved net settles in the thread set of each changing driver before the next region of that set
:tags: proc program scheduling nets
:type: simulation
:lrm: 1800-2023 4.5 24.3.1 24.3.2 28.12
:oracle: spec
:spec_check: ai
:fourstate: yes
:nondeterministic: no
:top_module: top
:expected_stdout: @program_resolved_port_regions.expected
*/
// Copyright (c) 2026 The kirimasim Authors
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
`timescale 1ns/1ns
program driver(output wire driven, input wire resolved);
  logic data = 1'bz;
  assign driven = data;
  initial begin
    #5;
    $display("5 active=%b", resolved);
    #5;
    top.marker = 1;
    data = 0;
    #0;
    $display("10 reactive=%b", resolved);
    #1;
    top.marker = 2;
    data = 1'bz;
    #0;
    $display("11 release=%b", resolved);
    #1;
    $display("12 active=%b", resolved);
    #1;
  end
endprogram

module top;
  logic module_data = 0;
  int marker = 0;
  wire resolved;
  assign (weak1, weak0) resolved = module_data;
  driver worker(.driven(resolved), .resolved(resolved));
  initial begin
    #5 module_data = 1;
    #7 module_data = 0;
  end
  always @(resolved) begin
    if ($time >= 10)
      $display("%0t module=%b marker=%0d", $time, resolved, marker);
  end
endmodule
