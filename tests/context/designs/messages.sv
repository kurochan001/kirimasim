// Severity tasks, a failing deferred assertion and $fatal: the messages
// and the end with the reason fatal.
`timescale 1ns/1ns
module messages;
  int n = 0;
  initial begin
    $info("info %0d", n);
    #2 $warning("warning %0d", n + 1);
    #3 $error("error %0d", n + 2);
    #1 n = 5;
    assert #0 (n == 4) else $error("n is %0d", n);
    #1 $fatal(1, "fatal %0d", n + 3);
  end
endmodule
