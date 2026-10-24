/*
:name: array_methods_snapshot_write_dynamic_max
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
:expected_stdout: @array_methods_snapshot_write_dynamic_max.expected
*/
module top;

typedef int E;
E original[];
int calls, trace;
function int key(input int value);
  calls++;
  trace = trace * 10 + value;
  foreach (original[i]) original[i] = 99;
  return -value;
endfunction

int r[$];
initial begin
original = new[4];
original[0] = 8;
original[1] = 3;
original[2] = 6;
original[3] = 3;
r = original.max() with (key(item));
$display("snapshot %0d %0d %0d %0d %0d", calls, trace, r.size(), r[0], original[0]);

end
endmodule
