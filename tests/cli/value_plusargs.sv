// $value$plusargs (SYSTEM_TASKS.md 9, CLI.md 2.4): the first plusarg that
// starts with the name, its rest read by the conversion, x when it cannot
// be read whole; 0 and no change when none matches.
module top;
  int n, k = 5;
  string s = "none";
  logic [7:0] h = 8'h11;
  initial begin
    n = $value$plusargs("mode=%s", s);
    $display("%0d %s", n, s);
    n = $value$plusargs("count=%d", k);
    $display("%0d %0d", n, k);
    n = $value$plusargs("mask=%h", h);
    $display("%0d %h", n, h);
  end
endmodule
