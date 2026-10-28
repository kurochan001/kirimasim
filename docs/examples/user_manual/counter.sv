// Copyright (c) 2026 The kirimasim Authors
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception

module counter #(parameter int WIDTH = 4) (
  input logic clk,
  input logic rst,
  output logic [WIDTH-1:0] q
);
  timeunit 1ns;
  timeprecision 1ps;

  always_ff @(posedge clk)
    if (rst) q <= '0;
    else q <= q + 1;
endmodule
