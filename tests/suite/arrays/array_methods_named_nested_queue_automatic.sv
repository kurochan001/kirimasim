/*
:name: array_methods_named_nested_queue_automatic
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
:expected_stdout: @array_methods_named_nested_queue_automatic.expected
*/
module top;
typedef int Row[$];
typedef Row E;
task automatic run();
E a[$];
E r[$];
int total;
a.push_back('{8,1});
a.push_back('{2,5});
a.push_back('{3,0});
a.push_back('{3,0});

r = a.min(x) with (x.sum(y) with (y));
total = r[0].sum(z) with (z);
$display("nested %0d %0d %0d", r.size(), r[0][0], total);
r[0][0] = 99;
r = a.min(x) with (x.sum(y) with (y));
$display("copy %0d", r[0][0]);

endtask
initial run();
endmodule
