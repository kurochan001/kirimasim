/*
:name: array_methods_keys_bounded_real_automatic
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
:expected_stdout: @array_methods_keys_bounded_real_automatic.expected
*/
module top;
typedef real E;
task automatic run();
E a[$:5];
E r[$];
a.delete();
a.push_back(2.5);
a.push_back(-1.0);
a.push_back(2.5);
a.push_back(0.0);
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
a.delete();
a.push_back(2.5);
a.push_back(-1.0);
a.push_back(2.5);
a.push_back(0.0);
a.sort();
$write("sort");
$write(" %.1f", a[0]);
$write(" %.1f", a[1]);
$write(" %.1f", a[2]);
$write(" %.1f", a[3]);
$write("\n");
a.delete();
a.push_back(2.5);
a.push_back(-1.0);
a.push_back(2.5);
a.push_back(0.0);
a.rsort();
$write("rsort");
$write(" %.1f", a[0]);
$write(" %.1f", a[1]);
$write(" %.1f", a[2]);
$write(" %.1f", a[3]);
$write("\n");
a.delete();
a.push_back(2.5);
a.push_back(-1.0);
a.push_back(2.5);
a.push_back(0.0);
a.reverse();
$write("reverse");
$write(" %.1f", a[0]);
$write(" %.1f", a[1]);
$write(" %.1f", a[2]);
$write(" %.1f", a[3]);
$write("\n");
endtask
initial run();
endmodule
