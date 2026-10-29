// Copyright (c) 2026 The kirimasim Authors
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
// Invalid queue refs warn on writes, not on reference creation or reads (7.10.1, 13.5).
`timescale 1ns/1ns
module top;
  typedef bit [7:0] word_t;
  word_t q[$], a[];
  integer bad, read_sum;

  task automatic write_element(ref word_t value);
    value = 8'h55; // WARN: one source position reused by distinct invalid actuals
  endtask

  task automatic read_element(const ref word_t value);
    read_sum += value;
  endtask

  task automatic inspect_element(ref word_t value);
    read_sum += value;
  endtask

  task automatic write_array_element(ref word_t value);
    value = 8'hee;
  endtask

  task automatic delayed_write(ref word_t value);
    #1;
    value = 8'h66; // WARN: invalid origin survives a waiting callee
  endtask

  task automatic forward_write(ref word_t value);
    delayed_write(value);
  endtask

  task automatic forward_read(const ref word_t value);
    #1;
    read_element(value);
  endtask

  initial begin
    read_sum = 0;
    q = '{8'h11, 8'h22};
    bad = -1;
    read_element(q[bad]);
    inspect_element(q[bad]);
    write_element(q[bad]);
    bad = 9;
    write_element(q[bad]);
    bad = 0;
    write_element(q[bad]);

    bad = -1;
    forward_read(q[bad]);
    forward_write(q[bad]);
    q[bad] += 8'h01; // WARN: compound write
    q[bad]++; // WARN: increment write
    q[bad][3:0] = 4'hf; // WARN: packed selection through an invalid queue element

    a = new[1];
    a[0] = 8'ha1;
    bad = 8;
    a[bad] = 8'hee;
    write_array_element(a[bad]);
    a[bad][3:0] = 4'hf;
    $display("refs %0d %h %h reads=%0d array=%0d %h",
             q.size(), q[0], q[1], read_sum, a.size(), a[0]);
  end
endmodule
