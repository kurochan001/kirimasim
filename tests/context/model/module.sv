// Copyright (c) 2026 The kirimasim Authors
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
module model;
  logic [64:0] result;
  initial begin
    automatic logic [64:0] pending = 65'h1_0000_0000_0000_0027;
    #3 result <= pending;
  end
endmodule
