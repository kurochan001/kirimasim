/*
:name: program_resolved_array_regions
:description: Unpacked program net ports resolve driver changes in Active and Reactive
:tags: proc program scheduling nets arrays
:type: simulation
:lrm: 1800-2023 4.5 24.3.1 24.3.2 28.12
:oracle: spec
:spec_check: ai
:fourstate: yes
:nondeterministic: no
:issue: 63
:top_module: top
:expected_stdout: @program_resolved_array_regions.expected
*/
// Copyright (c) 2026 The kirimasim Authors
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
`timescale 1ns/1ns
program drive(output wire driven[0:1], input wire resolved[0:1]);
  logic data = 1'bz;
  assign driven[0] = data;
  assign driven[1] = data;
  initial begin
    #1 $display("active=%b%b", resolved[0], resolved[1]);
    #1 data = 0;
    #0 $display("driven=%b%b", resolved[0], resolved[1]);
    #1 data = 1'bz;
    #0 $display("released=%b%b", resolved[0], resolved[1]);
    #1 $display("active again=%b%b", resolved[0], resolved[1]);
    data <= 0;
    @(data);
    #0 $display("re-nba=%b%b", resolved[0], resolved[1]);
    #1;
  end
endprogram

module top;
  logic module_data = 0;
  wire resolved[0:1];
  assign (weak1, weak0) resolved[0] = module_data;
  assign (weak1, weak0) resolved[1] = 1;
  drive worker(resolved, resolved);
  initial begin
    #1 module_data = 1;
    #3 module_data = 0;
  end
endmodule
