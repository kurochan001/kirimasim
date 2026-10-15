// A design for the command-line tests: final procedures run once each when
// the simulation ends, whatever the reason, and $finish inside one skips
// the rest (SCHEDULER_SPEC.md 6.2, 16.2).
module top;
  int n;
  initial #1 begin
    $display("a");
    n = 5;
  end
  final $display("final 1 n=%0d", n);
  final begin
    $display("final 2");
    $finish;
  end
  final $display("final 3");
endmodule
