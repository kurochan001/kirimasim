// A conditionless for with an empty body still reaches the instruction
// limit (CLI.md 2.6). Code after it is unreachable (SIR_SPEC.md 10).
module top;
  initial begin
    for (;;);
    $display("unreachable");
  end
  final $display("final ran");
endmodule
