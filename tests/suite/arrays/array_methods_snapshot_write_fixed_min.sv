/*
:name: array_methods_snapshot_write_fixed_min
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
:expected_stdout: @array_methods_snapshot_write_fixed_min.expected
*/
module top;

typedef int E;
E original[4:1];
int calls, trace;
function int key(input int value);
  calls++;
  trace = trace * 10 + value;
  foreach (original[i]) original[i] = 99;
  return -value;
endfunction

int r[$];
initial begin
original[4] = 8;
original[3] = 3;
original[2] = 6;
original[1] = 3;
r = original.min() with (key(item));
$display("snapshot %0d %0d %0d %0d %0d", calls, trace, r.size(), r[0], original[4]);

end
endmodule
