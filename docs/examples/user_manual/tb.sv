// Copyright (c) 2026 The kirimasim Authors
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception

`include "settings.svh"

module tb;
  timeunit 1ns;
  timeprecision 1ps;

  localparam int Width = `COUNTER_WIDTH;
  logic clk = 0;
  logic rst = 1;
  logic [Width-1:0] q;
  int cycles;

  counter #(.WIDTH(Width)) dut(.clk, .rst, .q);
  always #5 clk = ~clk;

  initial begin
    $timeformat(-9, 0, " ns", 0);
    if (!$value$plusargs("cycles=%d", cycles)) cycles = 4;
    if (cycles < 1) $fatal(1, "cycles must be positive");
    $display("width=%0d cycles=%0d", Width, cycles);
    #12 rst = 0;
    repeat (cycles) begin
      @(negedge clk);
      $display("t=%0t q=%0d", $time, q);
    end
    $finish;
  end
endmodule
