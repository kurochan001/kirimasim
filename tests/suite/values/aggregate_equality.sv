/*
:name: aggregate_equality
:description: ==, !=, === and !== of unpacked arrays and structures compare their elements and members pairwise: == is 0 if a pair differs, else x if a pair is unknown, else 1; === is 1 only if every pair is identical; nested structures, array members, strings and reals, rows of arrays, patterns and arrays with opposite ranges
:tags: values
:type: simulation
:lrm: 1800-2023 11.2.2 7.4.6 11.4.5 6.22.2
:oracle: spec verilator@5.052
:spec_check: ai
:fourstate: yes
:nondeterministic: no
:expected_stdout: @aggregate_equality.expected
*/
// The rule for x is VALUE_SEMANTICS.md 3.14. Icarus Verilog 13.0 accepts
// no unpacked structures. The lines without x or z match Verilator 5.052,
// checked without the z line, whose assignment it does not accept.
module top;
  typedef struct { logic [3:0] a; int b; } pair_t;
  typedef struct { pair_t p; byte v [2]; string s; real r; } deep_t;
  pair_t x, y;
  deep_t d1, d2;
  logic [3:0] u [0:2], w [2:0];
  int m [2][2];
  initial begin
    x = '{4'b1010, 5};
    y = '{4'b1010, 5};
    $display("same: %b %b %b %b", x == y, x != y, x === y, x !== y);
    y.b = 6;
    $display("differ: %b %b %b %b", x == y, x != y, x === y, x !== y);
    y = '{4'b1x10, 5};
    $display("x only: %b %b %b %b", x == y, x != y, x === y, x !== y);
    y.b = 6;
    $display("x and differ: %b %b %b", x == y, x != y, x === y);
    x.a = 4'b1x10;
    y.b = 5;
    $display("x on both: %b %b", x == y, x === y);
    $display("pattern: %b %b", x === pair_t'{4'b1x10, 5}, y == pair_t'{4'b1010, 5});
    d1 = '{'{4'd3, 7}, '{8'd1, 8'd2}, "abc", 2.5};
    d2 = d1;
    $display("deep same: %b %b", d1 == d2, d1 !== d2);
    d2.v[1] = 8'd9;
    $display("array member: %b", d1 == d2);
    d2 = d1;
    d2.s = "abd";
    $display("string member: %b", d1 == d2);
    d2 = d1;
    d2.r = 2.75;
    $display("real member: %b", d1 != d2);
    d2 = d1;
    d2.p.b = 8;
    $display("nested member: %b", d1 == d2);
    u = '{4'd1, 4'd2, 4'd3};
    w = '{4'd1, 4'd2, 4'd3};
    $display("opposite ranges: %b", u == w);
    w[0] = 4'bz;
    $display("z element: %b %b", u == w, u !== w);
    m = '{'{1, 2}, '{1, 2}};
    $display("rows: %b", m[0] == m[1]);
    m[1][1] = 3;
    $display("rows differ: %b", m[0] != m[1]);
  end
endmodule
