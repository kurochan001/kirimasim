/*
:name: cond_op_nonintegral
:description: The conditional operator on strings and reals: a known condition picks one side; an x or z condition gives the side both share, or the value of table 7-1 (the empty string, 0.0)
:tags: values
:type: simulation
:lrm: 1800-2023 11.4.11 7.4.5 11.4.5
:oracle: spec iverilog@13.0
:spec_check: ai
:fourstate: yes
:nondeterministic: no
:expected_stdout: @cond_op_nonintegral.expected
*/
// With an unknown condition, both sides are evaluated; if they are equal by
// ==, that value is the result, else the value of table 7-1 for the type
// of the result (11.4.11; VALUE_SEMANTICS.md 3.9). A real and an integer
// make a real, a shortreal and an integer a shortreal. Icarus 13.0 stops
// with an internal error on the strings; its real lines match.
module top;
  logic one = 1'b1, zero = 1'b0, unk = 1'bx, hiz = 1'bz;
  string a = "ab", b = "cd", s;
  real r, p = 1.5, q = 2.5;
  shortreal sr;
  int n = 3;
  initial begin
    s = one ? a : b;
    $display("[%s] %0d", s, s.len());
    s = zero ? a : b;
    $display("[%s] %0d", s, s.len());
    s = unk ? a : b;
    $display("[%s] %0d", s, s.len());
    s = hiz ? a : a;
    $display("[%s] %0d", s, s.len());
    s = unk ? {a, "x"} : {"a", "bx"};
    $display("[%s] %0d", s, s.len());
    r = one ? p : q;
    $display("%f", r);
    r = unk ? p : q;
    $display("%f", r);
    r = unk ? p : 1.5;
    $display("%f", r);
    r = hiz ? 3.0 : n;
    $display("%f", r);
    r = unk ? 2.0 : n;
    $display("%f", r);
    sr = unk ? shortreal'(1.25) : shortreal'(1.25);
    $display("%f", sr);
    sr = unk ? shortreal'(1.25) : n;
    $display("%f", sr);
  end
endmodule
