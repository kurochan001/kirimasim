/*
:name: array_methods_keys_fixed_string_automatic
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
:expected_stdout: @array_methods_keys_fixed_string_automatic.expected
*/
module top;
typedef string E;
task automatic run();
E a[4:1];
E r[$];
a[4] = "bb";
a[3] = "a";
a[2] = "ba";
a[1] = "a";
r = a.min();
$write("min");
$write(" %0d", r.size());
$write(" %s", r[0]);
$write("\n");
r = a.max();
$write("max");
$write(" %0d", r.size());
$write(" %s", r[0]);
$write("\n");
a[4] = "bb";
a[3] = "a";
a[2] = "ba";
a[1] = "a";
a.sort();
$write("sort");
$write(" %s", a[4]);
$write(" %s", a[3]);
$write(" %s", a[2]);
$write(" %s", a[1]);
$write("\n");
a[4] = "bb";
a[3] = "a";
a[2] = "ba";
a[1] = "a";
a.rsort();
$write("rsort");
$write(" %s", a[4]);
$write(" %s", a[3]);
$write(" %s", a[2]);
$write(" %s", a[1]);
$write("\n");
a[4] = "bb";
a[3] = "a";
a[2] = "ba";
a[1] = "a";
a.reverse();
$write("reverse");
$write(" %s", a[4]);
$write(" %s", a[3]);
$write(" %s", a[2]);
$write(" %s", a[1]);
$write("\n");
endtask
initial run();
endmodule
