// Copyright (c) 2026 The kirimasim Authors
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
// IEEE 1800-2023 7.10.1: ignored invalid queue writes still evaluate their RHS.
`timescale 1ns/1ns
module top;
  typedef bit [7:0] word_t;
  word_t q[$];
  integer index, calls;
  logic [64:0] wide;

  function automatic word_t rhs();
    calls++;
    return 8'hf0;
  endfunction

  initial begin
    calls = 0;
    q = '{8'h11, 8'h22};
    index = -1;
    repeat (3) begin
      q[index] = rhs(); // WARN: repeated negative index
    end
    index = 4;
    q[index] = rhs(); // WARN: beyond append position
    wide = 'x;
    q[wide] = rhs(); // WARN: unknown index
    wide = 'z;
    q[wide] = rhs(); // WARN: high-impedance index
    wide = 65'b1 << 64;
    q[wide] = rhs(); // WARN: unsigned index wider than 64 bits
    $display("unchanged %0d %h %h rhs=%0d", q.size(), q[0], q[1], calls);

    q[q.size()] = 8'h33;
    q[$+1] = 8'h44;
    $display("append %0d %h %h %h %h", q.size(), q[0], q[1], q[2], q[3]);

    q.delete();
    index = 1;
    q[index] = rhs(); // WARN: invalid index on an empty queue
    q[0] = 8'h55;
    q[$+1] = 8'h66;
    $display("empty %0d %h %h rhs=%0d", q.size(), q[0], q[1], calls);
    index = 0;
    q[index] = 8'h77;
    $display("recovered %0d %h %h", q.size(), q[0], q[1]);
  end
endmodule
