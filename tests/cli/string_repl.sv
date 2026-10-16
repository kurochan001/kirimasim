// A design for the command-line tests: a string replication with a
// negative count gives the empty string and one warning per run
// (VALUE_SEMANTICS.md 7.3).
module top;
  string s;
  int n;
  initial begin
    n = -1;
    s = {n{"ab"}};
    $display("[%s]", s);
    s = {n{"cd"}};
    $display("[%s]", s);
  end
endmodule
