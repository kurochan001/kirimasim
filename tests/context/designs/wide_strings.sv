// Values wider than 64 bits, strings and reals, ending when nothing is
// left to run.
module wide_strings;
  logic [127:0] w = 128'h0123_4567_89ab_cdef_fedc_ba98_7654_3210;
  string s = "kirima";
  real r = 1.5;
  initial begin
    for (int i = 0; i < 4; i++) begin
      w = {w[0], w[127:1]} ^ (128'd1 << i);
      s = {s, "+"};
      r = r * 2.25;
    end
    $display("%h %s %0d %f", w, s, s.len(), r);
    $display("%s", s.toupper());
  end
endmodule
