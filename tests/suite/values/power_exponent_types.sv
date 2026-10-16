/*
:name: power_exponent_types
:description: The exponent of ** is self-determined and may have any integral type: signed and unsigned, narrower and wider than the base, two-state and four-state, with the rules of table 11-4 for 0, 1, -1 and negative exponents, and x when the exponent has x or z bits
:tags: values
:type: simulation
:lrm: 1800-2023 11.4.3 11.6.1 11.8.1
:oracle: spec iverilog@13.0
:spec_check: ai
:fourstate: yes
:nondeterministic: no
:expected_stdout: @power_exponent_types.expected
*/
// The result has the type of the base (11.6.1, table 11-21): the exponent
// neither widens nor changes the sign of the result.
module top;
  byte sb;
  byte unsigned ub;
  shortint ss;
  longint sl;
  logic [3:0] e4;
  logic signed [3:0] se4;
  bit [2:0] e3;
  int r;
  logic [7:0] w8;
  logic signed [7:0] s8;
  initial begin
    sb = 3; ub = 3; ss = -2; sl = 40;
    r = 2 ** sb;
    $display("int ** byte 3: %0d", r);
    r = 2 ** ub;
    $display("int ** byte unsigned 3: %0d", r);
    r = 2 ** ss;
    $display("int ** shortint -2: %0d", r);
    r = (-1) ** ss;
    $display("-1 ** shortint -2: %0d", r);
    r = (-1) ** sb;
    $display("-1 ** byte 3: %0d", r);
    r = 1 ** ss;
    $display("1 ** shortint -2: %0d", r);
    r = 3 ** sl;
    $display("int 3 ** longint 40 (wraps): %0d", r);
    e3 = 3'd7;
    w8 = 8'd2 ** e3;
    $display("8-bit 2 ** 3-bit 7: %0d", w8);
    w8 = 8'd2 ** 4'd8;
    $display("8-bit 2 ** 8 (overflow): %0d", w8);
    se4 = -4'sd1;
    s8 = 8'sd2 ** se4;
    $display("signed 2 ** -1: %0d", s8);
    s8 = 8'sd0 ** se4;
    $display("signed 0 ** -1: %b", s8);
    s8 = -8'sd2 ** 4'd3;
    $display("signed -2 ** unsigned 3: %0d", s8);
    se4 = 4'sd0;
    s8 = 8'sd0 ** se4;
    $display("0 ** 0: %0d", s8);
    e4 = 4'b1x00;
    w8 = 8'd2 ** e4;
    $display("8-bit 2 ** x exponent: %b", w8);
    r = 2 ** e4;
    $display("int 2 ** x exponent: %0d", r);
    e4 = 4'bz001;
    w8 = 8'd1 ** e4;
    $display("8-bit 1 ** z exponent: %b", w8);
    // The exponent does not change the width or sign of the result.
    $display("width: %0d %0d", $bits(8'd2 ** sl), $bits(sb ** 32'd1));
    s8 = -8'sd3;
    $display("sign: %0d", (s8 ** 2'd3) < 0);
  end
endmodule
