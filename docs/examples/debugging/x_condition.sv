// Copyright (c) 2026 The kirimasim Authors
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception

module x_condition;
  timeunit 1ns;
  timeprecision 1ps;

  logic ready;
  initial begin
    $display("ready=%b", ready);
    if (ready)
      $display("branch=true");
    else
      $display("branch=false");
    #1;
    $finish(0);
  end
endmodule
