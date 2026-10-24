/*
:name: array_methods_snapshot_write_queue_min
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
:expected_stdout: @array_methods_snapshot_write_queue_min.expected
*/
module top;

typedef int E;
E original[$];
int calls, trace;
function int key(input int value);
  calls++;
  trace = trace * 10 + value;
  foreach (original[i]) original[i] = 99;
  return -value;
endfunction

int r[$];
initial begin
original.push_back(8);
original.push_back(3);
original.push_back(6);
original.push_back(3);
r = original.min() with (key(item));
$display("snapshot %0d %0d %0d %0d %0d", calls, trace, r.size(), r[0], original[0]);

end
endmodule
