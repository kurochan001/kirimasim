// Plusargs, which each run is given (the test passes +mode=fast and
// +count=12).
module plusargs;
  int n, k = 5;
  string s = "none";
  initial begin
    n = $value$plusargs("mode=%s", s);
    $display("%0d %s", n, s);
    n = $value$plusargs("count=%d", k);
    $display("%0d %0d", n, k);
    $display("%0d %0d", $test$plusargs("count"), $test$plusargs("missing"));
  end
endmodule
