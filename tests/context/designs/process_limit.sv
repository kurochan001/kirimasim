// Copyright (c) 2026 The kirimasim Authors
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception

// CLI 2.7; RUNTIME_ABI 9.8: Contexts may have independent child limits.
`timescale 1ns/1ns
module top;
  int hits = 0;
  initial begin
    fork
      begin #1; ++hits; end
      begin #2; ++hits; end
    join
    $display("done=%0d", hits);
  end
  final $display("final=%0d", hits);
endmodule
