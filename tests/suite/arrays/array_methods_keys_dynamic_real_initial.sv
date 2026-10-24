/*
:name: array_methods_keys_dynamic_real_initial
:description: Array-method references, nonintegral keys and snapshot rules
:tags: arrays
:type: simulation
:lrm: 1800-2023 7.12.1 7.12.2 7.12.3 13.5.2
:oracle: spec
:spec_check: ai
:fourstate: yes
:nondeterministic: no
:generator: gen_array_methods_matrix.py@1
:top_module: top
:expected_stdout: @array_methods_keys_dynamic_real_initial.expected
*/
module top;
typedef real E;
E a[];
E r[$];
initial begin
a = new[4];
a[0] = 2.5;
a[1] = -1.0;
a[2] = 2.5;
a[3] = 0.0;
r = a.min();
$write("min");
$write(" %0d", r.size());
$write(" %.1f", r[0]);
$write("\n");
r = a.max();
$write("max");
$write(" %0d", r.size());
$write(" %.1f", r[0]);
$write("\n");
a = new[4];
a[0] = 2.5;
a[1] = -1.0;
a[2] = 2.5;
a[3] = 0.0;
a.sort();
$write("sort");
$write(" %.1f", a[0]);
$write(" %.1f", a[1]);
$write(" %.1f", a[2]);
$write(" %.1f", a[3]);
$write("\n");
a = new[4];
a[0] = 2.5;
a[1] = -1.0;
a[2] = 2.5;
a[3] = 0.0;
a.rsort();
$write("rsort");
$write(" %.1f", a[0]);
$write(" %.1f", a[1]);
$write(" %.1f", a[2]);
$write(" %.1f", a[3]);
$write("\n");
a = new[4];
a[0] = 2.5;
a[1] = -1.0;
a[2] = 2.5;
a[3] = 0.0;
a.reverse();
$write("reverse");
$write(" %.1f", a[0]);
$write(" %.1f", a[1]);
$write(" %.1f", a[2]);
$write(" %.1f", a[3]);
$write("\n");
end
endmodule
