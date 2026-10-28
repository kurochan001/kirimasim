/*
:name: program_delayed_net_regions
:description: Zero and positive delayed program drivers settle in their originating set before module waiters
:tags: proc program scheduling nets delay
:type: simulation
:lrm: 1800-2023 4.5 10.3.3 24.3.1 24.3.2
:oracle: spec
:spec_check: ai
:fourstate: yes
:nondeterministic: no
:issue: 63
:top_module: top
:expected_stdout: @program_delayed_net_regions.expected
*/
// Copyright (c) 2026 The kirimasim Authors
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
`timescale 1ns/1ns
program drive(output wire zero_delayed, positive_delayed);
  logic data = 0;
  assign #0 zero_delayed = data;
  assign #2 positive_delayed = data;
  initial begin
    #3 top.marker = 10;
    data = 1;
    #0;
    #0;
    $display("3 program zero=%b delayed=%b", zero_delayed, positive_delayed);
    top.marker = 11;
    #2 top.marker = 20;
    #0;
    #0;
    $display("5 program delayed=%b", positive_delayed);
    top.marker = 21;
    #1 top.marker = 30;
    data = 0;
    #0;
    #0;
    $display("6 program zero=%b delayed=%b", zero_delayed, positive_delayed);
    top.marker = 31;
    #2 top.marker = 40;
    #0;
    #0;
    $display("8 program delayed=%b", positive_delayed);
    top.marker = 41;
    #1;
  end
endprogram

module top;
  wire zero_delayed, positive_delayed;
  int marker = 0;
  drive worker(zero_delayed, positive_delayed);
  always @(zero_delayed)
    if ($time >= 3)
      $display("%0t module zero=%b marker=%0d", $time, zero_delayed, marker);
  always @(positive_delayed)
    if ($time >= 3)
      $display("%0t module delayed=%b marker=%0d", $time, positive_delayed, marker);
endmodule
