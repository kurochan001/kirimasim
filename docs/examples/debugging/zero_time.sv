// Copyright (c) 2026 The kirimasim Authors
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception

module zero_time;
  timeunit 1ns;
  timeprecision 1ps;

  int iterations = 0;
  initial forever iterations++;
  final $display("final reached");
endmodule
