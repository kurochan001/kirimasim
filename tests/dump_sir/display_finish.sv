// --dump-sir of the thin path of P1: an initial block with $display, $write
// and $finish. The expected text is display_finish.expected (SIR_SPEC.md 9).
module top;
  initial begin
    $display("hello");
    $write("a");
    $display("b 100%%");
    $finish;
    $display("never");
  end
endmodule
