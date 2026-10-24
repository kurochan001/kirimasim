/*
:name: array_methods_keys_queue_string_ref
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
:expected_stdout: @array_methods_keys_queue_string_ref.expected
*/
module top;
typedef string E;
E source[$];
task automatic run(ref E a[$]);

E r[$];
a.delete();
a.push_back("bb");
a.push_back("a");
a.push_back("ba");
a.push_back("a");
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
a.delete();
a.push_back("bb");
a.push_back("a");
a.push_back("ba");
a.push_back("a");
a.sort();
$write("sort");
$write(" %s", a[0]);
$write(" %s", a[1]);
$write(" %s", a[2]);
$write(" %s", a[3]);
$write("\n");
a.delete();
a.push_back("bb");
a.push_back("a");
a.push_back("ba");
a.push_back("a");
a.rsort();
$write("rsort");
$write(" %s", a[0]);
$write(" %s", a[1]);
$write(" %s", a[2]);
$write(" %s", a[3]);
$write("\n");
a.delete();
a.push_back("bb");
a.push_back("a");
a.push_back("ba");
a.push_back("a");
a.reverse();
$write("reverse");
$write(" %s", a[0]);
$write(" %s", a[1]);
$write(" %s", a[2]);
$write(" %s", a[3]);
$write("\n");
endtask
initial run(source);
endmodule
