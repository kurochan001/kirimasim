// A design for the command-line tests: two lines of output, then $finish.
module top;
  initial begin
    $display("hello");
    $write("a");
    $finish;
  end
endmodule
