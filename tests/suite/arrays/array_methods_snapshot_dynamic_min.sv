/*
:name: array_methods_snapshot_dynamic_min
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
:expected_stdout: @array_methods_snapshot_dynamic_min.expected
*/
module top;
typedef int E;
E original[];
int calls, trace, receivers;
function int key(input int value);
  calls++;
  trace = trace * 10 + value;
  original.delete();
  return -value;
endfunction
typedef int Array[];
function Array provide(); receivers++; return original; endfunction

int r[$];
initial begin
original = new[4];
original[0] = 8;
original[1] = 3;
original[2] = 6;
original[3] = 3;
r = original.min() with (key(item));
$display("snapshot %0d %0d %0d %0d", calls, trace, r.size(), r[0]);
calls = 0; r = original.min() with (key(item));
$display("empty %0d %0d", calls, r.size());
original = '{5, 2};
r = provide().min() with (key(item));
$display("receiver %0d %0d %0d", receivers, calls, r[0]);

end
endmodule
