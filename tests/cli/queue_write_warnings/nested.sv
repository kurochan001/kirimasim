// Copyright (c) 2026 The kirimasim Authors
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
// Nested invalid origins stay distinct from detached valid cells (7.10.1, 7.10.3, 13.5).
`timescale 1ns/1ns
module top;
  typedef bit [7:0] word_t;
  typedef word_t queue_t[$];
  queue_t rows[$], arrays[];
  word_t q[$];
  integer index;

  task automatic write_nested(ref queue_t value);
    value[0] = 8'h55; // WARN: write through an invalid outer queue reference
  endtask

  task automatic hold(ref word_t value);
    #1;
    value = 8'h99;
  endtask

  initial begin
    rows.push_back('{8'h11, 8'h22});
    index = 6;
    rows[0][index] = 8'h66; // WARN: valid outer cell, invalid inner queue index
    index = -1;
    rows[index][0] = 8'h77; // WARN: invalid outer queue origin
    write_nested(rows[index]);
    $display("nested %0d %0d %h %h", rows.size(), rows[0].size(), rows[0][0], rows[0][1]);

    arrays = new[1];
    arrays[0] = '{8'ha1};
    index = 3;
    arrays[index][0] = 8'hbb;
    $display("array origin %0d %0d %h", arrays.size(), arrays[0].size(), arrays[0][0]);

    q = '{8'h11, 8'h22};
    fork
      hold(q[0]);
      begin
        #0;
        q.delete(0);
      end
    join
    $display("detached %0d %h", q.size(), q[0]);
  end
endmodule
