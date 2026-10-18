// Forms of the text (SIR_SPEC.md 9): a string constant with a tab, a
// backslash and a newline, and a run of elements of a two-dimensional
// array passed as an array argument.
module top;
  logic [3:0] mm [0:1][0:2];
  task automatic t(input logic [3:0] row [0:2]);
    $display("%h", row[0]);
  endtask
  initial begin
    $display("a\tb\\c\n");
    t(mm[1]);
  end
endmodule
