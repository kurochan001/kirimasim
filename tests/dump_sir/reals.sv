// Reals: itor, rtoi (rounding, 1.5), rconv between real and shortreal,
// real arithmetic and comparison, and the truth of a real (7.5).
module top;
  real r;
  shortreal s;
  int i;
  initial begin
    r = i + 0.5;
    i = r;
    s = r;
    if (r > 1.0) i = 0;
    if (r) i = 1;
  end
endmodule
