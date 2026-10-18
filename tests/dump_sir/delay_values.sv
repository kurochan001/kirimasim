// Delays known only when they run (SIR_SPEC.md 7.2): an integral one is
// scaled to ticks; a real one is rounded to the precision and then scaled.
`timescale 1ns/100ps
module top;
  int d = 3;
  real r = 2.75;
  initial begin
    #d;
    #r;
  end
endmodule
