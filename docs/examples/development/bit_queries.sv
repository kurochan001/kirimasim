// Copyright (c) 2026 The kirimasim Authors
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception

module bit_queries;
  timeunit 1ns;
  timeprecision 1ps;

  logic [7:0] value;
  logic [64:0] wide;
  int ones;
  int unknowns;
  int wide_ones;

  initial begin
    value = 8'b10xz_1100;
    wide = '0;
    wide[64] = 1'b1;
    wide[0] = 1'b1;
    #1;
    ones = $countones(value);
    unknowns = $countbits(value, 1'bx, 1'bz);
    wide_ones = $countones(wide);
    if (ones !== 3 || unknowns !== 2 || wide_ones !== 2)
      $fatal(1, "bit-query self-check failed");
    $display("ones=%0d unknown=%0d wide=%0d", ones, unknowns, wide_ones);
    $finish(0);
  end
endmodule
