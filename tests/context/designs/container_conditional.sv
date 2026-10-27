// Copyright (c) 2026 The kirimasim Authors
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception

// VALUE_SEMANTICS 3.9/3.14; RUNTIME_ABI 9.8: conditional values are Context-local.
module top;
  typedef logic [64:0] row_t[$];
  row_t a[$], b[$], result[$];
  logic condition;
  initial begin
    a = '{'{65'd5,65'd6},'{65'd7}};
    b = '{'{65'd5,65'd6},'{65'd9}};
    condition = 'x;
    result = condition ? a : b;
    #1;
    $display("%0d %0d %0d %0d", result.size(), result[0].size(),
             result[0][1] === 65'd6, result[1].size());
    result[0][0] = 0;
    $display("%0d", a[0][0] === 65'd5);
  end
endmodule
