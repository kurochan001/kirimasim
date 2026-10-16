/*
:name: assignment_patterns
:description: Assignment patterns, positional, named with default and replicated, fill unpacked arrays from the left bound, nested ones fill arrays of arrays, and pattern values of packed structures and arrays are their concatenation; unpacked arrays are copied element by element, by blocking and nonblocking assignments
:tags: values
:type: simulation
:lrm: 1800-2023 10.9 10.9.1 10.9.2 7.6 7.4.6
:oracle: spec verilator@5.052
:spec_check: ai
:fourstate: no
:nondeterministic: no
:expected_stdout: @assignment_patterns.expected
*/
// The elements of a pattern go to the elements of an unpacked array from
// its left bound, so d[3] of int d [3:0] gets the first one (10.9.1); a
// default fills the elements not named; a replication repeats the list.
// Icarus 13.0 does not accept the default and replicated forms; every
// line matches the output of Verilator 5.052.
module top;
  typedef struct packed { logic [3:0] hi; logic [3:0] lo; } byte_t;
  int a [4] = '{1, 2, 3, 4};
  int d [3:0] = '{10, 20, 30, 40};
  int m [2][3] = '{'{1, 2, 3}, '{4, 5, 6}};
  int z [5] = '{default: 7};
  int s [4] = '{0: 9, 2: 8, default: 1};
  int r [6] = '{3{5, 6}};
  byte_t b = '{4'hA, 4'h5};
  logic [3:0][7:0] pk = '{8'h11, 8'h22, 8'h33, 8'h44};
  int c [4];
  int n [4];
  initial begin
    $display("a %0d %0d %0d %0d", a[0], a[1], a[2], a[3]);
    $display("d %0d %0d %0d %0d", d[3], d[2], d[1], d[0]);
    $display("m %0d %0d %0d %0d %0d %0d", m[0][0], m[0][1], m[0][2], m[1][0], m[1][1], m[1][2]);
    $display("z %0d %0d s %0d %0d %0d %0d r %0d %0d %0d", z[0], z[4], s[0], s[1], s[2], s[3], r[0], r[1], r[5]);
    $display("b %h pk %h", b, pk);
    c = a;
    a = '{4, 3, 2, 1};
    $display("c %0d %0d a %0d %0d", c[0], c[3], a[0], a[3]);
    m[1] = '{7, 8, 9};
    $display("m1 %0d %0d %0d", m[1][0], m[1][1], m[1][2]);
    n <= '{100, 200, 300, 400};
    $display("n before %0d", n[0]);
    #1 $display("n after %0d %0d", n[0], n[3]);
    b = '{hi: 4'h3, lo: 4'hC};
    $display("b %h", b);
  end
endmodule
