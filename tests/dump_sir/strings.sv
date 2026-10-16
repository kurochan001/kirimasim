// Strings (SIR_SPEC.md 3.2.1, 5.2): a string literal becomes a string
// constant; concatenation, comparison and s[i] work on string values; a
// method that changes the string returns the new one, which is stored.
module top;
  string s, t;
  byte b;
  initial begin
    s = "ab";
    t = {s, "c"};
    if (t > s) b = t[2];
    t.putc(0, "x");
    s = $sformatf("%0d", t.len());
  end
endmodule
