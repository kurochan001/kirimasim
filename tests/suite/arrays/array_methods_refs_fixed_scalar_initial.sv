/*
:name: array_methods_refs_fixed_scalar_initial
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
:expected_stdout: @array_methods_refs_fixed_scalar_initial.expected
*/
module top;
typedef int E;
task automatic hold(ref int element, ref E receiver[4:1]);
  receiver.sort();
  $display("sort %0d", element);
  element = 99;
  $display("write %0d", receiver[4]);
  receiver.reverse();
  $display("reverse %0d", element);
endtask

E a[4:1];
E r[$];
initial begin
a[4] = 8;
a[3] = 3;
a[2] = 6;
a[1] = 3;
hold(a[4], a);
$display("values %0d %0d", a[4], a[1]);
r = a.min();
r[0] = 77;
$display("copy %0d %0d", a[4], r[0]);

end
endmodule
