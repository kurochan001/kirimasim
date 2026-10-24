/*
:name: array_methods_named_nested_dynamic_initial
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
:expected_stdout: @array_methods_named_nested_dynamic_initial.expected
*/
module top;
typedef int Row[$];
typedef Row E;
E a[];
E r[$];
int total;
initial begin
a = new[4];
a[0] = '{8,1};
a[1] = '{2,5};
a[2] = '{3,0};
a[3] = '{3,0};

r = a.min(x) with (x.sum(y) with (y));
total = r[0].sum(z) with (z);
$display("nested %0d %0d %0d", r.size(), r[0][0], total);
r[0][0] = 99;
r = a.min(x) with (x.sum(y) with (y));
$display("copy %0d", r[0][0]);

end
endmodule
