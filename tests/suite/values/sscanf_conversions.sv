/*
:name: sscanf_conversions
:description: $sscanf reads decimal, binary, octal and hexadecimal numbers with x and z digits, characters, strings and reals, honours widths, suppression and literal characters, returns the count of assigned values or -1 at the end of the input, assigns only that many targets, truncates to the target, reads integral inputs as strings, and writes targets that are elements, members and parts; $value$plusargs without a matching plusarg returns 0 and leaves its target alone
:tags: values
:type: simulation
:lrm: 1800-2023 21.3.4.3 21.6
:oracle: spec iverilog@13.0
:spec_check: ai
:fourstate: yes
:nondeterministic: no
:expected_stdout: @sscanf_conversions.expected
*/
// The other lines match Icarus Verilog 13.0, which keeps the z in the bit
// variable of "z into two-state"; a two-state variable holds 0 there.
module top;
  typedef struct packed { logic [7:0] hi; logic [7:0] lo; } pair_t;
  int a, b, c, n;
  logic [15:0] h;
  bit [7:0] two;
  logic [7:0] bytes [3];
  pair_t p;
  string s, t;
  real r, r2;
  logic [8*5-1:0] packed_in;
  byte ch;
  initial begin
    n = $sscanf("12 -34 +5", "%d %d %d", a, b, c);
    $display("decimal n=%0d %0d %0d %0d", n, a, b, c);
    n = $sscanf("1x0z f", "%h %b", h, two);
    $display("hex %0d %b %b", n, h, two);
    n = $sscanf("1z10", "%b", two);
    $display("z into two-state %0d %b", n, two);
    n = $sscanf("777 1010", "%o %b", a, b);
    $display("octal binary %0d %0d %0d", n, a, b);
    n = $sscanf("12345", "%2d%3d", a, b);
    $display("widths %0d %0d %0d", n, a, b);
    n = $sscanf("id=7, x=9", "id=%*d, x=%d", c);
    $display("suppressed %0d %0d", n, c);
    a = 0; b = 0;
    n = $sscanf("5;6", "%d,%d", a, b);
    $display("stops %0d %0d %0d", n, a, b);
    n = $sscanf("", "%d", a);
    $display("empty %0d", n);
    n = $sscanf("abc", "%d", a);
    $display("no match %0d", n);
    n = $sscanf("hello world", "%s %s", s, t);
    $display("strings %0d %s|%s", n, s, t);
    n = $sscanf("xyz", "%s", h);
    $display("string into bits %0d %h", n, h);
    n = $sscanf(" Q", "%c", ch);
    $display("char %0d %0d", n, ch);
    n = $sscanf("2.5e2 -0.125", "%f %e", r, r2);
    $display("real %0d %0.2f %0.3f", n, r, r2);
    n = $sscanf("70000", "%d", two);
    $display("truncated %0d %0d", n, two);
    packed_in = "41 42";
    n = $sscanf(packed_in, "%d %d", a, b);
    $display("integral input %0d %0d %0d", n, a, b);
    n = $sscanf("1 2 3 4", "%d %d %d %d", bytes[0], bytes[2], p.hi, h[7:0]);
    $display("targets %0d %0d %0d %0d %h", n, bytes[0], bytes[2], p.hi, h);
    c = 99;
    n = $value$plusargs("nothere=%d", c);
    $display("plusarg %0d %0d", n, c);
  end
endmodule
