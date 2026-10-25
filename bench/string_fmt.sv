// Strings: each iteration formats three numbers with $sformatf, joins the
// result with a part of itself, and hashes every character (FNV-1a).
// +n=<iterations> sets how many times the loop runs.
// bench-n: 500000
// bench-expect: string_fmt n=500000 h=127126b0
module string_fmt;
  int n;

  initial begin
    string s, t;
    int unsigned h;
    if (!$value$plusargs("n=%d", n)) n = 1000;
    h = 32'h811c9dc5;
    for (int i = 0; i < n; i++) begin
      s = $sformatf("%0d:%h:%b", i, i * 7, i[7:0]);
      t = {s, "-", s.substr(0, 1)};
      for (int k = 0; k < t.len(); k++) h = (h ^ t[k]) * 32'h01000193;
    end
    $display("string_fmt n=%0d h=%h", n, h);
    $finish;
  end
endmodule
