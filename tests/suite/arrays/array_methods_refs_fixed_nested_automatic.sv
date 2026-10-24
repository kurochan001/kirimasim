/*
:name: array_methods_refs_fixed_nested_automatic
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
:expected_stdout: @array_methods_refs_fixed_nested_automatic.expected
*/
module top;
typedef int Row[$];
typedef Row E;
task automatic hold(ref int element, ref E receiver[4:1]);
  receiver.sort() with (item[0]);
  $display("sort %0d", element);
  element = 99;
  $display("write %0d", receiver[4][0]);
  receiver.reverse();
  $display("reverse %0d", element);
endtask

task automatic run();
E a[4:1];
E r[$];
a[4] = '{8};
a[3] = '{3};
a[2] = '{6};
a[1] = '{3};
hold(a[4][0], a);
$display("values %0d %0d", a[4][0], a[1][0]);
r = a.min() with (item[0]);
r[0][0] = 77;
$display("copy %0d %0d", a[4][0], r[0][0]);

endtask
initial run();
endmodule
