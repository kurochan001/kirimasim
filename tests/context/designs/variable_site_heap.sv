// Copyright (c) 2026 The kirimasim Authors
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
module top;
  class item;
    int value;
    function new(int v); value = v; endfunction
  endclass
  item obj;
  int data[];
  bit [31:0] input_word, result_word, copy_word;
  always @* begin
    result_word = input_word;
    result_word = input_word;
    copy_word = input_word;
  end
  initial begin
    for (int i=1; i<=64; i++) begin
      data = new[i]; obj=new(i); data[i-1] = obj.value;
      input_word=data[i-1];
      #1;
      if (result_word != i || copy_word != i) $fatal;
    end
    $display("final %0d %0d",result_word,copy_word);
  end
endmodule
